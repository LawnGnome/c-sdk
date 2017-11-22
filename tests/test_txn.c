#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmocka.h"

#include "libnewrelic.h"
#include "libnewrelic_internal.h"

#include "nr_txn.h"
#include "test.h"
#include "util_memory.h"

/* Declare prototypes for mocks */
nrtxn_t* __wrap_nr_txn_set_as_web_transaction(const nrapp_t*,
                                              const nrtxnopt_t*,
                                              const nr_attribute_config_t*);

/*
 * Purpose: This is a cmocka mock. It wraps/monkey-patches
 * the nr_txn_set_as_web_transaction function.  Instead of
 * calling nr_txn_set_as_web_transaction, our code will
 * call __wrap_nr_txn_set_as_web_transaction.  The mock()
 * function used inside this function returns a queued value.
 * The testing programmer (us!) uses the will_return function
 * to queue values (see tests below)
 */
nrtxn_t* __wrap_nr_txn_set_as_web_transaction(
    const nrapp_t* app NRUNUSED,
    const nrtxnopt_t* opts NRUNUSED,
    const nr_attribute_config_t* attribute_config NRUNUSED) {
  return mock_ptr_type(nrtxn_t*);
}

/*
 * Purpose: This is a cmocka setup fixture. In this function, the
 * testing programmer (us!) can instantiate variables to use in
 * our tests.  Values are passed around via the **state double pointer
 *
 * Returns: an int indicating the success (0) or failture (non-zero)
 * of the fixture.  Used in test reporting output.
 */
static int group_setup(void** state NRUNUSED) {
  int* answer;
  newrelic_app_t* appWithInfo;

  appWithInfo = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));
  answer = (int*)malloc(sizeof(int));

  assert_non_null(answer);

  *state = appWithInfo;
  return 0;  // tells cmocka setup completed, 0==OK
}

/*
 * Purpose: This is a cmocka teardown` fixture. In this function, the
 * testing programmer (us!) can free memory or perform other tear downs.
 *
 * Returns: an int indicating the success (0) or failture (non-zero)
 * of the fixture.  Used in test reporting output.
 */
static int group_teardown(void** state) {
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;
  newrelic_destroy_app(&appWithInfo);
  return 0;  // tells cmocka teardown completed, 0==OK
}

/*
 * Purpose: Tests that function can survive a null app being passed
 */
static void test_null_app(void** state NRUNUSED) {
  newrelic_app_t* app = NULL;
  newrelic_txn_t* txn = NULL;
  txn = newrelic_start_web_transaction(app, "aTransaction");
  assert_null(txn);
}

/*
 * Purpose: Tests that function can survive a null name
 */
static void test_null_name(void** state) {
  nrtxn_t* txn;

  // fetch our fixture value from the state
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;

  // we mock the nr_txn_set_as_web_transaction function to
  // avoid segfaulting (or needing to completely stub
  // out the appWithInfo struct
  //will_return(__wrap_nr_txn_set_as_web_transaction, txn);
  txn = newrelic_start_web_transaction(appWithInfo, NULL);

  assert_null(txn);
}

/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  // to run tests we pass cmocka_run_group_tests an
  // array of unit tests.  A unit tests is a named function
  // passed into cmocka_unit_test
  const struct CMUnitTest license_tests[] = {
      cmocka_unit_test(test_null_app), cmocka_unit_test(test_null_name),
  };

  return cmocka_run_group_tests(license_tests,  // our tests
                                group_setup,    // setup fixture
                                group_teardown  // teardown fixtures
                                );
}