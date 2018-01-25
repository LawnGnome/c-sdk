#include "nr_axiom.h"

#include <stddef.h>

#include "node_datastore.h"
#include "node_datastore_private.h"
#include "nr_datastore_instance.h"
#include "nr_txn.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_system.h"
#include "util_time.h"

#include "tlib_main.h"
#include "test_node_helpers.h"

nrapp_t *
nr_app_verify_id (nrapplist_t *applist NRUNUSED, const char *agent_run_id NRUNUSED)
{
  return 0;
}

static char *
stack_dump_callback (void)
{
  return nr_strdup ("[\"Zip\",\"Zap\"]");
}

static nr_node_datastore_params_t
sample_node_datastore_params (nrtime_t duration)
{
  nr_node_datastore_params_t params = {
    .start      = {.when = 1 * NR_TIME_DIVISOR,            .stamp = 1},
    .stop       = {.when = 1 * NR_TIME_DIVISOR + duration, .stamp = 2},
    .datastore  = {.type = NR_DATASTORE_MONGODB},
    .collection = "my_table",
    .operation  = "my_operation"
  };

  return params;
}

static nr_node_datastore_params_t
sample_node_sql_params (nrtime_t duration)
{
  nr_node_datastore_params_t params = {
    .start        = {.when = 1 * NR_TIME_DIVISOR,            .stamp = 1},
    .stop         = {.when = 1 * NR_TIME_DIVISOR + duration, .stamp = 2},
    .datastore    = {.type = NR_DATASTORE_MYSQL},
    .sql          = {.sql = "SELECT * FROM table WHERE constant = 31"},
    .callbacks    = {.backtrace = stack_dump_callback},
  };

  return params;
}

static void
test_bad_parameters (void)
{
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  /*
   * Test : Bad Parameters
   */
  nr_txn_end_node_datastore (NULL, NULL, NULL);
  nr_txn_end_node_datastore (NULL, &params, NULL);

  nr_txn_end_node_datastore (txn, NULL, NULL);
  test_txn_untouched ("no start", txn);

  params = sample_node_datastore_params (duration);
  params.datastore.type = NR_DATASTORE_MUST_BE_LAST;
  nr_txn_end_node_datastore (txn, &params, NULL);
  test_txn_untouched ("bad datastore", txn);

  txn->status.recording = 0;
  params = sample_node_datastore_params (duration);
  nr_txn_end_node_datastore (txn, &params, NULL);
  test_txn_untouched ("not recording", txn);
  txn->status.recording = 1;

  txn->root.start_time.when = 2 * nr_get_time ();
  params = sample_node_datastore_params (duration);
  nr_txn_end_node_datastore (txn, &params, NULL);
  test_txn_untouched ("future txn start time", txn);
  txn->root.start_time.when = 0;

  params = sample_node_datastore_params (duration);
  params.start.stamp = params.stop.stamp + 1;
  nr_txn_end_node_datastore (txn, &params, NULL);
  test_txn_untouched ("future node start stamp", txn);

  nr_txn_destroy (&txn);
}

static void
test_table_and_operation (void)
{
  const char *tname = "table and operation";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
}

static void
test_no_table (void)
{
  const char *tname = "no table";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  params.collection = NULL;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/operation/MongoDB/my_operation",
    duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 2);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/operation/MongoDB/my_operation");
  nr_txn_destroy (&txn);
}

static void
test_no_table_no_operation (void)
{
  const char *tname = "no table no operation";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  params.collection = NULL;
  params.operation = NULL;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/operation/MongoDB/other",
    duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 2);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/operation/MongoDB/other");
  nr_txn_destroy (&txn);
}

static void
test_return_scoped_metric (void)
{
  const char *tname = "table and operation";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  char *scoped_metric = NULL;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  nr_txn_end_node_datastore (txn, &params, &scoped_metric);
  tlib_pass_if_str_equal (tname, scoped_metric, "Datastore/statement/MongoDB/my_table/my_operation");
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_free (scoped_metric);
}

