#include "nr_axiom.h"

#include <stddef.h>
#include <stdlib.h>

#include "nr_guid.h"
#include "nr_traces.h"
#include "nr_traces_private.h"
#include "util_buffer.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"

static inline int compare_nrtime_t(nrtime_t a, nrtime_t b) {
  if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  }
  return 0;
}

static int nr_harvest_trace_node_compare(const void* pa, const void* pb) {
  const nr_harvest_trace_node_t* a = (const nr_harvest_trace_node_t*)pa;
  const nr_harvest_trace_node_t* b = (const nr_harvest_trace_node_t*)pb;

  if (a->node->async_context == b->node->async_context) {
    /*
     * Two nodes in the same context: easy! Just compare the start times.
     */
    return compare_nrtime_t(a->node->start_time.when, b->node->start_time.when);
  } else if (0 == a->node->async_context) {
    /*
     * Node A is on the main context, so we want to compare its start time to
     * B's context start time.
     */
    return compare_nrtime_t(a->node->start_time.when, b->context_start);
  } else if (0 == b->node->async_context) {
    /*
     * As above, except reversed: B is on main.
     */
    return compare_nrtime_t(a->context_start, b->node->start_time.when);
  } else {
    /*
     * We don't really have a sensible way of comparing two nodes on non-main
     * contexts, since they're kind of orthogonal. To keep things consistent,
     * though, we'll compare their context start times, which should be OK,
     * since that will result in consistent results from this function to
     * qsort and we'll get the right order in the end.
     */
    return compare_nrtime_t(a->context_start, b->context_start);
  }
}

nr_harvest_trace_node_t* nr_harvest_trace_sort_nodes(const nrtxn_t* txn) {
  nrobj_t* contexts = NULL;
  nr_harvest_trace_node_t* nodes = NULL;
  int i;

  if (NULL == txn) {
    return NULL;
  }

  contexts = nro_new_hash();
  nodes = (nr_harvest_trace_node_t*)nr_calloc(txn->nodes_used,
                                              sizeof(nr_harvest_trace_node_t));

  for (i = 0; i < txn->nodes_used; i++) {
    const nrtxnnode_t* node = &txn->nodes[i];
    const char* ctx_name
        = nr_string_get(txn->trace_strings, node->async_context);
    nr_status_t err = NR_SUCCESS;
    nrtime_t ctx_start = 0;

    ctx_name = (NULL == ctx_name) ? "(main)" : ctx_name;
    ctx_start = (nrtime_t)nro_get_hash_long(contexts, ctx_name, &err);

    if ((NR_SUCCESS != err) || (node->start_time.when < ctx_start)) {
      ctx_start = node->start_time.when;
      (void)nro_set_hash_long(contexts, ctx_name, (int64_t)ctx_start);
    }

    nodes[i].context_start = ctx_start;
    nodes[i].node = node;
  }

  nro_delete(contexts);

  qsort(nodes, txn->nodes_used, sizeof(nr_harvest_trace_node_t),
        nr_harvest_trace_node_compare);

  return nodes;
}

static void add_hash_json_to_buffer(nrbuf_t* buf, const nrobj_t* hash) {
  char* json;

  if (0 == hash) {
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

static void nr_populate_datastore_spans(nr_span_event_t* span_event,
                                        const nrobj_t* data_hash,
                                        nr_txnnode_attributes_t* attributes) {
  const char* host;
  const char* port_path_or_id;
  const char* sql;
  const char* component;
  char* address;

  nr_span_event_set_category(span_event, NR_SPAN_DATASTORE);

  if (NULL != attributes && NULL != attributes->datastore.component) {
    component = attributes->datastore.component;
    nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_COMPONENT,
                                component);
  }

  host = nro_get_hash_string(data_hash, "host", NULL);
  nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_PEER_HOSTNAME,
                              host);

  port_path_or_id = nro_get_hash_string(data_hash, "port_path_or_id", NULL);
  if (NULL == host) {
    host = "unknown";
  }
  if (NULL == port_path_or_id) {
    port_path_or_id = "unknown";
  }
  address = nr_formatf("%s:%s", host, port_path_or_id);
  nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_PEER_ADDRESS,
                              address);
  nr_free(address);

  nr_span_event_set_datastore(
      span_event, NR_SPAN_DATASTORE_DB_INSTANCE,
      nro_get_hash_string(data_hash, "database_name", NULL));

  // depending on the sql recording settings we may have ONE (not both) of these
  // values
  sql = nro_get_hash_string(data_hash, "sql", NULL);
  if (NULL == sql) {
    sql = nro_get_hash_string(data_hash, "sql_obfuscated", NULL);
  }
  if (NULL != sql) {
    nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_DB_STATEMENT,
                                sql);
  }
}

