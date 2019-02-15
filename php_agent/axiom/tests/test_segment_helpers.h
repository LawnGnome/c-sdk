/*
 * This file contains support for segment tests.
 */
#ifndef TEST_SEGMENT_HELPERS_HDR
#define TEST_SEGMENT_HELPERS_HDR

#include "nr_segment.h"
#include "tlib_main.h"
#include "util_metrics_private.h"
#include "nr_txn.h"
#include "nr_txn_private.h"

#define test_txn_untouched(X1, X2) \
  test_txn_untouched_fn((X1), (X2), __FILE__, __LINE__)
#define test_metric_created(...) \
  test_segment_helper_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_metric_table_size(...) \
  test_metric_table_size_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_segment_metric_created(...) \
  test_segment_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_txn_metric_created(...) \
  test_txn_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)

/*
 * These functions are all marked as unused, as they likely won't all be
 * exercised by a single set of unit tests.
 */

static NRUNUSED void test_txn_untouched_fn(const char* testname,
                                           const nrtxn_t* txn,
                                           const char* file,
                                           int line) {
  test_pass_if_true(testname, 0 == txn->root_kids_duration,
                    "txn->root_kids_duration=" NR_TIME_FMT,
                    txn->root_kids_duration);
  test_pass_if_true(testname, 0 == nrm_table_size(txn->scoped_metrics),
                    "nrm_table_size (txn->scoped_metrics)=%d",
                    nrm_table_size(txn->scoped_metrics));
  test_pass_if_true(testname, 0 == nrm_table_size(txn->unscoped_metrics),
                    "nrm_table_size (txn->unscoped_metrics)=%d",
                    nrm_table_size(txn->unscoped_metrics));
  test_pass_if_true(testname, 0 == txn->nodes_used, "txn->nodes_used=%d",
                    txn->nodes_used);
}

static NRUNUSED void test_metric_vector_size(const nr_vector_t* metrics,
                                             size_t expected_size) {
  size_t actual_size = nr_vector_size(metrics);
  tlib_pass_if_size_t_equal("metric vector size", expected_size, actual_size);
}

static NRUNUSED void test_segment_metric_created_fn(const char* testname,
                                                    nr_vector_t* metrics,
                                                    const char* metric_name,
                                                    bool scoped,
                                                    const char* file,
                                                    int line) {
  bool passed = false;
  for (size_t i = 0; i < nr_vector_size(metrics); i++) {
    nr_segment_metric_t* sm = nr_vector_get(metrics, i);
    if (0 == nr_strcmp(metric_name, sm->name) && (scoped == sm->scoped)) {
      passed = true;
      break;
    }
  }

  test_pass_if_true(testname, passed, "metric %s (scoped %d) not created",
                    metric_name, scoped);
}

static NRUNUSED void test_txn_metric_created_fn(const char* testname,
                                                nrmtable_t* metrics,
                                                const char* expected,
                                                const char* file,
                                                int line) {
  test_pass_if_true(testname, NULL != nrm_find(metrics, expected),
                    "expected=%s", expected);
}

static NRUNUSED void test_metric_table_size_fn(const char* testname,
                                               const nrmtable_t* metrics,
                                               int expected_size,
                                               const char* file,
                                               int line) {
  int actual_size = nrm_table_size(metrics);

  test_pass_if_true(testname, expected_size == actual_size,
                    "expected_size=%d actual_size=%d", expected_size,
                    actual_size);
}

static NRUNUSED void test_segment_helper_metric_created_fn(const char* testname,
                                                           nrmtable_t* metrics,
                                                           uint32_t flags,
                                                           nrtime_t duration,
                                                           const char* name,
                                                           const char* file,
                                                           int line) {
  const nrmetric_t* m = nrm_find(metrics, name);
  const char* nm = nrm_get_name(metrics, m);

  test_pass_if_true(testname, 0 != m, "m=%p", m);
  test_pass_if_true(testname, 0 == nr_strcmp(nm, name), "nm=%s name=%s",
                    NRSAFESTR(nm), NRSAFESTR(name));

  if (0 != m) {
    test_pass_if_true(testname, flags == m->flags,
                      "name=%s flags=%u m->flags=%u", name, flags, m->flags);
    test_pass_if_true(testname, nrm_count(m) == 1,
                      "name=%s nrm_count (m)=" NR_TIME_FMT, name, nrm_count(m));
    test_pass_if_true(testname, nrm_total(m) == duration,
                      "name=%s nrm_total (m)=" NR_TIME_FMT
                      " duration=" NR_TIME_FMT,
                      name, nrm_total(m), duration);
    test_pass_if_true(testname, nrm_exclusive(m) == duration,
                      "name=%s nrm_exclusive (m)=" NR_TIME_FMT
                      " duration=" NR_TIME_FMT,
                      name, nrm_exclusive(m), duration);
    test_pass_if_true(testname, nrm_min(m) == duration,
                      "name=%s nrm_min (m)=" NR_TIME_FMT
                      " duration=" NR_TIME_FMT,
                      name, nrm_min(m), duration);
    test_pass_if_true(testname, nrm_max(m) == duration,
                      "name=%s nrm_max (m)=" NR_TIME_FMT
                      " duration=" NR_TIME_FMT,
                      name, nrm_max(m), duration);
    test_pass_if_true(testname, nrm_sumsquares(m) == (duration * duration),
                      "name=%s nrm_sumsquares (m)=" NR_TIME_FMT
                      " duration=" NR_TIME_FMT,
                      name, nrm_sumsquares(m), duration);
  }
}

static NRUNUSED nrtxn_t* new_txn(int background) {
  nrapp_t app;
  nrtxn_t* txn;

  nr_memset(&app, 0, sizeof(app));

  app.info.high_security = 0;
  app.state = NR_APP_OK;
  app.connect_reply = nro_create_from_json(
      "{\"collect_traces\":true,\"collect_errors\":true}");
  app.info.license = nr_strdup("0123456789012345678901234567890123456789");
  app.rnd = NULL;

  txn = nr_txn_begin(&app, &nr_txn_test_options, 0);
  if (0 == txn) {
    return txn;
  }

  txn->root.start_time.when
      = 0; /* So that we can have arbitrary node start times */

  nr_free(app.info.license);
  nro_delete(app.connect_reply);

  if (background) {
    nr_txn_set_as_background_job(txn, 0);
  }

  /*
   * Clear the metric tables to easily test if new metrics have been created.
   */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  nrm_table_destroy(&txn->scoped_metrics);
  txn->scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  return txn;
}

#endif /* TEST_SEGMENT_HELPERS_HDR */
