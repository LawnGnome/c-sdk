/*
 * This file contains the agent's view of the transaction data command:
 * the payload of data that is sent to the daemon at the end of every
 * transaction.
 */
#include "nr_axiom.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "nr_agent.h"
#include "nr_analytics_events.h"
#include "nr_app.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_distributed_trace.h"
#include "nr_segment_traces.h"
#include "nr_segment_tree.h"
#include "nr_slowsqls.h"
#include "nr_span_event.h"
#include "nr_synthetics.h"
#include "nr_traces.h"
#include "nr_txn.h"
#include "util_apdex.h"
#include "util_buffer.h"
#include "util_errno.h"
#include "util_flatbuffers.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_network.h"
#include "util_strings.h"
#include "util_syscalls.h"

char* nr_txndata_error_to_json(const nrtxn_t* txn) {
  nrobj_t* agent_attributes;
  nrobj_t* user_attributes;
  char* json;

  if (0 == txn->error) {
    return NULL;
  }

  agent_attributes = nr_attributes_agent_to_obj(txn->attributes,
                                                NR_ATTRIBUTE_DESTINATION_ERROR);
  user_attributes = nr_attributes_user_to_obj(txn->attributes,
                                              NR_ATTRIBUTE_DESTINATION_ERROR);

  json = nr_error_to_daemon_json(txn->error, txn->name, agent_attributes,
                                 user_attributes, txn->intrinsics,
                                 txn->request_uri);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);

  return json;
}

static uint32_t nr_txndata_prepend_custom_events(nr_flatbuffer_t* fb,
                                                 const nrtxn_t* txn) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t events;
  int i;
  int event_count;

  const size_t event_size = sizeof(uint32_t);
  const size_t event_align = sizeof(uint32_t);

  event_count = nr_analytics_events_number_saved(txn->custom_events);
  if (0 == event_count) {
    return 0;
  }

  offsets = (uint32_t*)nr_calloc(event_count, sizeof(uint32_t));
  offset = &offsets[0];

  /*
   * Iterate in reverse order to satisfy the integration tests, which should
   * probably be changed to compare custom events in an order agnostic way.
   */
  for (i = event_count - 1; i >= 0; i--, offset++) {
    const char* json;
    uint32_t data;

    json = nr_analytics_events_get_event_json(txn->custom_events, i);
    data = nr_flatbuffers_prepend_string(fb, json);

    nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
    nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
    *offset = nr_flatbuffers_object_end(fb);
  }

  nr_flatbuffers_vector_begin(fb, event_size, event_count, event_align);
  for (i = 0; i < event_count; i++) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  events = nr_flatbuffers_vector_end(fb, event_count);

  nr_free(offsets);
  return events;
}

static void nr_txndata_prepend_span_specific_json(nr_span_event_t* event,
                                                  nrbuf_t* buf,
                                                  const char* category,
                                                  const nrtxn_t* txn) {
  const nr_span_event_t* parent = nr_span_event_get_parent(event);
  long timestamp = nr_span_event_get_timestamp(event) / NR_TIME_DIVISOR_MS;
  double duration = nr_span_event_get_duration(event) / NR_TIME_DIVISOR_D;
  char duration_str[32];

  /*
   * Adding the specific part for each span event.
   */
  nr_buffer_add(buf, NR_PSTR("\"name\":"));
  nr_buffer_add_escape_json(buf, nr_span_event_get_name(event));
  nr_buffer_add(buf, NR_PSTR(","));

  nr_buffer_add(buf, NR_PSTR("\"guid\":"));
  nr_buffer_add_escape_json(buf, nr_span_event_get_guid(event));
  nr_buffer_add(buf, NR_PSTR(","));

  nr_buffer_add(buf, NR_PSTR("\"timestamp\":"));
  nr_buffer_write_uint64_t_as_text(buf, timestamp);
  nr_buffer_add(buf, NR_PSTR(","));

  nr_buffer_add(buf, NR_PSTR("\"duration\":"));
  snprintf(duration_str, sizeof(duration_str), "%f", duration);
  nr_buffer_add(buf, duration_str, nr_strlen(duration_str));
  nr_buffer_add(buf, NR_PSTR(","));

  nr_buffer_add(buf, NR_PSTR("\"category\":"));
  nr_buffer_add_escape_json(buf, category);
  nr_buffer_add(buf, NR_PSTR(","));

  if (NULL == parent) {
    const char* inbound_guid
        = nr_distributed_trace_inbound_get_guid(txn->distributed_trace);
    if (NULL != inbound_guid) {
      nr_buffer_add(buf, NR_PSTR("\"parentId\":"));
      nr_buffer_add_escape_json(buf, inbound_guid);
      nr_buffer_add(buf, NR_PSTR(","));
    }
    nr_buffer_add(buf, NR_PSTR("\"nr.entryPoint\":true"));
  } else {
    nr_buffer_add(buf, NR_PSTR("\"parentId\":"));
    nr_buffer_add_escape_json(buf, nr_span_event_get_guid(parent));
  }
}