/*
 * Purpose : Populate the span event with the extern data.
 *
 * Params : 1. The target span event that needs to be populated.
 *          2. The data_hash from the transaction node that corresponds with
 * this span event.
 */
static void nr_populate_extern_spans(nr_span_event_t* span_event,
                                     const nrobj_t* data_hash) {
  const char* method;
  const char* url;
  const char* component;

  method = nro_get_hash_string(data_hash, "procedure", NULL);
  nr_span_event_set_external(span_event, NR_SPAN_EXTERNAL_METHOD, method);

  url = nro_get_hash_string(data_hash, "uri", NULL);
  nr_span_event_set_external(span_event, NR_SPAN_EXTERNAL_URL, url);

  component = nro_get_hash_string(data_hash, "library", NULL);
  nr_span_event_set_external(span_event, NR_SPAN_EXTERNAL_COMPONENT, component);

  nr_span_event_set_category(span_event, NR_SPAN_HTTP);
}

/*
 * Purpose : Recursively print node segments to a buffer in json format.
 *
 * Params  : 1. The buffer.
 *           2. The transaction.
 *           3. The node to be added to the buffer.
 *           4. The index of the next node to be added to the buffer.  Though
 *              parameters (3) and (4) appear to be redundant, both are provided
 *              in order to accomodate the root node, which cannot be identified
 *              using an index, and does not inhabit the node array.
 *           5. A string pool that the node names will be put into.  This string
 *              pool is included in the data json after the nodes:  It is used
 *              to mimize the size of the JSON.
 *
 * Returns : The index of the node which should be added to the buffer next,
 *           or -1 in the event of an error.
 *
 * Notes   : Assumes segments are sorted by start stamp.
 */
