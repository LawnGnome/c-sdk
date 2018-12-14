#include "nr_axiom.h"
#include "nr_agent.h"
#include "nr_analytics_events.h"
#include "nr_analytics_events_private.h"
#include "nr_app.h"
#include "nr_attributes.h"
#include "nr_commands.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_custom_events.h"
#include "nr_errors.h"
#include "nr_errors_private.h"
#include "nr_slowsqls.h"
#include "nr_slowsqls.h"
#include "nr_traces.h"
#include "nr_traces_private.h"
#include "nr_txn.h"
#include "nr_txn_private.h"
#include "util_buffer.h"
#include "util_buffer.h"
#include "util_cpu.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_network.h"
#include "util_reply.h"
#include "util_sql.h"
#include "util_strings.h"
#include "util_syscalls.h"

#include "tlib_main.h"

/* This is defined only to satisfy link requirements, and is not shared amongst
 * threads. */
nrapplist_t* nr_agent_applist = 0;

void nr_agent_close_daemon_connection(void) {}

nr_status_t nr_agent_lock_daemon_mutex(void) {
  return NR_SUCCESS;
}

nr_status_t nr_agent_unlock_daemon_mutex(void) {
  return NR_SUCCESS;
}

nrapp_t* nr_app_verify_id(nrapplist_t* applist NRUNUSED,
                          const char* agent_run_id NRUNUSED) {
  return 0;
}

static void test_encode_errors(void) {
  nrtxn_t txn;
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  nr_aoffset_t errors;
  uint32_t count;
  int data_type;
  int did_pass;

  nr_memset(&txn, 0, sizeof(txn));
  txn.name = nr_strdup("txnname");
  txn.error = nr_error_create(123, "msg", "cls", "[\"stacktrace json\"]",
                              887788 * NR_TIME_DIVISOR_MS);

  txn.intrinsics = nro_new_hash();
  nro_set_hash_string(txn.intrinsics, "a", "b");

  txn.attributes = nr_attributes_create(0);
  nr_attributes_user_add_long(txn.attributes, NR_ATTRIBUTE_DESTINATION_ERROR,
                              "user_long", 1);
  nr_attributes_agent_add_long(txn.attributes, NR_ATTRIBUTE_DESTINATION_ERROR,
                               "agent_long", 2);
  nr_attributes_user_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_ERROR, "NOPE",
      1);
  nr_attributes_agent_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_ERROR, "NOPE",
      2);

  fb = nr_txndata_encode(&txn);
  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  data_type = nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  did_pass = tlib_pass_if_true(__func__, MESSAGE_BODY_TXN == data_type,
                               "data_type=%d", data_type);
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");
  if (0 != did_pass) {
    goto done;
  }

  count = nr_flatbuffers_table_read_vector_len(&tbl, TRANSACTION_FIELD_ERRORS);
  if (0 != tlib_pass_if_true(__func__, 1 == count, "count=%d", count)) {
    goto done;
  }

  errors = nr_flatbuffers_table_read_vector(&tbl, TRANSACTION_FIELD_ERRORS);

  /* Read the first error. */
  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, errors).offset);

  tlib_pass_if_int32_t_equal(
      __func__, 123,
      nr_flatbuffers_table_read_i8(&tbl, ERROR_FIELD_PRIORITY, 0));

  /*
   * Can't use tlib_pass_if_bytes_equal macro here because NR_PSTR needs to
   * be expanded before tlib_pass_if_bytes_equal for the number of
   * arguments to match.
   */
  tlib_pass_if_bytes_equal_f(
      __func__,
      NR_PSTR(
          "[887788,\"txnname\",\"msg\",\"cls\",{\"stack_trace\":[\"stacktrace "
          "json\"],\"agentAttributes\":{\"agent_long\":2},\"userAttributes\":{"
          "\"user_long\":1},\"intrinsics\":{\"a\":\"b\"}}]"),
      nr_flatbuffers_table_read_bytes(&tbl, ERROR_FIELD_DATA),
      nr_flatbuffers_table_read_vector_len(&tbl, ERROR_FIELD_DATA), __FILE__,
      __LINE__);

done:
  nr_flatbuffers_destroy(&fb);
  nr_txn_destroy_fields(&txn);
}

