#include <stdarg.h>
#include <stddef.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "app.h"
#include "transaction.h"

#include "nr_txn.h"
#include "test.h"
#include "util_memory.h"

/* Declare prototypes for mocks */
void __wrap_nr_txn_set_as_web_transaction(nrtxn_t* txn, const char* reason);

/*
 * Purpose: This is a cmocka mock. It wraps/monkey-patches
 * the nr_txn_set_as_web_transaction function.  Instead of
 * calling nr_txn_set_as_web_transaction, our code will
 * call __wrap_nr_txn_set_as_web_transaction.  The mock()
 * function used inside this function returns a queued value.
 * The testing programmer (us!) uses the will_return function
 * to queue values (see tests below)
 */
void __wrap_nr_txn_set_as_web_transaction(nrtxn_t* txn,
                                          const char* reason NRUNUSED) {
  check_expected(txn);
}

/*
 * Purpose: This is a cmocka setup fixture. In this function, the
 * testing programmer (us!) can instantiate variables to use in
 * our tests.  Values are passed around via the **state double pointer
 *
 * Returns: an int indicating the success (0) or failture (non-zero)
 * of the fixture.  Used in test reporting output.
 */
static int group_setup(void** state) {
  newrelic_app_t* app;
  nrapp_t* nrapp;
  nr_app_info_t* app_info;

  nrapp = (nrapp_t*)nr_zalloc(sizeof(nrapp_t));
  nrapp->state = NR_APP_OK;

  app_info = (nr_app_info_t*)nr_zalloc(sizeof(nr_app_info_t));

  app = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));
  app->app_info = app_info;
  app->app = nrapp;

  *state = app;
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
  nr_free(appWithInfo->app);
  newrelic_destroy_app(&appWithInfo);
  return 0;  // tells cmocka teardown completed, 0==OK
}

/*
 * Purpose: Tests that function can survive a null app being passed
 */
static void test_txn_null_app(void** state NRUNUSED) {
  // Good enough to check for NULL transaction.  The real test
  // is that nr_txn_set_as_web_transaction never gets called.  If it did
  // the unit test would blow up for not knowing what to return, and we
  // expect this test to fail before ever getting to that step.
  assert_null(newrelic_start_web_transaction(NULL, "aTransaction"));
}

/*
 * Purpose: Tests that function can survive a null name
 */
static void test_txn_null_name(void** state) {
  nrtxn_t* txn = NULL;

  // fetch our fixture value from the state
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;

  expect_any(__wrap_nr_txn_set_as_web_transaction, txn);
  txn = newrelic_start_web_transaction(appWithInfo, NULL);

  assert_non_null(txn);
  nr_txn_destroy(&txn);
}

/*
 * Purpose: Tests that function can survive a null name
 */
static void test_txn_valid(void** state) {
  nrtxn_t* txn = NULL;

  // fetch our fixture value from the state
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;

  expect_any(__wrap_nr_txn_set_as_web_transaction, txn);
  txn = newrelic_start_web_transaction(appWithInfo, "aTransaction");

  assert_non_null(txn);
  nr_txn_destroy(&txn);
}

/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  // to run tests we pass cmocka_run_group_tests an
  // array of unit tests.  A unit tests is a named function
  // passed into cmocka_unit_test
  const struct CMUnitTest license_tests[] = {
      cmocka_unit_test(test_txn_null_app),
      cmocka_unit_test(test_txn_null_name),
      cmocka_unit_test(test_txn_valid),
  };

  return cmocka_run_group_tests(license_tests,  // our tests
                                group_setup,    // setup fixture
                                group_teardown  // teardown fixtures
  );
}
