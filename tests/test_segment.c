#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "segment.h"
#include "transaction.h"
#include "test.h"

static void test_segment_set_parent(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_segment_t* a = newrelic_start_segment(txn, NULL, NULL);
  newrelic_segment_t* b = newrelic_start_segment(txn, NULL, NULL);

  assert_false(newrelic_set_segment_parent(NULL, NULL));
  assert_false(newrelic_set_segment_parent(a, NULL));
  assert_false(newrelic_set_segment_parent(NULL, b));
  assert_false(newrelic_set_segment_parent(a, a));

  assert_true(newrelic_set_segment_parent(a, b));
  assert_ptr_equal(b->segment, a->segment->parent);
  assert_ptr_equal(a->segment, b->segment->children.children[0]);

  newrelic_end_segment(txn, &a);
  newrelic_end_segment(txn, &b);
}

static void test_segment_set_timing(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_segment_t* segment = newrelic_start_segment(txn, NULL, NULL);
  nr_segment_t* txn_seg = segment->segment;

  assert_false(newrelic_set_segment_timing(NULL, 1, 2));

  txn->txn->root.start_time.when = 42;
  assert_true(newrelic_set_segment_timing(segment, 1, 2));
  assert_int_equal(43, txn_seg->start_time);
  assert_int_equal(45, txn_seg->stop_time);

  newrelic_end_segment(txn, &segment);

  /* Ending a segment with explicit times should keep the explicit times. */
  assert_int_equal(43, txn_seg->start_time);
  assert_int_equal(45, txn_seg->stop_time);
}

static void test_segment_validate_success(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("Should pass", "param"));
}

static void test_segment_validate_failure(void** state NRUNUSED) {
  assert_false(newrelic_validate_segment_param("Should/fail", "param"));
}

static void test_segment_validate_null_in(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param(NULL, "param"));
}

static void test_segment_validate_empty_in(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("", NULL));
}

static void test_segment_validate_null_name(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("Should pass", NULL));
}

static void test_segment_validate_empty_name(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("Should pass", ""));
}

int main(void) {
  const struct CMUnitTest segment_tests[] = {
      cmocka_unit_test(test_segment_set_parent),
      cmocka_unit_test(test_segment_set_timing),
      cmocka_unit_test(test_segment_validate_success),
      cmocka_unit_test(test_segment_validate_failure),
      cmocka_unit_test(test_segment_validate_null_in),
      cmocka_unit_test(test_segment_validate_empty_in),
      cmocka_unit_test(test_segment_validate_null_name),
      cmocka_unit_test(test_segment_validate_empty_name),
  };

  return cmocka_run_group_tests(segment_tests, txn_group_setup,
                                txn_group_teardown);
}
