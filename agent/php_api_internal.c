#include "php_agent.h"
#include "php_api_internal.h"
#include "php_call.h"
#include "php_hash.h"
#include "nr_datastore_instance.h"
#include "nr_header.h"
#include "nr_traces.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_system.h"

/* Test scaffolding */
#ifdef TAGS
void zif_newrelic_get_request_metadata(void); /* ctags landing pad only */
void newrelic_get_request_metadata(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_get_request_metadata) {
  char* id = NULL;
  char* synthetics = NULL;
  char* transaction = NULL;
  char* transport = NULL;
  char* newrelic = NULL;
  const char* transport_default = "unknown";
  nr_string_len_t transport_len = 0;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (FAILURE
      == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                  ZEND_NUM_ARGS() TSRMLS_CC, "|s", &transport,
                                  &transport_len)) {
    /*
     * This really, really shouldn't happen, since this is an internal API.
     */
    nrl_debug(NRL_API, "newrelic_get_request_metadata: cannot parse args");
    transport = NULL;
  }

  array_init(return_value);

  nr_header_outbound_request(NRPRG(txn), &id, &transaction, &synthetics,
                             &newrelic);

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(NRL_CAT,
                     "CAT: outbound request: transport='%.*s' %s=" NRP_FMT
                     " %s=" NRP_FMT,
                     transport ? 20 : NRSAFELEN(sizeof(transport_default) - 1),
                     transport ? transport : transport_default, X_NEWRELIC_ID,
                     NRP_CAT(id), X_NEWRELIC_TRANSACTION, NRP_CAT(transaction));
  }

  if (NULL != id) {
    nr_php_add_assoc_string(return_value, X_NEWRELIC_ID, id);
  }

  if (NULL != synthetics) {
    nr_php_add_assoc_string(return_value, X_NEWRELIC_SYNTHETICS, synthetics);
  }

  if (NULL != transaction) {
    nr_php_add_assoc_string(return_value, X_NEWRELIC_TRANSACTION, transaction);
  }

  if (NULL != newrelic) {
    nr_php_add_assoc_string(return_value, NEWRELIC, newrelic);
  }

  nr_free(id);
  nr_free(synthetics);
  nr_free(transaction);
  nr_free(newrelic);
}

#ifdef ENABLE_TESTING_API

PHP_FUNCTION(newrelic_get_hostname) {
  char* hostname = NULL;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_TSRMLS;

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  hostname = nr_system_get_hostname();
  nr_php_zval_str(return_value, hostname);
  nr_free(hostname);
}

PHP_FUNCTION(newrelic_get_metric_table) {
  char* json = NULL;
  zval* json_zv = NULL;
  const nrmtable_t* metrics;
  zend_bool scoped = 0;
  zval* table = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  RETVAL_FALSE;

  if (!nr_php_recording(TSRMLS_C)) {
    goto end;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &scoped)) {
    goto end;
  }

  metrics = scoped ? NRTXN(scoped_metrics) : NRTXN(unscoped_metrics);
  json = nr_metric_table_to_daemon_json(metrics);

  if (NULL == json) {
    php_error(E_WARNING, "%s", "cannot convert metric table to JSON");
    goto end;
  }

  json_zv = nr_php_zval_alloc();
  nr_php_zval_str(json_zv, json);

  table = nr_php_call(NULL, "json_decode", json_zv);
  if (!nr_php_is_zval_valid_array(table)) {
    php_error(E_WARNING, "json_decode() failed on data='%s'", json);
    goto end;
  }

  RETVAL_ZVAL(table, 1, 0);

end:
  nr_free(json);
  nr_php_zval_free(&json_zv);
  nr_php_zval_free(&table);
}

PHP_FUNCTION(newrelic_get_slowsqls) {
  const int count = nr_slowsqls_saved(NRTXN(slowsqls));
  int i;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  array_init(return_value);
  for (i = 0; i < count; i++) {
    const nr_slowsql_t* slowsql = nr_slowsqls_at(NRTXN(slowsqls), i);
    zval* ss_zv;

    if (NULL == slowsql) {
      php_error(E_WARNING, "NULL slowsql at index %d of %d", i, count);
      RETURN_FALSE;
    }

    ss_zv = nr_php_zval_alloc();
    array_init(ss_zv);

    add_assoc_long(ss_zv, "id", (zend_long)nr_slowsql_id(slowsql));
    add_assoc_long(ss_zv, "count", (zend_long)nr_slowsql_count(slowsql));
    add_assoc_long(ss_zv, "min", (zend_long)nr_slowsql_min(slowsql));
    add_assoc_long(ss_zv, "max", (zend_long)nr_slowsql_max(slowsql));
    add_assoc_long(ss_zv, "total", (zend_long)nr_slowsql_total(slowsql));
    nr_php_add_assoc_string(ss_zv, "metric",
                            nr_remove_const(nr_slowsql_metric(slowsql)));
    nr_php_add_assoc_string(ss_zv, "query",
                            nr_remove_const(nr_slowsql_query(slowsql)));
    nr_php_add_assoc_string(ss_zv, "params",
                            nr_remove_const(nr_slowsql_params(slowsql)));

    add_next_index_zval(return_value, ss_zv);
  }
}

PHP_FUNCTION(newrelic_get_trace_json) {
  nrtime_t duration;
  char* json;
  nrtime_t now;
  nrtxntime_t orig_stop_time;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  now = nr_get_time();
  duration = nr_time_duration(NRTXN(root).start_time.when, now);

  /*
   * We have to temporarily change the root node's stop time to now, otherwise
   * the sanity check in nr_traces_json_print_segments() will (rightly) be
   * unhappy.
   */
  orig_stop_time = NRTXN(root).stop_time;
  nr_txn_set_time(NRPRG(txn), &NRTXN(root).stop_time);

  json = nr_harvest_trace_create_data(NRPRG(txn), duration, NULL, NULL, NULL);
  nr_php_zval_str(return_value, json);
  nr_free(json);

  /*
   * Let's put things back how they were. Nobody will ever know that we had
   * this moment.
   */
  NRTXN(root).stop_time = orig_stop_time;
}

PHP_FUNCTION(newrelic_is_localhost) {
  char* host = NULL;
  nr_string_len_t host_len = 0;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &host,
                               &host_len)) {
    RETURN_FALSE;
  }

  if (nr_datastore_instance_is_localhost(host)) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}

PHP_FUNCTION(newrelic_is_recording) {
  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  if (nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}

#endif /* ENABLE_TESTING_API */
