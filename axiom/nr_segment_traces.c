#include "nr_axiom.h"

#include "nr_segment.h"
#include "nr_segment_traces.h"
#include "nr_txn.h"
#include "util_minmax_heap.h"
#include "util_strings.h"

static void add_hash_json_to_buffer(nrbuf_t* buf, const nrobj_t* hash) {
  char* json;

  if (NULL == hash) {
    nr_buffer_add(buf, "{}", 2);
    return;
  }

  json = nro_to_json(hash);
  nr_buffer_add(buf, json, nr_strlen(json));
  nr_free(json);
}

/*
 * Purpose : Adds the given data hash to the JSON buffer, including the
 * execution context string ID.
 */
static void add_async_hash_json_to_buffer(nrbuf_t* buf,
                                          const nrobj_t* hash,
                                          int async_context_value_idx) {
  /*
   * A previous version of this function also used the string table for
   * "async_context", but it turns out that RPM doesn't interpolate keys.
   */
  nr_buffer_add(buf, NR_PSTR("{\"async_context\":\"`"));
  nr_buffer_write_uint64_t_as_text(buf, (uint64_t)async_context_value_idx);
  nr_buffer_add(buf, NR_PSTR("\""));

  if (NULL != hash) {
    char* json = nro_to_json(hash);
    int json_len = nr_strlen(json);

    /*
     * An empty hash will be a two character string: "{}". If it's longer than
     * that, then there's data, which we should add to the buffer without the
     * surrounding braces.
     */
    if (json_len > 2) {
      nr_buffer_add(buf, ",", 1);
      nr_buffer_add(buf, json + 1, nr_strlen(json) - 2);
    }

    nr_free(json);
  }

  nr_buffer_add(buf, "}", 1);
}

void nr_segment_traces_stot_iterator_callback(nr_segment_t* segment,
                                              void* userdata) {
  uint64_t zerobased_start_ms;
  uint64_t zerobased_stop_ms;
  const char* segment_name;
  int idx;

  const nrtxn_t* txn = ((nr_segment_userdata_t*)userdata)->txn;
  nrbuf_t* buf = ((nr_segment_userdata_t*)userdata)->buf;
  nrpool_t* segment_names = ((nr_segment_userdata_t*)userdata)->segment_names;

  if (segment->start_time >= segment->stop_time) {
    ((nr_segment_userdata_t*)userdata)->success = -1;
    return;
  }

  zerobased_start_ms = (segment->start_time - txn->segment_root->start_time)
                       / NR_TIME_DIVISOR_MS;

  zerobased_stop_ms = (segment->stop_time - txn->segment_root->start_time)
                      / NR_TIME_DIVISOR_MS;

  if (txn->segment_root->start_time > segment->start_time) {
    zerobased_start_ms = 0;
  }

  if (txn->segment_root->start_time > segment->stop_time) {
    zerobased_stop_ms = 0;
  }

  if (zerobased_start_ms > zerobased_stop_ms) {
    zerobased_stop_ms = zerobased_start_ms;
  }

  segment_name = nr_string_get(txn->trace_strings, segment->name);
  idx = nr_string_add(segment_names, segment_name ? segment_name : "<unknown>");
  /* The internal string tables index at 1, and we wish to index by 0 here. */
  idx--;

  nr_buffer_add(buf, "[", 1);
  nr_buffer_write_uint64_t_as_text(buf, zerobased_start_ms);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_write_uint64_t_as_text(buf, zerobased_stop_ms);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "\"", 1);
  nr_buffer_add(buf, "`", 1);
  nr_buffer_write_uint64_t_as_text(buf, (uint64_t)idx);
  nr_buffer_add(buf, "\"", 1);
  nr_buffer_add(buf, ",", 1);

  /*
   * Segment parameters.
   *
   * We only want to add the async context if the transaction itself is
   * asynchronous: ie if the WebTransactionTotalTime metric > WebTransaction.
   * The reason for this is that APM displays the transaction trace differently
   * if it has an async context; the external is moved out of the place where
   * it was called, which is confusing for the common single threaded case (but
   * makes sense if there are parallel requests, since you see them together,
   * rather than nested under their individual start points).
   */
  if (segment->async_context && txn->async_duration) {
    int async_context_idx;
    const char* async_context;

    async_context = nr_string_get(txn->trace_strings, segment->async_context);
    async_context_idx = nr_string_add(
        segment_names, async_context ? async_context : "<unknown>");
    /* The internal string tables index at 1, and we wish to index by 0 here. */
    async_context_idx--;

    add_async_hash_json_to_buffer(buf, segment->user_attributes,
                                  async_context_idx);
  } else {
    add_hash_json_to_buffer(buf, segment->user_attributes);
  }

  // And now for all its children.
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "[", 1);
}