static void nr_txndata_prepend_span_datastore(nr_span_event_t* event,
                                              nrbuf_t* buf) {
  const char* component;
  const char* statement;
  const char* instance;
  const char* hostname;
  /*
   * A generic event won't have more to add to the first hash but datastore does
   * so we add an additional comma here
   */
  nr_buffer_add(buf, NR_PSTR(","));

  nr_buffer_add(buf, NR_PSTR("\"span.kind\":\"client\""));

  component = nr_span_event_get_datastore(event, NR_SPAN_DATASTORE_COMPONENT);
  if (NULL != component) {
    nr_buffer_add(buf, NR_PSTR(","));
    nr_buffer_add(buf, NR_PSTR("\"component\":"));
    nr_buffer_add_escape_json(buf, component);
  }

  // This is the end of the first hash
  nr_buffer_add(buf, NR_PSTR("},{},{"));

  statement
      = nr_span_event_get_datastore(event, NR_SPAN_DATASTORE_DB_STATEMENT);
  if (NULL != statement) {
    nr_buffer_add(buf, NR_PSTR("\"db.statement\":"));
    nr_buffer_add_escape_json(buf, statement);
    nr_buffer_add(buf, NR_PSTR(","));
  }

  instance = nr_span_event_get_datastore(event, NR_SPAN_DATASTORE_DB_INSTANCE);
  if (NULL != instance) {
    nr_buffer_add(buf, NR_PSTR("\"db.instance\":"));
    nr_buffer_add_escape_json(buf, instance);
    nr_buffer_add(buf, NR_PSTR(","));
  }

  hostname
      = nr_span_event_get_datastore(event, NR_SPAN_DATASTORE_PEER_HOSTNAME);
  if (NULL != hostname) {
    nr_buffer_add(buf, NR_PSTR("\"peer.hostname\":"));
    nr_buffer_add_escape_json(buf, hostname);
    nr_buffer_add(buf, NR_PSTR(","));
  }

  nr_buffer_add(buf, NR_PSTR("\"peer.address\":"));
  nr_buffer_add_escape_json(
      buf, nr_span_event_get_datastore(event, NR_SPAN_DATASTORE_PEER_ADDRESS));
}

static void nr_txndata_prepend_span_external(nr_span_event_t* span,
                                             nrbuf_t* buf) {
  const char* method;
  const char* url;
  const char* component;

  /*
   * A generic event won't have more to add to the first hash but external does
   * so we add an additional comma here
   */
  nr_buffer_add(buf, NR_PSTR(","));
  nr_buffer_add(buf, NR_PSTR("\"span.kind\":\"client\""));

  component = nr_span_event_get_external(span, NR_SPAN_EXTERNAL_COMPONENT);
  if (component) {
    nr_buffer_add(buf, NR_PSTR(","));
    nr_buffer_add(buf, NR_PSTR("\"component\":"));
    nr_buffer_add_escape_json(buf, component);
  }

  // This is the end of the first hash
  nr_buffer_add(buf, NR_PSTR("},{},{"));

  url = nr_span_event_get_external(span, NR_SPAN_EXTERNAL_URL);
  if (url) {
    nr_buffer_add(buf, NR_PSTR("\"http.url\":"));
    nr_buffer_add_escape_json(buf, url);
  }

  method = nr_span_event_get_external(span, NR_SPAN_EXTERNAL_METHOD);
  if (url && method) {
    nr_buffer_add(buf, NR_PSTR(","));
  }
  if (method) {
    nr_buffer_add(buf, NR_PSTR("\"http.method\":"));
    nr_buffer_add_escape_json(buf, method);
  }
}

