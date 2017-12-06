#include <stdarg.h>
#include <stddef.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "libnewrelic_internal.h"

#include "nr_txn.h"
#include "test.h"
#include "util_memory.h"

/*
 * Purpose: Test that affirms the default options returned are expected values.
 */
static void test_get_default_options(void** state NRUNUSED) {
  nrtxnopt_t* options = newrelic_get_default_options();

  nrtxnopt_t* correct = nr_zalloc(sizeof(nrtxnopt_t));
  correct->analytics_events_enabled = true;

  /* Assert that the true portion of the default options were set accordingly.
   */
  assert_true(newrelic_cmp_options(options, correct));
}

/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  const struct CMUnitTest options_tests[] = {
      cmocka_unit_test(test_get_default_options),
  };

  return cmocka_run_group_tests(options_tests,  // our tests
                                NULL, NULL);
}