int nr_segment_traces_json_print_segments(nrbuf_t* buf,
                                          const nrtxn_t* txn,
                                          nr_segment_t* root,
                                          nrpool_t* segment_names) {
  if ((NULL == buf) || (NULL == txn) || (NULL == segment_names) || (NULL == root)) {
    return -1;
  } else {
    /* Construct the userdata to be supplied to the callback */
    nr_segment_userdata_t userdata = {
        .buf = buf, .txn = txn, .segment_names = segment_names, .success = 0};

    nr_segment_iterate(
        root, (nr_segment_iter_t)nr_segment_traces_stot_iterator_callback,
        &userdata);

    return userdata.success;
  }
  return -1;
}

int nr_segment_compare(const nr_segment_t* a, const nr_segment_t* b) {
  nrtime_t duration_a = a->stop_time - a->start_time;
  nrtime_t duration_b = b->stop_time - b->start_time;

  if (duration_a < duration_b) {
    return -1;
  } else if (duration_a > duration_b) {
    return 1;
  }
  return 0;
}

char* nr_segment_traces_create_data(const nrtxn_t* txn,
                                    nrtime_t duration,
                                    const nrobj_t* agent_attributes,
                                    const nrobj_t* user_attributes,
                                    const nrobj_t* intrinsics) {
  nrbuf_t* buf;
  char* data;
  int rv;
  nrpool_t* segment_names;

  /* Currently, this function will not create a trace if the segment count of a transaction
   * exceeds 2000.  Future versions of this function shall sample segments down to
   * 2000.
   */
  if ((NULL == txn) || (0 == txn->segment_count) || (0 == duration) || (2000 < txn->segment_count)) {
    return NULL;
  }

  buf = nr_buffer_create(4096 * 8, 4096 * 4);
  segment_names = nr_string_pool_create();

  /*
   * Here we create a JSON string which will be eventually be compressed,
   * encoded, and embedded into the final trace JSON structure for the
   * collector.
   * For a reference to the transaction trace JSON structure please refer to:
   * https://github.com/newrelic/collector-protocol-validator
   * https://newrelic.atlassian.net/wiki/display/eng/The+Terror+and+Glory+of+Transaction+Traces
   */
  nr_buffer_add(buf, "[", 1);
  nr_buffer_add(buf, "[", 1);
  nr_buffer_add(buf, "0.0", 1); /* Unused timestamp. */
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "{}", 2); /* Unused:  Formerly request-parameters. */
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "{}", 2); /* Unused:  Formerly custom-parameters. */
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "[", 1);
  nr_buffer_add(buf, "0", 1);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_write_uint64_t_as_text(buf, duration / NR_TIME_DIVISOR_MS);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "\"ROOT\"", 6);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "{}", 2);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "[", 1);

  rv = nr_segment_traces_json_print_segments(buf, txn, txn->segment_root,
                                             segment_names);

  if (rv < 0) {
    /*
     * If there was an error during the printing of JSON, then set the duration
     * of this TT to zero so it will be replaced.
     */
    nr_string_pool_destroy(&segment_names);
    nr_buffer_destroy(&buf);
    return 0;
  }

  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, ",", 1);
  {
    nrobj_t* hash = nro_new_hash();

    if (agent_attributes) {
      nro_set_hash(hash, "agentAttributes", agent_attributes);
    }
    if (user_attributes) {
      nro_set_hash(hash, "userAttributes", user_attributes);
    }
    if (intrinsics) {
      nro_set_hash(hash, "intrinsics", intrinsics);
    }

    add_hash_json_to_buffer(buf, hash);
    nro_delete(hash);
  }
  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, ",", 1);
  {
    char* js = nr_string_pool_to_json(segment_names);

    nr_buffer_add(buf, js, nr_strlen(js));
    nr_free(js);
  }
  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "\0", 1);
  data = nr_strdup((const char*)nr_buffer_cptr(buf));

  nr_string_pool_destroy(&segment_names);
  nr_buffer_destroy(&buf);

  return data;
}

nr_minmax_heap_t* nr_segment_traces_heap_create(ssize_t bound) {
  return nr_minmax_heap_create(bound, (nr_minmax_heap_cmp_t)nr_segment_compare,
                               NULL, NULL, NULL);
}

void nr_segment_traces_stoh_iterator_callback(nr_segment_t* segment,
                                              void* userdata) {
  if (nrunlikely(NULL == segment || NULL == userdata)) {
    return;
  } else {
    nr_minmax_heap_t* heap = (nr_minmax_heap_t*)userdata;
    nr_minmax_heap_insert(heap, segment);

    return;
  }
}

void nr_segment_traces_tree_to_heap(nr_segment_t* root,
                                    nr_minmax_heap_t* heap) {
  nr_segment_iterate(
      root, (nr_segment_iter_t)nr_segment_traces_stoh_iterator_callback, heap);
}