static uint32_t nr_txndata_prepend_span_events(nr_flatbuffer_t* fb,
                                               const nrtxn_t* txn,
                                               nr_span_event_t* span_events[],
                                               int span_events_size) {
  int event_count = 0;
  const size_t event_size = sizeof(uint32_t);
  const size_t event_align = sizeof(uint32_t);
  uint32_t data;
  uint32_t* offsets;
  char* spanevt_json_common;
  nrbuf_t* buf;

  if (NULL == span_events || 0 == span_events_size) {
    return 0;
  }

  /*
   * Count the number of span events in the buffer.
   */
  while (event_count < span_events_size && span_events[event_count]) {
    event_count++;
  }

  if (event_count == 0) {
    return 0;
  }

  buf = nr_buffer_create(1024, 0);
  offsets = (uint32_t*)nr_calloc(event_count, sizeof(uint32_t));

  /*
   * This part is the same for all of this transaction's span events.
   */
  spanevt_json_common = nr_formatf(
      "[{"
      "\"type\":\"Span\","
      "\"traceId\":\"%s\","
      "\"transactionId\":\"%s\","
      "\"sampled\":%s,"
      "\"priority\":%f,",
      nr_distributed_trace_get_trace_id(txn->distributed_trace),
      nr_txn_get_guid(txn),
      nr_distributed_trace_is_sampled(txn->distributed_trace) ? "true"
                                                              : "false",
      nr_distributed_trace_get_priority(txn->distributed_trace));

  for (int i = 0; i < event_count; i++) {
    nr_span_event_t* evt = span_events[i];

    nr_buffer_reset(buf);
    nr_buffer_add(buf, spanevt_json_common, nr_strlen(spanevt_json_common));

    switch (nr_span_event_get_category(evt)) {
      case NR_SPAN_HTTP:
        nr_txndata_prepend_span_specific_json(evt, buf, "http", txn);
        nr_txndata_prepend_span_external(evt, buf);
        nr_buffer_add(buf, NR_PSTR("}]"));
        break;
      case NR_SPAN_DATASTORE:
        nr_txndata_prepend_span_specific_json(evt, buf, "datastore", txn);
        nr_txndata_prepend_span_datastore(evt, buf);
        nr_buffer_add(buf, NR_PSTR("}]"));
        break;
      case NR_SPAN_GENERIC:
      default:
        nr_txndata_prepend_span_specific_json(evt, buf, "generic", txn);
        nr_buffer_add(buf, NR_PSTR("},{},{}]"));
        break;
    }

    nr_buffer_add(buf, NR_PSTR("\0"));

    data = nr_flatbuffers_prepend_string(fb, (const char*)nr_buffer_cptr(buf));

    nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
    nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
    offsets[i] = nr_flatbuffers_object_end(fb);
  }

  /*
   * Adding all offsets to the flatbuffer vector.
   */
  nr_flatbuffers_vector_begin(fb, event_size, event_count, event_align);
  for (int i = (event_count - 1); i >= 0; i--) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  data = nr_flatbuffers_vector_end(fb, event_count);

  nr_buffer_destroy(&buf);
  nr_free(spanevt_json_common);
  nr_free(offsets);

  return data;
}

#ifdef NR_CAGENT
static uint32_t nr_txndata_prepend_span_events_to_flatbuffer(
    nr_flatbuffer_t* fb,
    const nrtxn_t* txn,
    nr_vector_t* span_events) {
  nr_span_event_t* events[NR_SPAN_EVENTS_MAX] = {0};
  size_t i;

  for (i = 0; i < nr_vector_size(span_events) && i < NR_SPAN_EVENTS_MAX; i++) {
    nr_vector_get_element(span_events, i, (void**)&events[i]);
  }
  return nr_txndata_prepend_span_events(fb, txn, events, i);
}
#endif /* NR_CAGENT */

