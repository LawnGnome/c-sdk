#include "nr_axiom.h"

#include "nr_txn.h"
#include "nr_txn_private.h"
#include "util_memory.h"
#include "util_metrics_private.h"
#include "util_sleep.h"
#include "util_strings.h"

#include "tlib_main.h"
#include "test_node_helpers.h"

void
test_metric_table_size_fn (const char *testname, const nrmtable_t *metrics, int expected_size, const char *file, int line)
{
  int actual_size = nrm_table_size (metrics);

  test_pass_if_true (testname, expected_size == actual_size, "expected_size=%d actual_size=%d",
    expected_size, actual_size);
}

void
test_txn_untouched_fn (const char *testname, const nrtxn_t *txn, const char *file, int line)
{
  test_pass_if_true (testname, 0 == txn->root_kids_duration, "txn->root_kids_duration=" NR_TIME_FMT, txn->root_kids_duration);
  test_pass_if_true (testname, 0 == nrm_table_size (txn->scoped_metrics),
    "nrm_table_size (txn->scoped_metrics)=%d", nrm_table_size (txn->scoped_metrics));
  test_pass_if_true (testname, 0 == nrm_table_size (txn->unscoped_metrics),
    "nrm_table_size (txn->unscoped_metrics)=%d", nrm_table_size (txn->unscoped_metrics));
  test_pass_if_true (testname, 0 == txn->nodes_used, "txn->nodes_used=%d", txn->nodes_used);
}

void
test_node_helper_metric_created_fn (
  const char *testname,
  nrmtable_t *metrics,
  uint32_t flags,
  nrtime_t duration,
  const char *name,
  const char *file,
  int line)
{
  const nrmetric_t *m = nrm_find (metrics, name);
  const char *nm = nrm_get_name (metrics, m);

  test_pass_if_true (testname, 0 != m, "m=%p", m);
  test_pass_if_true (testname, 0 == nr_strcmp (nm, name), "nm=%s name=%s", NRSAFESTR (nm), NRSAFESTR (name));

    if (0 != m) {
    test_pass_if_true (testname, flags == m->flags,
      "name=%s flags=%u m->flags=%u", name, flags, m->flags);
    test_pass_if_true (testname, nrm_count (m) == 1,
      "name=%s nrm_count (m)=" NR_TIME_FMT, name, nrm_count (m));
    test_pass_if_true (testname, nrm_total (m) == duration,
      "name=%s nrm_total (m)=" NR_TIME_FMT " duration=" NR_TIME_FMT, name, nrm_total (m), duration);
    test_pass_if_true (testname, nrm_exclusive (m) == duration,
      "name=%s nrm_exclusive (m)=" NR_TIME_FMT " duration=" NR_TIME_FMT, name, nrm_exclusive (m), duration);
    test_pass_if_true (testname, nrm_min (m) == duration,
      "name=%s nrm_min (m)=" NR_TIME_FMT " duration=" NR_TIME_FMT, name, nrm_min (m), duration);
    test_pass_if_true (testname, nrm_max (m) == duration,
      "name=%s nrm_max (m)=" NR_TIME_FMT " duration=" NR_TIME_FMT, name, nrm_max (m), duration);
    test_pass_if_true (testname, nrm_sumsquares (m) == (duration * duration),
      "name=%s nrm_sumsquares (m)=" NR_TIME_FMT " duration=" NR_TIME_FMT, name, nrm_sumsquares (m), duration);
  }
}

void
test_one_node_populated_fn (
  const char *testname,
  const nrtxn_t *txn,
  const char *expected_name,
  nrtime_t duration,
  const char *expected_data_hash,
  const char *file,
  int line)
{
  const nrtxnnode_t *n;
  const char *name;

  test_pass_if_true (testname, 1 == txn->nodes_used, "txn->nodes_used=%d", txn->nodes_used);

  if (1 != txn->nodes_used) {
    return;
  }

  n = txn->nodes;

  test_pass_if_true (testname, n->stop_time.when > n->start_time.when,
    "n->stop_time.when=" NR_TIME_FMT "n->start_time.when=" NR_TIME_FMT, n->stop_time.when, n->start_time.when);
  test_pass_if_true (testname, n->stop_time.stamp > n->start_time.stamp,
    "n->stop_time.stamp=%d n->start_time.stamp=%d", n->stop_time.stamp, n->start_time.stamp);

  test_pass_if_true (testname, duration == (n->stop_time.when - n->start_time.when),
    "duration= " NR_TIME_FMT "n->stop_time.when " NR_TIME_FMT "n->start_time.when " NR_TIME_FMT,
    duration, n->stop_time.when, n->start_time.when);

  test_pass_if_true (testname, 0 == n->count, "n->count=%d", n->count);

  name = nr_string_get (txn->trace_strings, n->name);
  test_pass_if_true (testname, 0 == nr_strcmp (expected_name, name),
    "expected_name=%s name=%s", NRSAFESTR (expected_name), NRSAFESTR (name));

  if (NULL == expected_data_hash) {
    test_pass_if_true (testname, NULL == n->data_hash, "data_hash=%p", n->data_hash);
  } else {
    char *data_hash = nro_to_json (n->data_hash);

    test_pass_if_true (testname, 0 == nr_strcmp (data_hash, expected_data_hash),
      "data_hash=%s expected=%s",
      NRSAFESTR (data_hash),
      NRSAFESTR (expected_data_hash));

    nr_free (data_hash);
  }
}

nrtxn_t *
new_txn (int background)
{
  nrapp_t app;
  nrtxn_t *txn;

  nr_memset (&app, 0, sizeof (app));

  app.info.high_security = 0;
  app.state = NR_APP_OK;
  app.connect_reply = nro_create_from_json ("{\"collect_traces\":true,\"collect_errors\":true}");
  app.info.license = nr_strdup ("0123456789012345678901234567890123456789");
  app.rnd = NULL;

  txn = nr_txn_begin (&app, &nr_txn_test_options, 0);
  if (0 == txn) {
    return txn;
  }

  txn->root.start_time.when = 0; /* So that we can have arbitrary node start times */

  nr_free (app.info.license);
  nro_delete (app.connect_reply);

  if (background) {
    nr_txn_set_as_background_job (txn, 0);
  }

  /*
   * Clear the metric tables to easily test if new metrics have been created.
   */
  nrm_table_destroy (&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create (NR_METRIC_DEFAULT_LIMIT);
  nrm_table_destroy (&txn->scoped_metrics);
  txn->scoped_metrics = nrm_table_create (NR_METRIC_DEFAULT_LIMIT);

  return txn;
}
