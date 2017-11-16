#include "nr_axiom.h"

#include "node_external.h"
#include "nr_header.h"
#include "nr_txn.h"
#include "util_memory.h"

#include "tlib_main.h"
#include "test_node_helpers.h"

nrapp_t *
nr_app_verify_id (nrapplist_t *applist NRUNUSED, const char *agent_run_id NRUNUSED)
{
  return 0;
}

void
nr_header_outbound_response (nrtxn_t *txn,
                             const char *encoded_response,
                             char **external_id_ptr, char **external_txnname_ptr,
                             char **external_guid_ptr)
{
  nrobj_t *obj;
  const char *external_id;
  const char *external_txnname;
  const char *external_guid;

  tlib_pass_if_not_null ("txn present", txn);
  tlib_pass_if_not_null ("encoded_response present", encoded_response);
  tlib_pass_if_not_null ("external_id_ptr present", external_id_ptr);
  tlib_pass_if_not_null ("external_txnname_ptr present", external_txnname_ptr);
  tlib_pass_if_not_null ("external_guid_ptr present", external_guid_ptr);

  obj = nro_create_from_json (encoded_response);
  external_id = nro_get_hash_string (obj, "id", 0);
  external_txnname = nro_get_hash_string (obj, "txnname", 0);
  external_guid = nro_get_hash_string (obj, "guid", 0);

  if (external_id && external_id_ptr) {
    *external_id_ptr = nr_strdup (external_id);
  }
  if (external_txnname && external_txnname_ptr) {
    *external_txnname_ptr = nr_strdup (external_txnname);
  }
  if (external_guid && external_guid_ptr) {
    *external_guid_ptr = nr_strdup (external_guid);
  }

  nro_delete (obj);
}

#define STRLEN(X) (X), (sizeof (X) - 1)