static uint32_t nr_txndata_prepend_errors(nr_flatbuffer_t* fb,
                                          const nrtxn_t* txn) {
  char* json;
  int32_t priority;
  uint32_t data;
  uint32_t error;

  const size_t error_count = 1;
  const size_t error_size = sizeof(uint32_t);
  const size_t error_align = sizeof(uint32_t);

  json = nr_txndata_error_to_json(txn);
  if (NULL == json) {
    return 0;
  }

  data = nr_flatbuffers_prepend_string(fb, json);
  nr_free(json);

  priority = nr_error_priority(txn->error);

  nr_flatbuffers_object_begin(fb, ERROR_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, ERROR_FIELD_DATA, data, 0);
  nr_flatbuffers_object_prepend_i32(fb, ERROR_FIELD_PRIORITY, priority, 0);
  error = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_vector_begin(fb, error_size, error_count, error_align);
  nr_flatbuffers_prepend_uoffset(fb, error);
  return nr_flatbuffers_vector_end(fb, error_count);
}

static double nrtime_to_double(nrtime_t x) {
  return (double)x;
}

static uint32_t nr_txndata_prepend_metric_data(nr_flatbuffer_t* fb,
                                               const nrmetric_t* metric,
                                               int scoped) {
  nr_flatbuffers_prep(fb, 8, 56);
  nr_flatbuffers_pad(fb, 6);
  nr_flatbuffers_prepend_bool(fb, nrm_is_forced(metric));
  nr_flatbuffers_prepend_bool(fb, scoped);

  if (nrm_is_apdex(metric)) {
    nr_flatbuffers_prepend_f64(fb, 0);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_max(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_min(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_failing(metric)));
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_tolerating(metric)));
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_satisfying(metric)));
  } else {
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_sumsquares(metric))
                                       / NR_TIME_DIVISOR_D_SQUARE);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_max(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_min(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_exclusive(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_total(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_count(metric)));
  }

  return nr_flatbuffers_len(fb);
}

static uint32_t nr_txndata_prepend_metric(nr_flatbuffer_t* fb,
                                          const nrmtable_t* table,
                                          const nrmetric_t* metric,
                                          int scoped) {
  uint32_t name;
  uint32_t data;

  name = nr_flatbuffers_prepend_string(fb, nrm_get_name(table, metric));

  nr_flatbuffers_object_begin(fb, METRIC_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, METRIC_FIELD_NAME, name, 0);
  data = nr_txndata_prepend_metric_data(fb, metric, scoped);
  nr_flatbuffers_object_prepend_struct(fb, METRIC_FIELD_DATA, data, 0);
  return nr_flatbuffers_object_end(fb);
}

typedef struct _nr_txndata_metric_table_t {
  bool scoped;
  const nrmtable_t* table;
} nr_txndata_metric_table_t;

/*
 * The variadic argument expects a set of const nr_txndata_metric_table_t
 * structs.
 */
static uint32_t nr_txndata_prepend_metrics(nr_flatbuffer_t* fb,
                                           size_t num_metric_tables,
                                           ...) {
  va_list args;
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t metrics;
  size_t num_metrics = 0;
  size_t i;
  nr_txndata_metric_table_t* tables;

  if (0 == num_metric_tables) {
    return 0;
  }

  /* We never really expect more than four tables, so using alloca() should be
   * safe here. */
  tables = (nr_txndata_metric_table_t*)nr_alloca(
      num_metric_tables * sizeof(nr_txndata_metric_table_t));
  va_start(args, num_metric_tables);
  for (i = 0; i < num_metric_tables; i++) {
    tables[i] = va_arg(args, nr_txndata_metric_table_t);

    if (nrunlikely(NULL == tables[i].table)) {
      nrl_warning(NRL_TXN,
                  "unexpected NULL metric table at index %zu; ignoring metrics",
                  i);
      return 0;
    }

    num_metrics += nrm_table_size(tables[i].table);
  }
  va_end(args);

  if (0 == num_metrics) {
    return 0;
  }

  offsets = (uint32_t*)nr_calloc(num_metrics, sizeof(uint32_t));
  offset = &offsets[0];

  for (i = 0; i < num_metric_tables; i++) {
    int j;
    bool scoped = tables[i].scoped;
    const nrmtable_t* table = tables[i].table;
    int num_in_table = nrm_table_size(table);

    for (j = 0; j < num_in_table; j++, offset++) {
      const nrmetric_t* metric;

      metric = nrm_get_metric(table, j);
      *offset = nr_txndata_prepend_metric(fb, table, metric, scoped);
    }
  }

  nr_flatbuffers_vector_begin(fb, sizeof(uint32_t), num_metrics,
                              sizeof(uint32_t));
  for (i = 0; i < num_metrics; i++) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  metrics = nr_flatbuffers_vector_end(fb, num_metrics);

  nr_free(offsets);
  return metrics;
}