static void test_encode_slowsqls(void) {
  nrtxn_t txn;
  nr_flatbuffers_table_t tbl;
  nr_flatbuffers_table_t slow;
  nr_flatbuffer_t* fb;
  nr_aoffset_t slowsqls;
  uint32_t count;
  int data_type;
  int did_pass;
  nr_slowsqls_params_t params = {
      .sql = "SELECT *",
      .duration = 1 * NR_TIME_DIVISOR,
      .stacktrace_json = "[\"backtrace\"]",
      .metric_name = "metric_name",
  };

  nr_memset(&txn, 0, sizeof(txn));
  txn.name = nr_strdup("txn_name");
  txn.request_uri = nr_strdup("request_uri");

  txn.slowsqls = nr_slowsqls_create(10);

  nr_slowsqls_add(txn.slowsqls, &params);

  fb = nr_txndata_encode(&txn);
  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  data_type = nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  did_pass = tlib_pass_if_true(__func__, MESSAGE_BODY_TXN == data_type,
                               "data_type=%d", data_type);
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");
  if (0 != did_pass) {
    goto done;
  }

  count
      = nr_flatbuffers_table_read_vector_len(&tbl, TRANSACTION_FIELD_SLOW_SQLS);
  if (0 != tlib_pass_if_true(__func__, 1 == count, "count=%d", count)) {
    goto done;
  }

  slowsqls
      = nr_flatbuffers_table_read_vector(&tbl, TRANSACTION_FIELD_SLOW_SQLS);

  nr_flatbuffers_table_init(
      &slow, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, slowsqls).offset);

  tlib_pass_if_int32_t_equal(
      __func__, 1787882637,
      nr_flatbuffers_table_read_i32(&slow, SLOWSQL_FIELD_ID, 0));

  tlib_pass_if_uint32_t_equal(
      __func__, 1,
      nr_flatbuffers_table_read_u32(&slow, SLOWSQL_FIELD_COUNT, 0));

  tlib_pass_if_uint64_t_equal(
      __func__, 1 * NR_TIME_DIVISOR,
      nr_flatbuffers_table_read_u64(&slow, SLOWSQL_FIELD_TOTAL_MICROS, 0));

  tlib_pass_if_uint64_t_equal(
      __func__, 1 * NR_TIME_DIVISOR,
      nr_flatbuffers_table_read_u64(&slow, SLOWSQL_FIELD_MIN_MICROS, 0));

  tlib_pass_if_uint64_t_equal(
      __func__, 1 * NR_TIME_DIVISOR,
      nr_flatbuffers_table_read_u64(&slow, SLOWSQL_FIELD_MAX_MICROS, 0));

  tlib_pass_if_str_equal(
      __func__, "metric_name",
      nr_flatbuffers_table_read_str(&slow, SLOWSQL_FIELD_METRIC));

  tlib_pass_if_str_equal(
      __func__, "SELECT *",
      nr_flatbuffers_table_read_str(&slow, SLOWSQL_FIELD_QUERY));

  tlib_pass_if_bytes_equal_f(
      __func__, NR_PSTR("{\"backtrace\":[\"backtrace\"]}"),
      nr_flatbuffers_table_read_bytes(&slow, SLOWSQL_FIELD_PARAMS),
      nr_flatbuffers_table_read_vector_len(&slow, SLOWSQL_FIELD_PARAMS),
      __FILE__, __LINE__);

done:
  nr_flatbuffers_destroy(&fb);
  nr_txn_destroy_fields(&txn);
}