static void
test_instance_info_with_data_hash (void)
{
  const char *tname = "instance info with data hash";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  params.instance = nr_datastore_instance_create ("super_db_host", "3306", "my_database");

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{\"host\":\"super_db_host\",\"port_path_or_id\":\"3306\",\"database_name\":\"my_database\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 4);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/instance/MongoDB/super_db_host/3306");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_instance_info_reporting_disabled (void)
{
  const char *tname = "instance info disabled";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  txn->options.instance_reporting_enabled = 0;
  params.instance = nr_datastore_instance_create ("super_db_host", "3306", "my_database");

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{\"database_name\":\"my_database\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_instance_database_name_reporting_disabled (void)
{
  const char *tname = "instance info without data hash";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  txn->options.database_name_reporting_enabled = 0;
  params.instance = nr_datastore_instance_create ("super_db_host", "3306", "my_database");

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{\"host\":\"super_db_host\",\"port_path_or_id\":\"3306\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 4);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/instance/MongoDB/super_db_host/3306");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_instance_info_without_data_hash (void)
{
  const char *tname = "instance info without data hash";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  params.instance = nr_datastore_instance_create ("super_db_host", "3306", "my_database");

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{\"host\":\"super_db_host\",\"port_path_or_id\":\"3306\",\"database_name\":\"my_database\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 4);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/instance/MongoDB/super_db_host/3306");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_instance_info_empty (void)
{
  const char *tname = "instance info empty strings";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  params.instance = nr_datastore_instance_create ("", "", "");

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{\"host\":\"unknown\",\"port_path_or_id\":\"unknown\",\"database_name\":\"unknown\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 4);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/instance/MongoDB/unknown/unknown");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_instance_info_null (void)
{
  const char *tname = "instance info is NULL";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  params.instance = nr_datastore_instance_create (NULL, NULL, NULL);;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{\"host\":\"unknown\",\"port_path_or_id\":\"unknown\",\"database_name\":\"unknown\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 4);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/instance/MongoDB/unknown/unknown");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_instance_info_with_localhost (void)
{
  const char *tname = "instance hostname is localhost";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  char *system_host = nr_system_get_hostname ();
  char *expected_data = nr_formatf ("{\"host\":\"%s\",\"port_path_or_id\":\"3306\",\"database_name\":\"my_database\"}", system_host);
  char *expected_metric = nr_formatf ("Datastore/instance/MongoDB/%s/3306", system_host);
  nr_free (system_host);

  params.instance = nr_datastore_instance_create ("localhost", "3306", "my_database");

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation", duration, expected_data);
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 4);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, expected_metric);
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_free (expected_data);
  nr_free (expected_metric);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_instance_metric_with_slashes (void)
{
  const char *tname = "instance with socket";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_datastore_params (duration);

  params.instance = nr_datastore_instance_create ("super_db_host", "/path/to/socket", "my_database");

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MongoDB/my_table/my_operation",
    duration, "{\"host\":\"super_db_host\",\"port_path_or_id\":\"\\/path\\/to\\/socket\",\"database_name\":\"my_database\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 4);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MongoDB/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MongoDB/my_operation");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/instance/MongoDB/super_db_host//path/to/socket");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MongoDB/my_table/my_operation");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

#define EXPLAIN_PLAN_JSON "[[\"a\",\"b\"],[[1,2],[3,4]]]"
static void
test_explain_plan (void)
{
  const char *tname = "explain plan present";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 100;
  const nr_slowsql_t *slow;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  params.sql.plan_json = EXPLAIN_PLAN_JSON;

  txn->options.ep_threshold = 1;
  txn->options.tt_recordsql = NR_SQL_RAW;
  txn->options.ss_threshold = 1;

  nr_txn_end_node_datastore (txn, &params, NULL);
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"explain_plan\":" EXPLAIN_PLAN_JSON ","
     "\"sql\":\"SELECT * FROM table WHERE constant = 31\""
    "}");

  slow = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_str_equal ("raw slowsql saved", nr_slowsql_metric (slow), "Datastore/statement/MySQL/table/select");
  tlib_pass_if_str_equal ("raw slowsql saved", nr_slowsql_query (slow), "SELECT * FROM table WHERE constant = 31");
  tlib_pass_if_str_equal ("raw slowsql saved", nr_slowsql_params (slow), "{\"explain_plan\":[[\"a\",\"b\"],[[1,2],[3,4]]],\"backtrace\":[\"Zip\",\"Zap\"]}");

  nr_txn_destroy (&txn);
}

static void
test_web_transaction_insert (void)
{
  const char *tname = "web txn";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MySQL/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MySQL/select");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MySQL/table/select");
  nr_txn_destroy (&txn);
}

static void
test_options_tt_recordsql_obeyed_part0 (void)
{
  const char *tname = "NR_SQL_NONE";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  txn->options.ss_threshold = duration + 1;
  txn->options.ep_threshold = duration + 1;

  txn->options.tt_recordsql = NR_SQL_NONE;
  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MySQL/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MySQL/select");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MySQL/table/select");
  nr_txn_destroy (&txn);
}

static void
test_options_tt_recordsql_obeyed_part1 (void)
{
  const char *tname = "NR_SQL_RAW";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  txn->options.ss_threshold = duration + 1;
  txn->options.ep_threshold = duration + 1;

  txn->options.tt_recordsql = NR_SQL_RAW;
  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"sql\":\"SELECT * FROM table WHERE constant = 31\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MySQL/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MySQL/select");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MySQL/table/select");

  nr_txn_destroy (&txn);
}

static void
test_options_tt_recordsql_obeyed_part2 (void)
{
  const char *tname = "NR_SQL_OBFUSCATED";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  txn->options.ss_threshold = duration + 1;
  txn->options.ep_threshold = duration + 1;

  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;
  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MySQL/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "Datastore/operation/MySQL/select");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/statement/MySQL/table/select");

  nr_txn_destroy (&txn);
}

static void
test_options_high_security_tt_recordsql_raw (void)
{
  const char *tname = "high security ensures obfuscation";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  txn->options.ss_threshold = duration + 1;
  txn->options.ep_threshold = duration + 1;

  txn->high_security = 1;
  txn->options.tt_recordsql = NR_SQL_RAW;
  nr_txn_end_node_datastore (txn, &params, NULL);

  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\"}");

  nr_txn_destroy (&txn);
}