static uint32_t nr_txndata_prepend_slowsqls(nr_flatbuffer_t* fb,
                                            const nrtxn_t* txn) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t slowsqls;
  int i;
  int slowsql_count;

  const size_t slowsql_size = sizeof(uint32_t);
  const size_t slowsql_align = sizeof(uint32_t);

  slowsql_count = nr_slowsqls_saved(txn->slowsqls);
  if (0 == slowsql_count) {
    return 0;
  }

  offsets = (uint32_t*)nr_calloc(slowsql_count, sizeof(uint32_t));
  offset = &offsets[0];

  for (i = slowsql_count - 1; i >= 0; i--, offset++) {
    const nr_slowsql_t* slow;
    uint32_t params;
    uint32_t query;
    uint32_t metric;

    slow = nr_slowsqls_at(txn->slowsqls, i);
    params = nr_flatbuffers_prepend_string(fb, nr_slowsql_params(slow));
    query = nr_flatbuffers_prepend_string(fb, nr_slowsql_query(slow));
    metric = nr_flatbuffers_prepend_string(fb, nr_slowsql_metric(slow));

    nr_flatbuffers_object_begin(fb, SLOWSQL_NUM_FIELDS);
    nr_flatbuffers_object_prepend_uoffset(fb, SLOWSQL_FIELD_PARAMS, params, 0);
    nr_flatbuffers_object_prepend_uoffset(fb, SLOWSQL_FIELD_QUERY, query, 0);
    nr_flatbuffers_object_prepend_uoffset(fb, SLOWSQL_FIELD_METRIC, metric, 0);
    nr_flatbuffers_object_prepend_u64(fb, SLOWSQL_FIELD_MAX_MICROS,
                                      nr_slowsql_max(slow) / NR_TIME_DIVISOR_US,
                                      0);
    nr_flatbuffers_object_prepend_u64(fb, SLOWSQL_FIELD_MIN_MICROS,
                                      nr_slowsql_min(slow) / NR_TIME_DIVISOR_US,
                                      0);
    nr_flatbuffers_object_prepend_u64(
        fb, SLOWSQL_FIELD_TOTAL_MICROS,
        nr_slowsql_total(slow) / NR_TIME_DIVISOR_US, 0);
    nr_flatbuffers_object_prepend_i32(fb, SLOWSQL_FIELD_COUNT,
                                      nr_slowsql_count(slow), 0);
    nr_flatbuffers_object_prepend_u32(fb, SLOWSQL_FIELD_ID, nr_slowsql_id(slow),
                                      0);
    *offset = nr_flatbuffers_object_end(fb);
  }

  nr_flatbuffers_vector_begin(fb, slowsql_size, slowsql_count, slowsql_align);
  for (i = slowsql_count - 1; i >= 0; i--) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  slowsqls = nr_flatbuffers_vector_end(fb, slowsql_count);

  nr_free(offsets);
  return slowsqls;
}

#ifndef NR_CAGENT
static char* nr_txndata_trace_data_json(const nrtxn_t* txn,
                                        nr_span_event_t* span_events[],
                                        int span_events_size) {
  nrobj_t* agent_attributes;
  nrobj_t* user_attributes;
  nrtime_t duration;
  char* data_json;

  duration = nr_txn_duration(txn);

  if (0 == nr_txn_should_save_trace(txn, duration)) {
    return NULL;
  }

  agent_attributes = nr_attributes_agent_to_obj(
      txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);
  user_attributes = nr_attributes_user_to_obj(
      txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);

  data_json = nr_harvest_trace_create_data(txn, duration, agent_attributes,
                                           user_attributes, txn->intrinsics,
                                           span_events, span_events_size);
  nro_delete(agent_attributes);
  nro_delete(user_attributes);

  return data_json;
}
#endif