static void test_encode_metrics(void) {
  nrtxn_t txn;
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  nr_aoffset_t metrics;
  nr_aoffset_t data;
  uint32_t count;
  int data_type;
  int did_pass;

  nr_memset(&txn, 0, sizeof(txn));
  txn.name = nr_strdup("my_txn_name");
  txn.scoped_metrics = nrm_table_create(10);
  txn.unscoped_metrics = nrm_table_create(10);

  nrm_add(txn.scoped_metrics, "scoped", 1 * NR_TIME_DIVISOR);
  nrm_add(txn.unscoped_metrics, "unscoped", 2 * NR_TIME_DIVISOR);
  nrm_add_internal(1, txn.unscoped_metrics, "forced", 1,
                   (nrtime_t)(2.222222 * NR_TIME_DIVISOR_D),
                   (nrtime_t)(3.456789 * NR_TIME_DIVISOR_D),
                   (nrtime_t)(4.482911 * NR_TIME_DIVISOR_D),
                   (nrtime_t)(5.555556 * NR_TIME_DIVISOR_D),
                   (nrtime_t)(6.060606 * NR_TIME_DIVISOR_D_SQUARE));

  nrm_add_apdex(txn.unscoped_metrics, "apdex", 1, 2, 3,
                4.816326 * NR_TIME_DIVISOR);

  fb = nr_txndata_encode(&txn);

  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  data_type = nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  did_pass = tlib_pass_if_true(__func__, MESSAGE_BODY_TXN == data_type,
                               "data_type=%d", data_type);
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");
  if (0 != did_pass) {
    goto done;
  }

  count = nr_flatbuffers_table_read_vector_len(&tbl, TRANSACTION_FIELD_METRICS);
  if (0 != tlib_pass_if_true(__func__, 4 == count, "count=%d", count)) {
    goto done;
  }

  metrics = nr_flatbuffers_table_read_vector(&tbl, TRANSACTION_FIELD_METRICS);

  /*
   * Metrics[0]: "scoped"
   */
  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, metrics).offset);
  tlib_pass_if_str_equal(
      __func__, "scoped",
      nr_flatbuffers_table_read_str(&tbl, METRIC_FIELD_NAME));

  data = nr_flatbuffers_table_lookup(&tbl, METRIC_FIELD_DATA);
  tlib_pass_if_double_equal(
      __func__, 1,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_COUNT));
  tlib_pass_if_double_equal(
      __func__, 1,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_TOTAL));
  tlib_pass_if_double_equal(
      __func__, 1,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_EXCLUSIVE));
  tlib_pass_if_double_equal(
      __func__, 1,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MIN));
  tlib_pass_if_double_equal(
      __func__, 1,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MAX));
  tlib_pass_if_double_equal(
      __func__, 1,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_SOS));
  tlib_pass_if_int8_t_equal(
      __func__, 1,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_SCOPED));
  tlib_pass_if_int8_t_equal(
      __func__, 0,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_FORCED));

  /*
   * Metrics[1]: "apdex"
   */
  metrics.offset += sizeof(uint32_t);
  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, metrics).offset);

  tlib_pass_if_str_equal(
      __func__, "apdex",
      nr_flatbuffers_table_read_str(&tbl, METRIC_FIELD_NAME));

  data = nr_flatbuffers_table_lookup(&tbl, METRIC_FIELD_DATA);
  tlib_pass_if_double_equal(
      __func__, 1.0,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_COUNT));
  tlib_pass_if_double_equal(
      __func__, 2.0,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_TOTAL));
  tlib_pass_if_double_equal(
      __func__, 3.0,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_EXCLUSIVE));
  tlib_pass_if_double_equal(
      __func__, 4.816326,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MIN));
  tlib_pass_if_double_equal(
      __func__, 4.816326,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MAX));
  tlib_pass_if_double_equal(
      __func__, 0.0,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_SOS));
  tlib_pass_if_int8_t_equal(
      __func__, 0,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_SCOPED));
  tlib_pass_if_int8_t_equal(
      __func__, 0,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_FORCED));

  /*
   * Metrics[2]: "forced"
   */
  metrics.offset += sizeof(uint32_t);
  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, metrics).offset);

  tlib_pass_if_str_equal(
      __func__, "forced",
      nr_flatbuffers_table_read_str(&tbl, METRIC_FIELD_NAME));

  data = nr_flatbuffers_table_lookup(&tbl, METRIC_FIELD_DATA);
  tlib_pass_if_double_equal(
      __func__, 1.0,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_COUNT));
  tlib_pass_if_double_equal(
      __func__, 2.222222,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_TOTAL));
  tlib_pass_if_double_equal(
      __func__, 3.456789,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_EXCLUSIVE));
  tlib_pass_if_double_equal(
      __func__, 4.482911,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MIN));
  tlib_pass_if_double_equal(
      __func__, 5.555556,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MAX));
  tlib_pass_if_double_equal(
      __func__, 6.060606,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_SOS));
  tlib_pass_if_int8_t_equal(
      __func__, 0,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_SCOPED));
  tlib_pass_if_int8_t_equal(
      __func__, 1,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_FORCED));

  /*
   * Metrics[3]: "unscoped"
   */
  metrics.offset += sizeof(uint32_t);
  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, metrics).offset);

  tlib_pass_if_str_equal(
      __func__, "unscoped",
      nr_flatbuffers_table_read_str(&tbl, METRIC_FIELD_NAME));

  data = nr_flatbuffers_table_lookup(&tbl, METRIC_FIELD_DATA);
  tlib_pass_if_double_equal(
      __func__, 1.0,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_COUNT));
  tlib_pass_if_double_equal(
      __func__, 2.0,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_TOTAL));
  tlib_pass_if_double_equal(
      __func__, 2.0,
      nr_flatbuffers_read_f64(tbl.data,
                              data.offset + METRIC_DATA_VOFFSET_EXCLUSIVE));
  tlib_pass_if_double_equal(
      __func__, 2.0,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MIN));
  tlib_pass_if_double_equal(
      __func__, 2.0,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_MAX));
  tlib_pass_if_double_equal(
      __func__, 4.0,
      nr_flatbuffers_read_f64(tbl.data, data.offset + METRIC_DATA_VOFFSET_SOS));
  tlib_pass_if_int8_t_equal(
      __func__, 0,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_SCOPED));
  tlib_pass_if_int8_t_equal(
      __func__, 0,
      nr_flatbuffers_read_i8(tbl.data,
                             data.offset + METRIC_DATA_VOFFSET_FORCED));

