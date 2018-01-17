#include <stdarg.h>
#include <stddef.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "config.h"

#include "nr_txn.h"
#include "test.h"
#include "util_memory.h"

#define LICENSE_KEY ("Thisisafortycharacterkeyabcdefghijklmnop")

/*
 * Purpose: Test that affirms the transaction options returned without a
 *          newrelic_config_t are the same as the default options.
 */
static void test_get_transaction_options_default(void** state NRUNUSED) {
  nrtxnopt_t* actual = newrelic_get_transaction_options(NULL);
  nrtxnopt_t* expected = newrelic_get_default_options();

  /* Assert that the options were set accordingly.
   */
  assert_true(nr_txn_cmp_options(actual, expected));

  nr_free(actual);
  nr_free(expected);
}

/*
 * Purpose: Test that affirms the transaction options with the transaction
 *          tracer disabled are correct.
 */
static void test_get_transaction_options_tt_disabled(void** state NRUNUSED) {
  nrtxnopt_t* actual;
  nrtxnopt_t* expected;
  newrelic_config_t* config = newrelic_new_config("app name", LICENSE_KEY);

  config->transaction_tracer.enabled = false;

  actual = newrelic_get_transaction_options(config);
  expected = newrelic_get_default_options();

  expected->tt_enabled = false;

  /* Assert that the options were set accordingly.
   */
  assert_true(nr_txn_cmp_options(actual, expected));

  nr_free(actual);
  nr_free(expected);
  free(config);
}

/*
 * Purpose: Test that affirms the transaction options with the transaction
 *          tracer enabled and set to apdex mode are correct.
 */
static void test_get_transaction_options_tt_threshold_apdex(void** state NRUNUSED) {
  nrtxnopt_t* actual;
  nrtxnopt_t* expected;
  newrelic_config_t* config = newrelic_new_config("app name", LICENSE_KEY);

  config->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_APDEX_FAILING;

  actual = newrelic_get_transaction_options(config);
  expected = newrelic_get_default_options();

  expected->tt_enabled = true;
  expected->tt_is_apdex_f = true;

  /* Assert that the options were set accordingly.
   */
  assert_true(nr_txn_cmp_options(actual, expected));

  nr_free(actual);
  nr_free(expected);
  free(config);
}

/*
 * Purpose: Test that affirms the transaction options with the transaction
 *          tracer enabled and set to a duration are correct.
 */
static void test_get_transaction_options_tt_threshold_duration(void** state NRUNUSED) {
  nrtxnopt_t* actual;
  nrtxnopt_t* expected;
  newrelic_config_t* config = newrelic_new_config("app name", LICENSE_KEY);

  config->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  config->transaction_tracer.duration_us = 10;

  actual = newrelic_get_transaction_options(config);
  expected = newrelic_get_default_options();

  expected->tt_enabled = true;
  expected->tt_is_apdex_f = false;
  expected->tt_threshold = 10;

  /* Assert that the options were set accordingly.
   */
  assert_true(nr_txn_cmp_options(actual, expected));

  nr_free(actual);
  nr_free(expected);
  free(config);
}

/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  const struct CMUnitTest options_tests[] = {
      cmocka_unit_test(test_get_transaction_options_default),
      cmocka_unit_test(test_get_transaction_options_tt_disabled),
      cmocka_unit_test(test_get_transaction_options_tt_threshold_apdex),
      cmocka_unit_test(test_get_transaction_options_tt_threshold_duration),
  };

  return cmocka_run_group_tests(options_tests,  // our tests
                                NULL, NULL);
}