static void
test_node_rollup (void)
{
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;
  nrtxnnode_t rollup;
  nrtxntime_t stop;
  nrtxntime_t start;
  nr_status_t rv;
  int rollup_name_idx;

  rollup.stop_time.when = 0;
  txn->trace_strings = nr_string_pool_create ();

  rollup.count = 0;
  rollup.start_time.stamp = 7;
  rollup.stop_time.stamp = 8;
  start.stamp = 9;
  stop.stamp = 10;
  txn->last_added = &rollup;
  rollup_name_idx = nr_string_add (txn->trace_strings, "myname");
  rollup.name = rollup_name_idx;

  start.when = 400;
  stop.when = 500;
  rollup.start_time.when = 100;

  /*
   * Test : Bad Parameters
   */
  rv = nr_txn_node_rollup (0, 0, 0, 0);
  tlib_pass_if_true ("zero params", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_txn_node_rollup (0, &start, &stop, "myname");
  tlib_pass_if_true ("no txn", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_txn_node_rollup (txn, 0, &stop, "myname");
  tlib_pass_if_true ("no start", (NR_FAILURE == rv) && (0 == rollup.count), "rv=%d rollup.count=%d", (int)rv, rollup.count);

  rv = nr_txn_node_rollup (txn, &start, 0, "myname");
  tlib_pass_if_true ("no stop", (NR_FAILURE == rv) && (0 == rollup.count), "rv=%d rollup.count=%d", (int)rv, rollup.count);

  /*
   * Test : No Last Added
   */
  txn->last_added = 0;
  rv = nr_txn_node_rollup (txn, &start, &stop, "myname");
  tlib_pass_if_true ("no last added", (NR_FAILURE == rv) && (0 == rollup.count), "rv=%d rollup.count=%d", (int)rv, rollup.count);
  txn->last_added = &rollup;

  /*
   * Test : Rollup Has Wrong Metric
   */
  rollup.name = rollup_name_idx + 1;
  rv = nr_txn_node_rollup (txn, &start, &stop, "myname");
  tlib_pass_if_true ("no last added", (NR_FAILURE == rv) && (0 == rollup.count), "rv=%d rollup.count=%d", (int)rv, rollup.count);
  rollup.name = rollup_name_idx;

  /*
   * Test : Rollup Node Has Kids (Non-Consecutive Stamps)
   */
  rollup.start_time.stamp = 6;
  rv = nr_txn_node_rollup (txn, &start, &stop, "myname");
  tlib_pass_if_true ("rollup has kids", (NR_FAILURE == rv) && (0 == rollup.count), "rv=%d rollup.count=%d", (int)rv, rollup.count);
  rollup.start_time.stamp = 7;

  /*
   * Test : Success!
   */
  rv = nr_txn_node_rollup (txn, &start, &stop, "myname");
  tlib_pass_if_true ("node rollup success", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("node rollup count", 1 == rollup.count, "rollup.count=%d", rollup.count);
  tlib_pass_if_true ("node rollup start stamp", 9 == rollup.start_time.stamp,
    "rollup.start_time.stamp=%d", rollup.start_time.stamp);
  tlib_pass_if_true ("node rollup stop stamp", 10 == rollup.stop_time.stamp,
    "rollup.stop_time.stamp=%d", rollup.stop_time.stamp);
  tlib_pass_if_true ("node rollup start time", 100 == rollup.start_time.when,
    "rollup.start_time.when=" NR_TIME_FMT, rollup.start_time.when);
  tlib_pass_if_true ("node rollup stop time", 500 == rollup.stop_time.when,
    "rollup.stop_time.when=" NR_TIME_FMT, rollup.stop_time.when);

  nr_string_pool_destroy (&txn->trace_strings);
}

static void
test_bad_parameters (void)
{
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  const char *async_context = NULL;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (0, 0, 0, 0, 0, 0, 0, 0);
  nr_txn_do_end_node_external (0, async_context, &start, &stop, STRLEN ("newrelic.com"), 0, 0);

  nr_txn_do_end_node_external (txn, async_context, 0, &stop, STRLEN ("newrelic.com"), 0, 0);
  test_txn_untouched ("no start", txn);

  nr_txn_do_end_node_external (txn, async_context, &start, NULL, STRLEN ("newrelic.com"), 0, 0);
  test_txn_untouched ("no start", txn);

  txn->status.recording = 0;
  nr_txn_do_end_node_external (txn, async_context, &start, &stop, STRLEN ("newrelic.com"), 0, 0);
  test_txn_untouched ("not recording", txn);
  txn->status.recording = 1;

  txn->root.start_time.when = 2 * nr_get_time ();
  nr_txn_do_end_node_external (txn, async_context, &start, &stop, STRLEN ("newrelic.com"), 0, 0);
  test_txn_untouched ("future txn start time", txn);
  txn->root.start_time.when = 0;

  start.stamp = stop.stamp + 1;
  nr_txn_do_end_node_external (txn, async_context, &start, &stop, STRLEN ("newrelic.com"), 0, 0);
  test_txn_untouched ("future node start stamp", txn);
  start.stamp = stop.stamp - 1;

  nr_txn_destroy (&txn);
}

static void
test_web_transaction (void)
{
  const char *tname = "web txn";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("newrelic.com"), 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/newrelic.com/all", duration,
    "{\"uri\":\"newrelic.com\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/newrelic.com/all");
  nr_txn_destroy (&txn);
}

static void
test_null_url (void)
{
  const char *tname = "zero url";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, 0, 12, 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/<unknown>/all", duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/<unknown>/all");
  nr_txn_destroy (&txn);
}

static void
test_empty_url (void)
{
  const char *tname = "empty url";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, "", 12, 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/<unknown>/all", duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/<unknown>/all");
  nr_txn_destroy (&txn);
}

static void
test_zero_urllen (void)
{
  const char *tname = "zero urllen";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, "newrelic.com", 0, 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/<unknown>/all", duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/<unknown>/all");
  nr_txn_destroy (&txn);
}

static void
test_negative_urllen (void)
{
  const char *tname = "negative urllen";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, "newrelic.com", -1, 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/<unknown>/all", duration, "{}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/<unknown>/all");
  nr_txn_destroy (&txn);
}

static void
test_domain_parsing_fails (void)
{
  const char *tname = "domain parsing fails";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, "@@@@@", 5, 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/<unknown>/all", duration, "{\"uri\":\"\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/<unknown>/all");
  nr_txn_destroy (&txn);
}


/*
 * TODO
 * PHP-694 The agent creates metrics containing free-form text, which
 * can lead to sensitive customer data being sent to the collector.
 */
static void
test_junk_url (void)
{
  const char *tname = "junk url";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("this is a junk url"), 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/this is a junk url/all", duration,
    "{\"uri\":\"this is a junk url\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/this is a junk url/all");
  nr_txn_destroy (&txn);
}

static void
test_url_saving_strips_parameters (void)
{
  const char *tname = "strip external url parameters";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, "http://newrelic.com?secret=shhhhhh", 34, 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/newrelic.com/all", duration,
    "{\"uri\":\"http:\\/\\/newrelic.com\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/newrelic.com/all");
  nr_txn_destroy (&txn);
}