static uint32_t nr_txndata_prepend_error_events(nr_flatbuffer_t* fb,
                                                const nrtxn_t* txn) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t events;
  int event_count;
  nr_analytics_event_t* event;
  const char* json;
  uint32_t data;

  const size_t event_size = sizeof(uint32_t);
  const size_t event_align = sizeof(uint32_t);

  event = nr_error_to_event(txn);
  if (NULL == event) {
    return 0;
  }

  /*
   * Currently there is only one error captured per transaction, but we write
   * it as a vector in preparation for a future where multiple errors are kept.
   */
  event_count = 1;
  offsets = (uint32_t*)nr_calloc(event_count, sizeof(uint32_t));
  offset = &offsets[0];

  json = nr_analytics_event_json(event);
  data = nr_flatbuffers_prepend_string(fb, json);
  nr_analytics_event_destroy(&event);

  nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
  *offset = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_vector_begin(fb, event_size, event_count, event_align);
  nr_flatbuffers_prepend_uoffset(fb, offsets[0]);
  events = nr_flatbuffers_vector_end(fb, event_count);

  nr_free(offsets);
  return events;
}

static uint32_t nr_txndata_prepend_trace_to_flatbuffer(nr_flatbuffer_t* fb,
                                                       const nrtxn_t* txn,
                                                       char* data_json) {
  double duration_ms;
  double timestamp_ms;
  uint32_t data;
  uint32_t guid;
  int force_persist;

  if (NULL == data_json) {
    return 0;
  }

  data = nr_flatbuffers_prepend_string(fb, data_json);
  nr_free(data_json);
  guid = nr_flatbuffers_prepend_string(fb, nr_txn_get_guid(txn));

  timestamp_ms = nr_txn_start_time(txn) / NR_TIME_DIVISOR_MS_D;
  duration_ms = nr_txn_duration(txn) / NR_TIME_DIVISOR_MS_D;
  force_persist = nr_txn_should_force_persist(txn);

  nr_flatbuffers_object_begin(fb, TRACE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, TRACE_FIELD_DATA, data, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRACE_FIELD_GUID, guid, 0);
  nr_flatbuffers_object_prepend_bool(fb, TRACE_FIELD_FORCE_PERSIST,
                                     force_persist, 0);
  nr_flatbuffers_object_prepend_f64(fb, TRACE_FIELD_DURATION, duration_ms, 0);
  nr_flatbuffers_object_prepend_f64(fb, TRACE_FIELD_TIMESTAMP, timestamp_ms, 0);
  return nr_flatbuffers_object_end(fb);
}

#ifndef NR_CAGENT
static uint32_t nr_txndata_prepend_trace(nr_flatbuffer_t* fb,
                                         const nrtxn_t* txn,
                                         nr_span_event_t* span_events[],
                                         int span_events_size) {
  char* data_json;

  data_json = nr_txndata_trace_data_json(txn, span_events, span_events_size);
  return nr_txndata_prepend_trace_to_flatbuffer(fb, txn, data_json);
}
#endif

static uint32_t nr_txndata_prepend_txn_event(nr_flatbuffer_t* fb,
                                             const nrtxn_t* txn) {
  nr_analytics_event_t* event;
  const char* json;
  uint32_t data;

  event = nr_txn_to_event(txn);
  if (NULL == event) {
    return 0;
  }

  json = nr_analytics_event_json(event);
  data = nr_flatbuffers_prepend_string(fb, json);
  nr_analytics_event_destroy(&event);

  nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
  return nr_flatbuffers_object_end(fb);
}

