#include <stdarg.h>
#include <stddef.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "external.h"
#include "util_memory.h"

#include "test.h"

static newrelic_external_segment_t* duplicate_external_segment(const newrelic_external_segment_t* in) {
  newrelic_external_segment_t* out = nr_zalloc(sizeof(newrelic_external_segment_t));

  out->txn              = in->txn;
  out->params.start     = in->params.start;
  out->params.stop      = in->params.stop;
  out->params.library   = in->params.library   ? nr_strdup(in->params.library)   : NULL;
  out->params.procedure = in->params.procedure ? nr_strdup(in->params.procedure) : NULL;
  out->params.url       = in->params.url       ? nr_strdup(in->params.url)       : NULL;

  return out;
}

/* Declare prototypes for mocks. */
void __wrap_nr_txn_end_node_external(nrtxn_t* txn, const nr_node_external_params_t* params);

/*
 * Purpose: Mock to validate that appropriate values are passed into
 * nr_txn_end_node_external().
 */
void __wrap_nr_txn_end_node_external(nrtxn_t* txn, const nr_node_external_params_t* params) {
  check_expected(txn);
  check_expected(params);
}

/*
 * Purpose: Test that newrelic_start_external_segment() handles invalid inputs
 * correctly.
 */
static void test_start_external_segment_invalid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*) *state;
  newrelic_external_segment_params_t params = {
    .uri = NULL,
  };

  assert_null(newrelic_start_external_segment(NULL, NULL));
  assert_null(newrelic_start_external_segment(txn, NULL));
  assert_null(newrelic_start_external_segment(NULL, &params));

  /* A NULL uri should also result in a NULL. */
  assert_null(newrelic_start_external_segment(txn, &params));

  /* Now we'll test library and procedure. */
  params.uri = "https://newrelic.com/";

  params.library = "foo/bar";
  assert_null(newrelic_start_external_segment(txn, &params));
  params.library = NULL;

  params.procedure = "foo/bar";
  assert_null(newrelic_start_external_segment(txn, &params));
  params.procedure = NULL;
}

/*
 * Purpose: Test that newrelic_start_external_segment() handles valid inputs
 * correctly.
 */
static void test_start_external_segment_valid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*) *state;
  newrelic_external_segment_params_t params = {
    .uri = "https://newrelic.com/",
  };
  newrelic_external_segment_t* segment;

  /* Test without library or procedure. */
  segment = newrelic_start_external_segment(txn, &params);
  assert_non_null(segment);
  assert_ptr_equal(txn, segment->txn);
  assert_int_not_equal(0, segment->params.start.when);

  /* Ensure the uri was actually copied. */
  assert_string_equal(params.uri, segment->params.url);
  assert_ptr_not_equal(params.uri, segment->params.url);
  assert_null(segment->params.library);
  assert_null(segment->params.procedure);

  newrelic_destroy_external_segment(&segment);

  /* Now test with library and procedure. */
  params.library = "curl";
  params.procedure = "GET";
  segment = newrelic_start_external_segment(txn, &params);
  assert_non_null(segment);
  assert_ptr_equal(txn, segment->txn);
  assert_int_not_equal(0, segment->params.start.when);
  assert_string_equal(params.uri, segment->params.url);
  assert_ptr_not_equal(params.uri, segment->params.url);
  assert_string_equal(params.library, segment->params.library);
  assert_ptr_not_equal(params.library, segment->params.library);
  assert_string_equal(params.procedure, segment->params.procedure);
  assert_ptr_not_equal(params.procedure, segment->params.procedure);

  newrelic_destroy_external_segment(&segment);
}

/*
 * Purpose: Test that newrelic_end_external_segment() handles invalid inputs
 * correctly.
 */
static void test_end_external_segment_invalid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*) *state;
  newrelic_external_segment_t segment = {
    .txn = txn,
    .params = {
      .start = { .stamp = 1, .when = 1 },
      .url = "https://httpbin.org/",
    },
  };
  newrelic_external_segment_t* segment_ptr = duplicate_external_segment(&segment);

  assert_false(newrelic_end_external_segment(NULL, NULL));
  assert_false(newrelic_end_external_segment(txn, NULL));

  /* This should destroy the given segment. */
  assert_false(newrelic_end_external_segment(NULL, &segment_ptr));
  assert_null(segment_ptr);

  /* A different transaction should result in failure. */
  segment_ptr = duplicate_external_segment(&segment);
  assert_false(newrelic_end_external_segment(txn + 1, &segment_ptr));
  assert_null(segment_ptr);
}

/*
 * Purpose: Test that newrelic_end_external_segment() handles valid inputs
 * correctly.
 */
static void test_end_external_segment_valid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*) *state;
  newrelic_external_segment_t segment = {
    .txn = txn,
    .params = {
      .start = { .stamp = 1, .when = 1 },
      .url = "https://httpbin.org/",
    },
  };
  newrelic_external_segment_t* segment_ptr = duplicate_external_segment(&segment);

  expect_value(__wrap_nr_txn_end_node_external, txn, txn);
  expect_value(__wrap_nr_txn_end_node_external, params, &segment_ptr->params);
  assert_true(newrelic_end_external_segment(txn, &segment_ptr));
  assert_null(segment_ptr);
}

/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  const struct CMUnitTest external_tests[] = {
      cmocka_unit_test(test_start_external_segment_invalid),
      cmocka_unit_test(test_start_external_segment_valid),
      cmocka_unit_test(test_end_external_segment_invalid),
      cmocka_unit_test(test_end_external_segment_valid),
  };

  return cmocka_run_group_tests(external_tests,  // our tests
                                txn_group_setup, txn_group_teardown);
}