done:
  nr_flatbuffers_destroy(&fb);
  nr_txn_destroy_fields(&txn);
}

static void test_encode_error_events(void) {
  nrtxn_t txn;
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  nr_aoffset_t events;
  uint32_t count;
  int data_type;
  int did_pass;

  nr_memset(&txn, 0, sizeof(txn));
  txn.error = nr_error_create(123, "msg", "cls", "[\"stacktrace json\"]",
                              857281 * NR_TIME_DIVISOR_MS);
  txn.options.error_events_enabled = 1;
  txn.name = nr_strdup("my_txn_name");
  txn.guid = nr_strdup("abcd");
  txn.root.start_time.when = 415 * NR_TIME_DIVISOR;
  txn.root.stop_time.when = txn.root.start_time.when + 543 * NR_TIME_DIVISOR_MS;

  fb = nr_txndata_encode(&txn);
  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  data_type = nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  did_pass = tlib_pass_if_true(__func__, MESSAGE_BODY_TXN == data_type,
                               "data_type=%d", data_type);
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");
  if (0 != did_pass) {
    goto done;
  }

  count = nr_flatbuffers_table_read_vector_len(&tbl,
                                               TRANSACTION_FIELD_ERROR_EVENTS);
  if (0 != tlib_pass_if_true(__func__, 1 == count, "count=%d", count)) {
    goto done;
  }

  events
      = nr_flatbuffers_table_read_vector(&tbl, TRANSACTION_FIELD_ERROR_EVENTS);

  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, events).offset);
  tlib_pass_if_bytes_equal_f(
      __func__,
      NR_PSTR("["
              "{"
              "\"type\":\"TransactionError\","
              "\"timestamp\":857.28100,"
              "\"error.class\":\"cls\","
              "\"error.message\":\"msg\","
              "\"transactionName\":\"my_txn_name\","
              "\"duration\":0.54300,"
              "\"nr.transactionGuid\":\"abcd\""
              "},"
              "{},"
              "{}"
              "]"),
      nr_flatbuffers_table_read_bytes(&tbl, EVENT_FIELD_DATA),
      nr_flatbuffers_table_read_vector_len(&tbl, EVENT_FIELD_DATA), __FILE__,
      __LINE__);

done:
  nr_flatbuffers_destroy(&fb);
  nr_txn_destroy_fields(&txn);
}