static uint32_t nr_txndata_prepend_synthetics_resource_id(nr_flatbuffer_t* fb,
                                                          const nrtxn_t* txn) {
  const char* synthetics_resource_id;

  synthetics_resource_id = nr_synthetics_resource_id(txn->synthetics);
  if (synthetics_resource_id) {
    return nr_flatbuffers_prepend_string(fb, synthetics_resource_id);
  }
  return 0;
}

static uint32_t nr_txndata_prepend_request_uri(nr_flatbuffer_t* fb,
                                               const nrtxn_t* txn) {
  if (txn->request_uri) {
    return nr_flatbuffers_prepend_string(fb, txn->request_uri);
  }
  return nr_flatbuffers_prepend_string(fb, "<unknown>");
}

static uint32_t nr_txndata_prepend_transaction(nr_flatbuffer_t* fb,
                                               const nrtxn_t* txn,
                                               int32_t pid) {
  uint32_t custom_events;
  uint32_t error_events;
  uint32_t errors;
  uint32_t metrics;
  uint32_t name;
  uint32_t request_uri;
  uint32_t resource_id;
  uint32_t slowsqls;
  uint32_t txn_event;
  uint32_t txn_trace;
  uint32_t span_events;

#ifdef NR_CAGENT
  /* The C Agent, leveraging the Axiom library, currently uses a tree of
   * segments to represent a transaction trace.  Thus, its assembly of a trace
   * is substantively different than that of the PHP Agent.  Conditionally
   * compile the appropriate call for trace assembly based on the agent in use.
   */
  nr_segment_tree_result_t assembly_result = {0};

  nr_segment_tree_assemble_data(txn, &assembly_result, NR_TXN_MAX_NODES,
                                NR_SPAN_EVENTS_MAX);
  txn_trace = nr_txndata_prepend_trace_to_flatbuffer(
      fb, txn, assembly_result.trace_json);

  span_events = nr_txndata_prepend_span_events_to_flatbuffer(
      fb, txn, assembly_result.span_events);
#else
  nr_span_event_t* spans[NR_TXN_MAX_NODES + 1] = {NULL};
  int span_events_size = sizeof(spans) / sizeof(spans[0]);

  /*
   * Because of its size, txn_trace should be assembled first to optimize memory
   * locality for the daemon.
   */
  txn_trace = nr_txndata_prepend_trace(fb, txn, spans, span_events_size);
  span_events
      = nr_txndata_prepend_span_events(fb, txn, spans, span_events_size);
#endif
  error_events = nr_txndata_prepend_error_events(fb, txn);
  custom_events = nr_txndata_prepend_custom_events(fb, txn);
  slowsqls = nr_txndata_prepend_slowsqls(fb, txn);
  errors = nr_txndata_prepend_errors(fb, txn);

#ifdef NR_CAGENT
  metrics = nr_txndata_prepend_metrics(
      fb, 4,
      ((nr_txndata_metric_table_t){.scoped = false,
                                   .table = txn->unscoped_metrics}),
      ((nr_txndata_metric_table_t){.scoped = true,
                                   .table = txn->scoped_metrics}),
      ((nr_txndata_metric_table_t){.scoped = false,
                                   .table = assembly_result.unscoped_metrics}),
      ((nr_txndata_metric_table_t){.scoped = true,
                                   .table = assembly_result.scoped_metrics}));

  nrm_table_destroy(&assembly_result.scoped_metrics);
  nrm_table_destroy(&assembly_result.unscoped_metrics);
#else
  metrics = nr_txndata_prepend_metrics(
      fb, 2,
      ((nr_txndata_metric_table_t){.scoped = false,
                                   .table = txn->unscoped_metrics}),
      ((nr_txndata_metric_table_t){.scoped = true,
                                   .table = txn->scoped_metrics}));
#endif

  txn_event = nr_txndata_prepend_txn_event(fb, txn);
  resource_id = nr_txndata_prepend_synthetics_resource_id(fb, txn);
  request_uri = nr_txndata_prepend_request_uri(fb, txn);
  name = nr_flatbuffers_prepend_string(fb, txn->name);

  nr_flatbuffers_object_begin(fb, TRANSACTION_NUM_FIELDS);
  nr_flatbuffers_object_prepend_f64(
      fb, TRANSACTION_FIELD_SAMPLING_PRIORITY,
      (double)nr_distributed_trace_get_priority(txn->distributed_trace), 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_ERROR_EVENTS,
                                        error_events, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_TRACE, txn_trace,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_CUSTOM_EVENTS,
                                        custom_events, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_SLOW_SQLS,
                                        slowsqls, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_ERRORS, errors,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_METRICS, metrics,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_TXN_EVENT,
                                        txn_event, 0);
  nr_flatbuffers_object_prepend_i32(fb, TRANSACTION_FIELD_PID, pid, 0);
  nr_flatbuffers_object_prepend_uoffset(
      fb, TRANSACTION_FIELD_SYNTHETICS_RESOURCE_ID, resource_id, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_URI, request_uri,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_NAME, name, 0);

  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_SPAN_EVENTS,
                                        span_events, 0);