static void
test_node_rollup_prevention (void)
{
  /*
   * Test : Node rollup creates same metrics and prevents a new node from
   *        being saved.
   */
  nrtxn_t *txn = new_txn (0);
  const char *tname = "node_rollup_prevention";
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("newrelic.com"), 0, 0);
  txn->root_kids_duration = 0;
  start.stamp = 3;
  stop.stamp = 4;
  start.when = 3 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;
  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("newrelic.com"), 1, 0);
  tlib_pass_if_true ("node rollup prevents new node", 1 == txn->nodes_used, "txn->nodes_used=%d", txn->nodes_used);
  tlib_pass_if_true ("node rollup increases count", 1 == txn->nodes[0].count, "txn->nodes[0].count=%d", txn->nodes[0].count);
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  nr_txn_destroy (&txn);
}

static void
test_only_external_id (void)
{
  /* No cross process metrics here: Both external_id and external_txnname are needed. */
  const char *tname = "only_external_id";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("newrelic.com"), 0, "{\"id\":\"12345#6789\"}");
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/newrelic.com/all", duration,
    "{\"uri\":\"newrelic.com\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/newrelic.com/all");
  nr_txn_destroy (&txn);
}

static void
test_only_external_txnname (void)
{
  /* No cross process metrics here: Both external_id and external_txnname are needed. */
  const char *tname = "only_external_txnname";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("newrelic.com"), 0, "{\"txnname\":\"my_txn\"}");
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "External/newrelic.com/all", duration,
    "{\"uri\":\"newrelic.com\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "External/newrelic.com/all");
  nr_txn_destroy (&txn);
}

static void
test_external_id_and_txnname (void)
{
  const char *tname = "cross process metrics";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("newrelic.com"), 0, "{\"id\":\"12345#6789\",\"txnname\":\"my_txn\"}");
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "ExternalTransaction/newrelic.com/12345#6789/my_txn", duration,
    "{\"uri\":\"newrelic.com\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "External/newrelic.com/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "ExternalApp/newrelic.com/12345#6789/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "ExternalTransaction/newrelic.com/12345#6789/my_txn");
  nr_txn_destroy (&txn);
}

static void
test_external_id_txnname_and_guid (void)
{
  const char *tname = "external_id_txnname_and_guid";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, NULL, &start, &stop, STRLEN ("newrelic.com"), 0, "{\"id\":\"12345#6789\",\"txnname\":\"my_txn\",\"guid\":\"0123456789ABCDEF\"}");
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, duration);
  test_one_node_populated (tname, txn, "ExternalTransaction/newrelic.com/12345#6789/my_txn", duration,
    "{\"transaction_guid\":\"0123456789ABCDEF\",\"uri\":\"newrelic.com\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 3);
  test_metric_created (tname, txn->unscoped_metrics, MET_FORCED, duration, "External/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "External/newrelic.com/all");
  test_metric_created (tname, txn->unscoped_metrics, 0,          duration, "ExternalApp/newrelic.com/12345#6789/all");
  test_metric_created (tname, txn->scoped_metrics,   0,          duration, "ExternalTransaction/newrelic.com/12345#6789/my_txn");
  nr_txn_destroy (&txn);
}

static void
test_async_external_call (void)
{
  const char *tname = "async external call";
  nrtxn_t *txn = new_txn (0);
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration = 4 * NR_TIME_DIVISOR;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 1 * NR_TIME_DIVISOR;
  stop.when = start.when + duration;

  nr_txn_do_end_node_external (txn, "async_context", &start, &stop, STRLEN ("newrelic.com"), 0, 0);
  tlib_pass_if_time_equal (tname, txn->root_kids_duration, 0);
  test_one_node_populated (tname, txn, "External/newrelic.com/all", duration,
    "{\"uri\":\"newrelic.com\"}");
  test_metric_table_size (tname, txn->scoped_metrics, 1);
  test_metric_table_size (tname, txn->unscoped_metrics, 1);
  test_metric_created (tname, txn->scoped_metrics, 0, duration, "External/newrelic.com/all");
  nr_txn_destroy (&txn);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void
test_main (void *p NRUNUSED)
{
  test_node_rollup ();
  test_bad_parameters ();
  test_web_transaction ();
  test_null_url ();
  test_empty_url ();
  test_zero_urllen ();
  test_negative_urllen ();
  test_domain_parsing_fails ();
  test_junk_url ();
  test_url_saving_strips_parameters ();
  test_node_rollup ();
  test_node_rollup_prevention ();
  test_only_external_id ();
  test_only_external_txnname ();
  test_external_id_and_txnname ();
  test_external_id_txnname_and_guid ();
  test_async_external_call ();
}