static void
test_stack_recorded (void)
{
  const char *tname = "stack recorded";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  txn->options.tt_recordsql = NR_SQL_NONE;
  txn->options.ss_threshold = 1;
  nr_txn_end_node_datastore (txn, &params, NULL);

  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"]}");

  nr_txn_destroy (&txn);
}

static void
test_missing_stack_callback (void)
{
  const char *tname = "backtrack callback is NULL";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  params.callbacks.backtrace = NULL;

  txn->options.tt_recordsql = NR_SQL_NONE;
  txn->options.ss_threshold = 1;
  nr_txn_end_node_datastore (txn, &params, NULL);

  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration, "{}");

  nr_txn_destroy (&txn);
}

static void
test_slowsql_raw_saved (void)
{
  const char *tname = "raw slowsql saved";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);
  const nr_slowsql_t *slow;

  txn->options.tt_recordsql = NR_SQL_RAW;
  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 1;
  txn->options.ss_threshold = 1;

  nr_txn_end_node_datastore (txn, &params, NULL);

  slow = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_uint32_t_equal (tname, nr_slowsql_id (slow), 3202261176);
  tlib_pass_if_int_equal (tname, nr_slowsql_count (slow), 1);
  tlib_pass_if_time_equal (tname, nr_slowsql_min (slow), 4000000);
  tlib_pass_if_time_equal (tname, nr_slowsql_max (slow), 4000000);
  tlib_pass_if_time_equal (tname, nr_slowsql_total (slow), 4000000);
  tlib_pass_if_str_equal (tname, nr_slowsql_metric (slow), "Datastore/statement/MySQL/table/select");
  tlib_pass_if_str_equal (tname, nr_slowsql_query (slow), "SELECT * FROM table WHERE constant = 31");
  tlib_pass_if_str_equal (tname, nr_slowsql_params (slow), "{\"backtrace\":[\"Zip\",\"Zap\"]}");
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],\"sql\":\"SELECT * FROM table WHERE constant = 31\"}");

  nr_txn_destroy (&txn);
}

static void
test_slowsql_obfuscated_saved (void)
{
  const char *tname = "obfuscated slowsql saved";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);
  const nr_slowsql_t *slow;

  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;
  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 1;

  nr_txn_end_node_datastore (txn, &params, NULL);

  slow = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_uint32_t_equal (tname, nr_slowsql_id (slow), 3202261176);
  tlib_pass_if_int_equal (tname, nr_slowsql_count (slow), 1);
  tlib_pass_if_time_equal (tname, nr_slowsql_min (slow), 4000000);
  tlib_pass_if_time_equal (tname, nr_slowsql_max (slow), 4000000);
  tlib_pass_if_time_equal (tname, nr_slowsql_total (slow), 4000000);
  tlib_pass_if_str_equal (tname, nr_slowsql_metric (slow), "Datastore/statement/MySQL/table/select");
  tlib_pass_if_str_equal (tname, nr_slowsql_query (slow), "SELECT * FROM table WHERE constant = ?");
  tlib_pass_if_str_equal (tname, nr_slowsql_params (slow), "{\"backtrace\":[\"Zip\",\"Zap\"]}");
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\"}");

  nr_txn_destroy (&txn);
}

