
#include <stdarg.h>
#include <stddef.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "segment.h"
#include "util_memory.h"
#include "util_sleep.h"

#include "test.h"

/*
 * Purpose: Test that newrelic_start_segment() handles invalid inputs
 * correctly.
 */
static void test_start_segment_invalid(void** state NRUNUSED) {
  const char* name = "bob";

  assert_null(newrelic_start_segment(NULL, NULL, NULL));
  assert_null(newrelic_start_segment(NULL, NULL, name));
  assert_null(newrelic_start_segment(NULL, name, name));
  assert_null(newrelic_start_segment(NULL, name, NULL));
}

/*
 * Purpose: Test that newrelic_start_segment() handles NULL names
 * and categories correctly.
 */
static void test_start_segment_name_cat_null(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_segment_t* seg = newrelic_start_segment(txn, NULL, NULL);
  assert_string_equal("Unnamed Segment", seg->name);
  assert_string_equal("Custom", seg->category);
}

/*
 * Purpose: Test that newrelic_start_segment() stores the name
 * and the transaction.
 */
static void test_start_segment_name_cat_txn(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  const char* name = "bob";
  const char* category = "bee";
  nrtime_t* cur_kids_duration = txn->cur_kids_duration;

  newrelic_segment_t* seg = newrelic_start_segment(txn, name, category);
  assert_string_equal(name, seg->name);
  assert_ptr_not_equal(name, seg->name);
  assert_string_equal(category, seg->category);
  assert_ptr_not_equal(category, seg->category);
  assert_ptr_equal(txn, seg->transaction);

  assert_ptr_equal(seg->kids_duration_save, cur_kids_duration);
  assert_ptr_equal(txn->cur_kids_duration, &seg->kids_duration);
}

/*
 * Purpose: Test that newrelic_end_segment() handles invalid inputs
 * correctly.
 */
static void test_end_segment_invalid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_segment_t* seg = NULL;
  newrelic_txn_t othertxn;

  assert_null(newrelic_end_segment(NULL, NULL));
  assert_null(newrelic_end_segment(txn, NULL));

  assert_null(newrelic_end_segment(txn, &seg));
  seg = newrelic_start_segment(txn, NULL, NULL);
  assert_null(newrelic_end_segment(NULL, &seg));

  seg = newrelic_start_segment(txn, NULL, NULL);
  seg->transaction = NULL;
  assert_null(newrelic_end_segment(txn, &seg));

  seg = newrelic_start_segment(txn, NULL, NULL);
  seg->transaction = &othertxn;
  assert_null(newrelic_end_segment(txn, &seg));
}

/*
 * Purpose: Test that newrelic_end_segment() frees the segment.
 */
static void test_end_segment_free(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_segment_t* seg = newrelic_start_segment(txn, NULL, NULL);

  assert_non_null(newrelic_end_segment(txn, &seg));
  assert_null(seg);
}

/*
 * Purpose: Test that newrelic_end_segment() updates metrics
 * and trace nodes in the transaction.
 */
static void test_end_segment_metric_trace(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_segment_t* seg = newrelic_start_segment(txn, NULL, NULL);
  nrtxnnode_t* node = txn->last_added;

  assert_non_null(newrelic_end_segment(txn, &seg));
  assert_int_equal(1, nrm_table_size(txn->scoped_metrics));
  assert_int_equal(1, nrm_table_size(txn->unscoped_metrics));
  assert_ptr_not_equal(node, txn->last_added);
}

/*
 * Purpose: Test that newrelic_end_segment() updates duration
 * in the transaction.
 */
static void test_end_segment_duration(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_segment_t* seg = newrelic_start_segment(txn, NULL, NULL);
  nrtime_t duration = *(txn->cur_kids_duration);

  nr_msleep(5); /* To see a change in duration */

  assert_non_null(newrelic_end_segment(txn, &seg));
  assert_int_not_equal(duration, *(txn->cur_kids_duration));
}

/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  const struct CMUnitTest segment_tests[] = {
      cmocka_unit_test(test_start_segment_invalid),
      cmocka_unit_test(test_start_segment_name_cat_null),
      cmocka_unit_test(test_start_segment_name_cat_txn),
      cmocka_unit_test(test_end_segment_invalid),
      cmocka_unit_test(test_end_segment_free),
      cmocka_unit_test(test_end_segment_metric_trace),
      cmocka_unit_test(test_end_segment_duration),
  };

  return cmocka_run_group_tests(segment_tests, txn_group_setup,
                                txn_group_teardown);
}