static void test_encode_custom_events(void) {
  nrtxn_t txn;
  nr_flatbuffers_table_t tbl;
  nrobj_t* params;
  nr_flatbuffer_t* fb;
  nr_aoffset_t events;
  nrtime_t now;
  uint32_t count;
  int data_type;
  int did_pass;

  now = 123 * NR_TIME_DIVISOR;
  params = nro_create_from_json("{\"a\":1,\"b\":\"c\"}");

  nr_memset(&txn, 0, sizeof(txn));
  txn.custom_events = nr_analytics_events_create(100);
  nr_custom_events_add_event(txn.custom_events, "type1", params, now, NULL);
  nr_custom_events_add_event(txn.custom_events, "type2", params, now, NULL);

  fb = nr_txndata_encode(&txn);
  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  data_type = nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  did_pass = tlib_pass_if_true(__func__, MESSAGE_BODY_TXN == data_type,
                               "data_type=%d", data_type);
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");
  if (0 != did_pass) {
    goto done;
  }

  count = nr_flatbuffers_table_read_vector_len(&tbl,
                                               TRANSACTION_FIELD_CUSTOM_EVENTS);
  if (0 != tlib_pass_if_true(__func__, 2 == count, "count=%d", count)) {
    goto done;
  }

  events
      = nr_flatbuffers_table_read_vector(&tbl, TRANSACTION_FIELD_CUSTOM_EVENTS);

  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, events).offset);
  tlib_pass_if_bytes_equal_f(
      __func__,
      NR_PSTR("[{\"type\":\"type1\",\"timestamp\":123.00000},{\"b\":\"c\","
              "\"a\":1},{}]"),
      nr_flatbuffers_table_read_bytes(&tbl, EVENT_FIELD_DATA),
      nr_flatbuffers_table_read_vector_len(&tbl, EVENT_FIELD_DATA), __FILE__,
      __LINE__);

  events.offset += sizeof(uint32_t);
  nr_flatbuffers_table_init(
      &tbl, tbl.data, tbl.length,
      nr_flatbuffers_read_indirect(tbl.data, events).offset);
  tlib_pass_if_bytes_equal_f(
      __func__,
      NR_PSTR("[{\"type\":\"type2\",\"timestamp\":123.00000},{\"b\":\"c\","
              "\"a\":1},{}]"),
      nr_flatbuffers_table_read_bytes(&tbl, EVENT_FIELD_DATA),
      nr_flatbuffers_table_read_vector_len(&tbl, EVENT_FIELD_DATA), __FILE__,
      __LINE__);

done:
  nr_flatbuffers_destroy(&fb);
  nr_txn_destroy_fields(&txn);
  nro_delete(params);
}

static void test_encode_trace(void) {
  nrtxn_t txn;
  nr_flatbuffers_table_t tbl;
  nr_flatbuffer_t* fb;
  nrtime_t duration = 1234 * NR_TIME_DIVISOR;
  int data_type;
  int did_pass;

  nr_memset(&txn, 0, sizeof(txn));
  txn.options.tt_threshold = duration - 1;
  txn.status.has_inbound_record_tt = 0;
  txn.status.has_outbound_record_tt = 0;
  txn.type = 0;
  txn.guid = nr_strdup("0123456789abcdef");
  txn.name = nr_strdup("txnname");
  txn.request_uri = nr_strdup("url");

  txn.synthetics = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");
  txn.intrinsics = nro_new_hash();
  txn.attributes = nr_attributes_create(0);
  nro_set_hash_string(txn.intrinsics, "a", "b");
  nr_attributes_user_add_long(
      txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE, "user_long", 1);
  nr_attributes_agent_add_long(
      txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE, "agent_long", 2);
  nr_attributes_user_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_TXN_TRACE,
      "NOPE", 1);
  nr_attributes_agent_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_TXN_TRACE,
      "NOPE", 2);

  txn.trace_strings = nr_string_pool_create();
  txn.nodes_used = 1;

  txn.root.data_hash = 0;
  txn.nodes[0].data_hash = 0;

  txn.root.name = nr_string_add(txn.trace_strings, "the_root");
  txn.nodes[0].name = nr_string_add(txn.trace_strings, "the_node");

  txn.root.start_time.when = 1 * NR_TIME_DIVISOR;
  txn.nodes[0].start_time.when = 2 * NR_TIME_DIVISOR;
  txn.nodes[0].stop_time.when = 3 * NR_TIME_DIVISOR;
  txn.root.stop_time.when = duration + txn.root.start_time.when;

  txn.root.start_time.stamp = 1;
  txn.nodes[0].start_time.stamp = 2;
  txn.nodes[0].stop_time.stamp = 3;
  txn.root.stop_time.stamp = 4;

  txn.root.async_context = 0;
  txn.nodes[0].async_context = 0;

  fb = nr_txndata_encode(&txn);
  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  data_type = nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  did_pass = tlib_pass_if_true(__func__, MESSAGE_BODY_TXN == data_type,
                               "data_type=%d", data_type);
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, TRANSACTION_FIELD_TRACE),
      "trace missing");
  if (0 != did_pass) {
    goto done;
  }

  tlib_pass_if_double_equal(
      __func__, 1000.0,
      nr_flatbuffers_table_read_f64(&tbl, TRACE_FIELD_TIMESTAMP, 0.0));
  tlib_pass_if_double_equal(
      __func__, 1234000.0,
      nr_flatbuffers_table_read_f64(&tbl, TRACE_FIELD_DURATION, 0.0));
  tlib_pass_if_int_equal(
      __func__, 0,
      nr_flatbuffers_table_read_bool(&tbl, TRACE_FIELD_FORCE_PERSIST, 0));
  tlib_pass_if_str_equal(__func__, "0123456789abcdef",
                         nr_flatbuffers_table_read_str(&tbl, TRACE_FIELD_GUID));
  tlib_pass_if_bytes_equal_f(
      __func__,
      NR_PSTR("[[0,{},{},[0,1234000,\"ROOT\",{},[[0,1234000,\"`0\",{},[[1000,"
              "2000,\"`1\",{},[]]]]]],"
              "{\"agentAttributes\":{\"agent_long\":2},\"userAttributes\":{"
              "\"user_long\":1},\"intrinsics\":{\"a\":\"b\"}}],"
              "[\"the_root\",\"the_node\"]]"),
      nr_flatbuffers_table_read_bytes(&tbl, TRACE_FIELD_DATA),
      nr_flatbuffers_table_read_vector_len(&tbl, TRACE_FIELD_DATA), __FILE__,
      __LINE__);