#ifdef NR_CAGENT
  nr_vector_destroy(&assembly_result.span_events);
#else
  for (int i = 0; i < span_events_size && spans[i]; i++) {
    nr_span_event_destroy(&spans[i]);
  }
#endif

  return nr_flatbuffers_object_end(fb);
}

nr_flatbuffer_t* nr_txndata_encode(const nrtxn_t* txn) {
  nr_flatbuffer_t* fb;
  uint32_t message;
  uint32_t agent_run_id;
  uint32_t transaction;

  fb = nr_flatbuffers_create(0);
  transaction = nr_txndata_prepend_transaction(fb, txn, (int32_t)nr_getpid());
  agent_run_id = nr_flatbuffers_prepend_string(fb, txn->agent_run_id);

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, transaction, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_TXN, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id, 0);
  message = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_finish(fb, message);

  return fb;
}

/* Hook for stubbing TXNDATA messages during testing. */
nr_status_t (*nr_cmd_txndata_hook)(int daemon_fd, const nrtxn_t* txn) = NULL;

/*
 * This timeout will delay this PHP process, but the request has finished, so
 * this will not impact response time.  Therefore this is not as important
 * as the timeout in appinfo.  However, it will prevent this process from
 * handling a new request, so it will have some impact.
 */
#define NR_TXNDATA_SEND_TIMEOUT_MSEC 500

nr_status_t nr_cmd_txndata_tx(int daemon_fd, const nrtxn_t* txn) {
  nr_flatbuffer_t* msg;
  size_t msglen;
  nr_status_t st;

  if (nr_cmd_txndata_hook) {
    return nr_cmd_txndata_hook(daemon_fd, txn);
  }

  if ((NULL == txn) || (daemon_fd < 0)) {
    return NR_FAILURE;
  }

  nrl_verbosedebug(
      NRL_TXN,
      "sending txnname='%.64s'"
      " agent_run_id=" NR_AGENT_RUN_ID_FMT
      " nodes_used=%d"
      " duration=" NR_TIME_FMT " threshold=" NR_TIME_FMT " priority=%f",
      txn->name ? txn->name : "unknown", txn->agent_run_id, txn->nodes_used,
      nr_txn_duration(txn), txn->options.tt_threshold,
      (double)nr_distributed_trace_get_priority(txn->distributed_trace));

  msg = nr_txndata_encode(txn);
  msglen = nr_flatbuffers_len(msg);

  nrl_verbosedebug(NRL_DAEMON, "sending transaction message, len=%zu", msglen);

  if (nr_command_is_flatbuffer_invalid(msg, msglen)) {
    nr_flatbuffers_destroy(&msg);
    return NR_FAILURE;
  }

  nr_agent_lock_daemon_mutex();
  {
    nrtime_t deadline;

    deadline
        = nr_get_time() + (NR_TXNDATA_SEND_TIMEOUT_MSEC * NR_TIME_DIVISOR_MS);
    st = nr_write_message(daemon_fd, nr_flatbuffers_data(msg), msglen,
                          deadline);
  }
  nr_agent_unlock_daemon_mutex();
  nr_flatbuffers_destroy(&msg);

  if (NR_SUCCESS != st) {
    nrl_error(NRL_DAEMON, "TXNDATA failure: len=%zu errno=%s", msglen,
              nr_errno(errno));
    nr_agent_close_daemon_connection();
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}