int nr_traces_json_print_segments(nrbuf_t* buf,
                                  const nrtxn_t* txn,
                                  const nrtxnnode_t* node,
                                  int next,
                                  const nr_harvest_trace_node_t* nodes,
                                  nrpool_t* node_names,
                                  nr_span_event_t* span_events[],
                                  int span_events_size,
                                  const nr_span_event_t* parent_span) {
  int subsequent_kid = 0;
  uint64_t zerobased_start_ms;
  uint64_t zerobased_stop_ms;
  const char* node_name;
  int idx;
  nr_span_event_t* span = NULL;

  if ((0 == buf) || (0 == txn) || (0 == node) || (next < 0) || (0 == nodes)) {
    return -1;
  }

  if (node->start_time.stamp >= node->stop_time.stamp) {
    return -1;
  }

  zerobased_start_ms = (node->start_time.when - txn->root.start_time.when)
                       / NR_TIME_DIVISOR_MS;
  zerobased_stop_ms
      = (node->stop_time.when - txn->root.start_time.when) / NR_TIME_DIVISOR_MS;

  if (txn->root.start_time.when > node->start_time.when) {
    zerobased_start_ms = 0;
  }

  if (txn->root.start_time.when > node->stop_time.when) {
    zerobased_stop_ms = 0;
  }

  if (zerobased_start_ms > zerobased_stop_ms) {
    zerobased_stop_ms = zerobased_start_ms;
  }

  node_name = nr_string_get(txn->trace_strings, node->name);
  idx = nr_string_add(node_names, node_name ? node_name : "<unknown>");
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
  if (node->async_context && txn->async_duration) {
    int async_context_idx;
    const char* async_context;

    async_context = nr_string_get(txn->trace_strings, node->async_context);
    async_context_idx = nr_string_add(
        node_names, async_context ? async_context : "<unknown>");
    /* The internal string tables index at 1, and we wish to index by 0 here. */
    async_context_idx--;

    add_async_hash_json_to_buffer(buf, node->data_hash, async_context_idx);
  } else {
    add_hash_json_to_buffer(buf, node->data_hash);
  }

  /*
   * Create a span event.
   */
  if (NULL != span_events && next < span_events_size) {
    span = nr_span_event_create();

    if (NULL == node->id) {
      /*
       * Trace node representing a custom segment.
       */
      char* guid = nr_guid_create(txn->rnd);
      nr_span_event_set_guid(span, guid);
      nr_free(guid);
    } else {
      /*
       * Trace node representing an external segment.
       *
       * node->id is an id that was used as current span id in an outgoing DT
       * payload.
       */
      nr_span_event_set_guid(span, node->id);
    }
    if (NULL != node->attributes && NR_EXTERNAL == node->attributes->type) {
      nr_span_event_set_category(span, NR_SPAN_HTTP);
    }

    nr_span_event_set_name(span, node_name);
    nr_span_event_set_parent(span, parent_span);
    nr_span_event_set_timestamp(span, node->start_time.when);
    nr_span_event_set_duration(
        span, nr_time_duration(node->start_time.when, node->stop_time.when));

    if (NULL != node->attributes) {
      if (NR_DATASTORE == node->attributes->type) {
        nr_populate_datastore_spans(span, node->data_hash, node->attributes);
      } else if (NR_EXTERNAL == node->attributes->type) {
        nr_populate_extern_spans(span, node->data_hash);
      }
    }

    span_events[next] = span;
  }

  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "[", 1);

  /*
   * In English, we want to continue iterating if:
   *   there is another node to check, and
   *   the next node's start time is before the parent node's stop time, and
   *   either:
   *     the parent node is on the main execution context, or
   *     the parent node is on the same execution context as the next node.
   *
   * The key is that we never want to recurse from a non-main context into
   * another non-main context.
   */
  while ((next < txn->nodes_used)
         && (nodes[next].node->start_time.stamp < node->stop_time.stamp)
         && ((0 == node->async_context)
             || (node->async_context == nodes[next].node->async_context))) {
    if (subsequent_kid) {
      nr_buffer_add(buf, ",", 1);
    }
    next = nr_traces_json_print_segments(buf, txn, nodes[next].node, next + 1,
                                         nodes, node_names, span_events,
                                         span_events_size, span);
    if (next < 0) {
      return -1;
    }
    subsequent_kid = 1;
  }

  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "]", 1);

  return next;
}

char* nr_harvest_trace_create_data(const nrtxn_t* txn,
                                   nrtime_t duration,
                                   const nrobj_t* agent_attributes,
                                   const nrobj_t* user_attributes,
                                   const nrobj_t* intrinsics,
                                   nr_span_event_t* span_events[],
                                   int span_events_size) {
  nrbuf_t* buf;
  char* data;
  int rv;
  nr_harvest_trace_node_t* nodes;
  nrpool_t* node_names;

  if ((0 == txn) || (txn->nodes_used <= 0) || (0 == duration)) {
    return 0;
  }

  /*
   * Setting span_events to NULL prevents the creation of span events in
   * nr_traces_json_print_segments.
   */
  if (!nr_txn_should_create_span_events(txn)) {
    span_events = NULL;
  }

  buf = nr_buffer_create(4096 * 8, 4096 * 4);
  node_names = nr_string_pool_create();

  /*
   * Copy the transaction nodes into a new array that can be sorted by
   * start stamp.  This copy allows the transaction to be constant and
   * unmodified.
   */
  nodes = nr_harvest_trace_sort_nodes(txn);

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
  nr_buffer_add(buf, "0.0",
                1); /* Unused timestamp. TODO(willhf) investigate this */
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

  rv = nr_traces_json_print_segments(buf, txn, &(txn->root), 0, nodes,
                                     node_names, span_events, span_events_size,
                                     NULL);
  nr_free(nodes);
  if (rv < 0) {
    /*
     * If there was an error during the printing of JSON, then set the duration
     * of this TT to zero so it will be replaced.
     */
    nr_string_pool_destroy(&node_names);
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
    char* js = nr_string_pool_to_json(node_names);

    nr_buffer_add(buf, js, nr_strlen(js));
    nr_free(js);
  }
  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "\0", 1);
  data = nr_strdup((const char*)nr_buffer_cptr(buf));

  nr_string_pool_destroy(&node_names);
  nr_buffer_destroy(&buf);

  return data;
}