done:
  nr_flatbuffers_destroy(&fb);
  nr_txn_destroy_fields(&txn);
}

static void test_encode_txn_event(void) {
  nrtxn_t txn;
  nr_flatbuffer_t* fb = NULL;
  nr_flatbuffers_table_t tbl;
  int data_type;
  int did_pass;

  nr_memset(&txn, 0, sizeof(txn));

  txn.status.background = 0;
  txn.status.ignore_apdex = 0;
  txn.options.analytics_events_enabled = 1;
  txn.options.apdex_t = 10;
  txn.guid = nr_strdup("abcd");
  txn.name = nr_strdup("my_txn_name");
  txn.root.start_time.when = 123 * NR_TIME_DIVISOR;
  txn.root.stop_time.when = txn.root.start_time.when + 987 * NR_TIME_DIVISOR_MS;
  txn.unscoped_metrics = nrm_table_create(100);
  txn.synthetics = NULL;
  txn.type = 0;

  nrm_add(txn.unscoped_metrics, "Datastore/all", 1 * NR_TIME_DIVISOR);
  nrm_add(txn.unscoped_metrics, "External/all", 2 * NR_TIME_DIVISOR);
  nrm_add(txn.unscoped_metrics, "WebFrontend/QueueTime", 3 * NR_TIME_DIVISOR);

  txn.attributes = nr_attributes_create(0);
  nr_attributes_user_add_long(
      txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "user_long", 1);
  nr_attributes_agent_add_long(
      txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "agent_long", 2);
  nr_attributes_user_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_TXN_EVENT,
      "NOPE", 1);
  nr_attributes_agent_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_TXN_EVENT,
      "NOPE", 2);

  fb = nr_txndata_encode(&txn);
  nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));

  data_type = nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  did_pass = tlib_pass_if_true(__func__, MESSAGE_BODY_TXN == data_type,
                               "data_type=%d", data_type);
  if (0 != did_pass) {
    goto done;
  }

  did_pass = tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");
  if (0 != did_pass) {
    goto done;
  }

  did_pass
      = tlib_pass_if_true(__func__,
                          0
                              != nr_flatbuffers_table_read_union(
                                     &tbl, &tbl, TRANSACTION_FIELD_TXN_EVENT),
                          "trace missing");
  if (0 != did_pass) {
    goto done;
  }

  tlib_pass_if_bytes_equal_f(
      __func__,
      NR_PSTR("[{\"type\":\"Transaction\",\"name\":\"my_txn_name\","
              "\"timestamp\":123.00000,"
              "\"duration\":0.98700,\"totalTime\":0.98700,\"nr.apdexPerfZone\":"
              "\"F\","
              "\"queueDuration\":3.00000,\"externalDuration\":2.00000,"
              "\"databaseDuration\":1.00000},"
              "{\"user_long\":1},{\"agent_long\":2}]"),
      nr_flatbuffers_table_read_bytes(&tbl, EVENT_FIELD_DATA),
      nr_flatbuffers_table_read_vector_len(&tbl, EVENT_FIELD_DATA), __FILE__,
      __LINE__);

