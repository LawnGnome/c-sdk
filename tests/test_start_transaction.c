#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "libnewrelic.h"
#include "libnewrelic_internal.h"
#include "app.h"
#include "transaction.h"
#include "nr_txn.h"
#include "util_memory.h"

void __wrap_nr_txn_set_as_web_transaction(nrtxn_t* txn, const char* reason);
void __wrap_nr_txn_set_as_background_job(nrtxn_t* txn, const char* reason);

/**
 * Purpose: Mock to ensure the nr_txn_set_as_web_transaction
 * gets called for web transactions
 */
void __wrap_nr_txn_set_as_web_transaction(nrtxn_t* txn,
                                          const char* reason NRUNUSED) {
  check_expected(txn);
}

/**
 * Purpose: Mock to ensure the __wrap_nr_txn_set_as_background_job
 * gets called for non-web transactions
 */
void __wrap_nr_txn_set_as_background_job(nrtxn_t* txn,
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
  newrelic_app_t* appWithInfo;

  appWithInfo = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));

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
static void test_start_transaction_null_app_web(void** state NRUNUSED) {
  newrelic_app_t* app = NULL;
  newrelic_txn_t* txn = NULL;

  txn = newrelic_start_transaction(app, "aTransaction", true);
  assert_null(txn);
}

/*
 * Purpose: Tests that function can survive a null app being passed
 */
static void test_start_transaction_null_app_background(void** state NRUNUSED) {
  newrelic_app_t* app = NULL;
  newrelic_txn_t* txn = NULL;

  txn = newrelic_start_transaction(app, "aTransaction", false);
  assert_null(txn);
}

/*
 * Purpose: Tests that function can survive a null name
 */
static void test_start_transaction_null_name_web(void** state) {
  // nrapp_t *app;
  newrelic_txn_t* txn;

  // fetch our fixture value from the state
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;

  // test with a "web" transaction
  expect_any(__wrap_nr_txn_set_as_web_transaction, txn);
  txn = newrelic_start_transaction(appWithInfo, NULL, true);
  assert_null(txn);
}

/*
 * Purpose: Tests that function can survive a null name
 */
static void test_start_transaction_null_name_background(void** state) {
  // nrapp_t *app;
  newrelic_txn_t* txn;

  // fetch our fixture value from the state
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;

  // tests with a "background" transaction
  expect_any(__wrap_nr_txn_set_as_background_job, txn);
  txn = newrelic_start_transaction(appWithInfo, NULL, false);
  assert_null(txn);
}

/*
 * Purpose: Tests that function works with a real string transaction name
 */
static void test_start_transaction_string_name_web(void** state) {
  // nrapp_t *app;
  newrelic_txn_t* txn;

  // fetch our fixture value from the state
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;

  // test with a "web" transaction
  expect_any(__wrap_nr_txn_set_as_web_transaction, txn);
  txn = newrelic_start_transaction(appWithInfo, "TheTransaction", true);
  assert_null(txn);
}

/*
 * Purpose: Tests that function works with a real string transaction name
 */
static void test_start_transaction_string_name_background(void** state) {
  // nrapp_t *app;
  newrelic_txn_t* txn;

  // fetch our fixture value from the state
  newrelic_app_t* appWithInfo;
  appWithInfo = (newrelic_app_t*)*state;

  // tests with a "background" transaction
  expect_any(__wrap_nr_txn_set_as_background_job, txn);
  txn = newrelic_start_transaction(appWithInfo, "TheTransaction", false);
  assert_null(txn);
}
/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  // to run tests we pass cmocka_run_group_tests an
  // array of unit tests.  A unit tests is a named function
  // passed into cmocka_unit_test
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_start_transaction_null_app_web),
      cmocka_unit_test(test_start_transaction_null_app_background),
      cmocka_unit_test(test_start_transaction_null_name_web),
      cmocka_unit_test(test_start_transaction_null_name_background),
      cmocka_unit_test(test_start_transaction_string_name_web),
      cmocka_unit_test(test_start_transaction_string_name_background),
  };

  return cmocka_run_group_tests(tests,  // our tests
                                group_setup,    // setup fixture
                                group_teardown  // teardown fixtures
  );
}