static void
test_table_not_found (void)
{
  const char *tname = "table not found";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  params.sql.sql = "SELECT";

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/operation/MySQL/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],\"sql_obfuscated\":\"SELECT\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 2);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MySQL/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/operation/MySQL/select");
  nr_txn_destroy (&txn);
}

static void
test_table_and_operation_not_found (void)
{
  const char *tname = "table, operation not found";
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);

  params.sql.sql = "*";

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "Datastore/operation/MySQL/other", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],\"sql_obfuscated\":\"*\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 2);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/all");
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "Datastore/MySQL/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "Datastore/operation/MySQL/other");
  nr_txn_destroy (&txn);
}

static void
modify_table_name (char *tablename)
{
  if (0 == nr_strcmp (tablename, "fix_me")) {
    tablename[3] = '\0';
  }
}

nr_slowsqls_labelled_query_t sample_input_query = {
  .name  = "Doctrine DQL Query",
  .query = "SELECT COUNT(b) from Bot b where b.size = 23;",
};

static void
test_input_query_raw (void)
{
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);
  const nr_slowsql_t *slowsql;
  const char *tname = "raw input query";

  params.sql.input_query = &sample_input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;
  txn->options.tt_recordsql = NR_SQL_RAW;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_int_equal (tname, nr_slowsqls_saved (txn->slowsqls), 1);
  slowsql = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_str_equal (tname, nr_slowsql_params (slowsql),
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"input_query\":{"
       "\"label\":\"Doctrine DQL Query\","
       "\"query\":\"SELECT COUNT(b) from Bot b where b.size = 23;\"}}");
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
   "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"sql\":\"SELECT * FROM table WHERE constant = 31\","
     "\"input_query\":{"
       "\"label\":\"Doctrine DQL Query\","
       "\"query\":\"SELECT COUNT(b) from Bot b where b.size = 23;\"}}");
  nr_txn_destroy (&txn);
}

static void
test_input_query_empty (void)
{
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);
  const nr_slowsql_t *slowsql;
  const char *tname = "input query empty";
  nr_slowsqls_labelled_query_t input_query;

  input_query.name = "";
  input_query.query = "";
  params.sql.input_query = &input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_int_equal (tname, nr_slowsqls_saved (txn->slowsqls), 1);
  slowsql = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_str_equal (tname, nr_slowsql_params (slowsql),
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"input_query\":{"
       "\"label\":\"\","
       "\"query\":\"\"}}");
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\","
     "\"input_query\":{"
       "\"label\":\"\","
       "\"query\":\"\"}}");
  nr_txn_destroy (&txn);
}

static void
test_input_query_null_fields (void)
{
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);
  const nr_slowsql_t *slowsql;
  const char *tname = "input query NULL fields";
  nr_slowsqls_labelled_query_t input_query;

  input_query.name = NULL;
  input_query.query = NULL;
  params.sql.input_query = &input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_int_equal (tname, nr_slowsqls_saved (txn->slowsqls), 1);
  slowsql = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_str_equal (tname, nr_slowsql_params (slowsql),
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"input_query\":{"
       "\"label\":\"\","
       "\"query\":\"\"}}");
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\","
     "\"input_query\":{"
       "\"label\":\"\","
       "\"query\":\"\"}}");
  nr_txn_destroy (&txn);
}

static void
test_input_query_obfuscated (void)
{
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);
  const nr_slowsql_t *slowsql;
  const char *tname = "input query obfuscated";

  params.sql.input_query = &sample_input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_int_equal (tname, nr_slowsqls_saved (txn->slowsqls), 1);
  slowsql = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_str_equal (tname, nr_slowsql_params (slowsql),
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"input_query\":{"
       "\"label\":\"Doctrine DQL Query\","
       "\"query\":\"SELECT COUNT(b) from Bot b where b.size = ?;\"}}");
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\","
     "\"input_query\":{"
       "\"label\":\"Doctrine DQL Query\","
       "\"query\":\"SELECT COUNT(b) from Bot b where b.size = ?;\"}}");
  nr_txn_destroy (&txn);
}