done:
  nr_flatbuffers_destroy(&fb);
  nr_txn_destroy_fields(&txn);
}

static void test_bad_daemon_fd(void) {
  nrtxn_t txn;
  nr_status_t st;

  nr_memset(&txn, 0, sizeof(txn));

  st = nr_cmd_txndata_tx(-1, &txn);
  tlib_pass_if_status_failure(__func__, st);
}

static void test_null_txn(void) {
  int socks[2];
  nr_status_t st;

  nbsockpair(socks);
  st = nr_cmd_txndata_tx(socks[0], NULL);
  tlib_pass_if_status_failure(__func__, st);

  nr_close(socks[0]);
  nr_close(socks[1]);
}

static void test_empty_txn(void) {
  nrtxn_t txn;
  int socks[2];
  nrbuf_t* buf = NULL;
  nr_flatbuffers_table_t tbl;
  nr_status_t st;
  nr_aoffset_t absolute;

  nbsockpair(socks);
  nr_memset(&txn, 0, sizeof(txn));

  /*
   * Don't blow up!
   */
  st = nr_cmd_txndata_tx(socks[0], &txn);
  if (0 != tlib_pass_if_status_success(__func__, st)) {
    /* send failed, cannot continue */
    goto done;
  }

  buf = nr_network_receive(socks[1], 100 /* msecs */);
  if (0 != tlib_pass_if_true(__func__, NULL != buf, "buf=%p", buf)) {
    /* receive failure, cannot continue */
    goto done;
  }

  nr_flatbuffers_table_init_root(&tbl, (const uint8_t*)nr_buffer_cptr(buf),
                                 nr_buffer_len(buf));

  tlib_pass_if_int_equal(__func__, MESSAGE_BODY_TXN,
                         nr_flatbuffers_table_read_i8(
                             &tbl, MESSAGE_FIELD_DATA_TYPE, MESSAGE_BODY_NONE));

  tlib_pass_if_true(
      __func__,
      0 != nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA),
      "transaction data missing");

  /*
   * The following should not be present in the transaction data.
   */
  absolute = nr_flatbuffers_table_lookup(
      &tbl, TRANSACTION_FIELD_SYNTHETICS_RESOURCE_ID);
  tlib_pass_if_size_t_equal(__func__, 0, absolute.offset);
  absolute = nr_flatbuffers_table_lookup(&tbl, TRANSACTION_FIELD_CUSTOM_EVENTS);
  tlib_pass_if_size_t_equal(__func__, 0, absolute.offset);
  absolute = nr_flatbuffers_table_lookup(&tbl, TRANSACTION_FIELD_ERRORS);
  tlib_pass_if_size_t_equal(__func__, 0, absolute.offset);
  absolute = nr_flatbuffers_table_lookup(&tbl, TRANSACTION_FIELD_METRICS);
  tlib_pass_if_size_t_equal(__func__, 0, absolute.offset);
  absolute = nr_flatbuffers_table_lookup(&tbl, TRANSACTION_FIELD_SLOW_SQLS);
  tlib_pass_if_size_t_equal(__func__, 0, absolute.offset);
  absolute = nr_flatbuffers_table_lookup(&tbl, TRANSACTION_FIELD_TRACE);
  tlib_pass_if_size_t_equal(__func__, 0, absolute.offset);
  absolute = nr_flatbuffers_table_lookup(&tbl, TRANSACTION_FIELD_TXN_EVENT);
  tlib_pass_if_size_t_equal(__func__, 0, absolute.offset);

  /*
   * Name is always written.
   */
  tlib_pass_if_null(
      __func__, nr_flatbuffers_table_read_bytes(&tbl, TRANSACTION_FIELD_NAME));

  tlib_pass_if_int_equal(
      __func__, nr_getpid(),
      (int)nr_flatbuffers_table_read_i32(&tbl, TRANSACTION_FIELD_PID, 0));

done:
  nr_buffer_destroy(&buf);
  nr_close(socks[0]);
  nr_close(socks[1]);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_encode_custom_events();
  test_encode_errors();
  test_encode_metrics();
  test_encode_error_events();
  test_encode_slowsqls();
  test_encode_trace();
  test_encode_txn_event();

  test_bad_daemon_fd();
  test_null_txn();
  test_empty_txn();
}