static void
test_instance_info_present (void)
{
  nrtxn_t *txn = new_txn (0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_node_datastore_params_t params = sample_node_sql_params (duration);
  const nr_slowsql_t *slowsql;
  const char *tname = "instance info present";

  params.instance = nr_datastore_instance_create ("super_db_host", "3306", "my_database");
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;

  nr_txn_end_node_datastore (txn, &params, NULL);
  tlib_pass_if_int_equal (tname, nr_slowsqls_saved (txn->slowsqls), 1);
  slowsql = nr_slowsqls_at (txn->slowsqls, 0);
  tlib_pass_if_str_equal (tname, nr_slowsql_params (slowsql),
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"host\":\"super_db_host\","
     "\"port_path_or_id\":\"3306\","
     "\"database_name\":\"my_database\"}");
  test_one_node_populated (tname, txn, "Datastore/statement/MySQL/table/select", duration,
    "{\"backtrace\":[\"Zip\",\"Zap\"],"
     "\"host\":\"super_db_host\","
     "\"port_path_or_id\":\"3306\","
     "\"database_name\":\"my_database\","
     "\"sql_obfuscated\":\"SELECT * FROM table WHERE constant = ?\"}");
  nr_txn_destroy (&txn);
  nr_datastore_instance_destroy (&params.instance);
}

static void
test_get_operation_and_table (void)
{
  char *table;
  const char *operation = NULL;
  nrtxn_t txn;

  txn.special_flags.no_sql_parsing = 0;
  txn.special_flags.show_sql_parsing = 0;

  operation = NULL;
  table = nr_txn_node_sql_get_operation_and_table (NULL, &operation, "select * from my_table", &modify_table_name);
  tlib_pass_if_null ("null txn", table);
  tlib_pass_if_null ("null txn", operation);

  operation = NULL;
  txn.special_flags.no_sql_parsing = 1;
  table = nr_txn_node_sql_get_operation_and_table (&txn, &operation, "select * from my_table", &modify_table_name);
  tlib_pass_if_null ("no_sql_parsing", table);
  tlib_pass_if_null ("no_sql_parsing", operation);
  txn.special_flags.no_sql_parsing = 0;

  operation = NULL;
  table = nr_txn_node_sql_get_operation_and_table (&txn, &operation, "select * from my_table", &modify_table_name);
  tlib_pass_if_str_equal ("success", table, "my_table");
  tlib_pass_if_str_equal ("success", operation, "select");
  nr_free (table);

  operation = NULL;
  table = nr_txn_node_sql_get_operation_and_table (&txn, &operation, "select *", &modify_table_name);
  tlib_pass_if_null ("no table found", table);
  tlib_pass_if_str_equal ("no table found", operation, "select");
  nr_free (table);

  operation = NULL;
  table = nr_txn_node_sql_get_operation_and_table (&txn, &operation, "select * from fix_me", &modify_table_name);
  tlib_pass_if_str_equal ("table modified", table, "fix");
  tlib_pass_if_str_equal ("table modified", operation, "select");
  nr_free (table);

  operation = NULL;
  table = nr_txn_node_sql_get_operation_and_table (&txn, &operation, "select * from fix_me", NULL);
  tlib_pass_if_str_equal ("callback is NULL", table, "fix_me");
  tlib_pass_if_str_equal ("callback is NULL", operation, "select");
  nr_free (table);
}

static void
test_node_stack_worthy (void)
{
  int rv;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->options.ss_threshold = 0;
  txn->options.tt_slowsql = 0;
  txn->options.ep_threshold = 0;

  rv = nr_txn_node_datastore_stack_worthy (0, 0);
  tlib_pass_if_true ("zero params", 0 == rv, "rv=%d", rv);

  rv = nr_txn_node_datastore_stack_worthy (txn, 10);
  tlib_pass_if_true ("all options zero", 0 == rv, "rv=%d", rv);

  txn->options.ss_threshold = 5;
  rv = nr_txn_node_datastore_stack_worthy (txn, 10);
  tlib_pass_if_true ("above ss_threshold", 1 == rv, "rv=%d", rv);

  txn->options.ss_threshold = 15;
  rv = nr_txn_node_datastore_stack_worthy (txn, 10);
  tlib_pass_if_true ("below ss_threshold", 0 == rv, "rv=%d", rv);
  txn->options.ss_threshold = 0;

  txn->options.ep_threshold = 5;
  rv = nr_txn_node_datastore_stack_worthy (txn, 10);
  tlib_pass_if_true ("non-zero ep_threshold tt_slowsql disabled", 0 == rv, "rv=%d", rv);

  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 15;
  rv = nr_txn_node_datastore_stack_worthy (txn, 10);
  tlib_pass_if_true ("below ep_threshold", 0 == rv, "rv=%d", rv);

  txn->options.ep_threshold = 5;
  rv = nr_txn_node_datastore_stack_worthy (txn, 10);
  tlib_pass_if_true ("success", 1 == rv, "rv=%d", rv);
}

static void
test_node_potential_explain_plan (void)
{
  int rv;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->options.tt_slowsql = 1;
  txn->options.ep_enabled = 0;
  txn->options.ep_threshold = 15;
  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;

  rv = nr_txn_node_potential_explain_plan (NULL, 0);
  tlib_pass_if_int_equal ("NULL txn", 0, rv);

  rv = nr_txn_node_potential_explain_plan (txn, 20);
  tlib_pass_if_int_equal ("explain plan disabled", 0, rv);

  txn->options.ep_enabled = 1;

  rv = nr_txn_node_potential_explain_plan (txn, 10);
  tlib_pass_if_int_equal ("explain plan below threshold", 0, rv);

  rv = nr_txn_node_potential_explain_plan (txn, 20);
  tlib_fail_if_int_equal ("explain plan enabled", 0, rv);
}

static void
test_node_potential_slowsql (void)
{
  int rv;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->options.tt_slowsql = 0;
  txn->options.ep_threshold = 0;
  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;

  rv = nr_txn_node_potential_slowsql (0, 0);
  tlib_pass_if_true ("zero params", 0 == rv, "rv=%d", rv);

  rv = nr_txn_node_potential_slowsql (txn, 10);
  tlib_pass_if_true ("all options zero", 0 == rv, "rv=%d", rv);

  txn->options.ep_threshold = 5;
  rv = nr_txn_node_potential_slowsql (txn, 10);
  tlib_pass_if_true ("non-zero ep_threshold tt_slowsql disabled", 0 == rv, "rv=%d", rv);

  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 15;
  rv = nr_txn_node_potential_slowsql (txn, 10);
  tlib_pass_if_true ("below ep_threshold", 0 == rv, "rv=%d", rv);

  txn->options.ep_threshold = 5;
  rv = nr_txn_node_potential_slowsql (txn, 10);
  tlib_pass_if_true ("success", 1 == rv, "rv=%d", rv);

  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 5;
  txn->options.tt_recordsql = NR_SQL_NONE;
  rv = nr_txn_node_potential_slowsql (txn, 10);
  tlib_pass_if_true ("sql recording off", 0 == rv, "rv=%d", rv);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void
test_main (void *p NRUNUSED)
{
  /*
   * Tests of the main nr_txn_end_node_datastore() function.
   */
  test_bad_parameters ();
  test_table_and_operation ();
  test_no_table ();
  test_no_table_no_operation ();
  test_return_scoped_metric ();
  test_instance_info_with_data_hash ();
  test_instance_info_reporting_disabled ();
  test_instance_database_name_reporting_disabled ();
  test_instance_info_without_data_hash ();
  test_instance_info_empty ();
  test_instance_info_null ();
  test_instance_info_with_localhost ();
  test_instance_metric_with_slashes ();

  /*
   * Tests ported from the old node_sql tests.
   */
  test_explain_plan ();
  test_web_transaction_insert ();
  test_options_tt_recordsql_obeyed_part0 ();
  test_options_tt_recordsql_obeyed_part1 ();
  test_options_tt_recordsql_obeyed_part2 ();
  test_options_high_security_tt_recordsql_raw ();
  test_stack_recorded ();
  test_missing_stack_callback ();
  test_slowsql_raw_saved ();
  test_slowsql_obfuscated_saved ();
  test_table_not_found ();
  test_table_and_operation_not_found ();
  test_input_query_raw ();
  test_input_query_empty ();
  test_input_query_null_fields ();
  test_input_query_obfuscated ();
  test_instance_info_present ();

  /*
   * Tests for the helper functions within node_datastore.c.
   */
  test_get_operation_and_table ();
  test_node_stack_worthy ();
  test_node_potential_explain_plan ();
  test_node_potential_slowsql ();
}
