#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "libnewrelic_internal.h"
#include "test.h"
#include "nr_txn.h"
#include "util_memory.h"

/* Declare prototypes for mocks */
nr_status_t __wrap_nr_txn_add_user_custom_parameter(nrtxn_t* txn,
                                                    const char* key,
                                                    const nrobj_t* value);

/**
 * Purpose: Mock to catch adding a customer attribute/parameter.  The mock()
 * function used inside this function returns a queued value.
 * The testing programmer (us!) uses the will_return function
 * to queue values (see tests below)
 */
nr_status_t __wrap_nr_txn_add_user_custom_parameter(nrtxn_t* txn NRUNUSED,
                                                    const char* key NRUNUSED,
                                                    const nrobj_t* value
                                                        NRUNUSED) {
  return (nr_status_t)mock();
}

/**
 * Purpose: This is a cmocka setup fixture. In this function, the
 * testing programmer (us!) can instantiate variables to use in
 * our tests.  Values are passed around via the **state double pointer
 *
 * Returns: an int indicating the success (0) or failture (non-zero)
 * of the fixture.  Used in test reporting output.
 */
static int group_setup(void** state) {
  newrelic_txn_t* txn;
  txn = (newrelic_txn_t*)nr_zalloc(sizeof(newrelic_txn_t));

  *state = txn;
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
  newrelic_txn_t* txn;
  txn = (newrelic_txn_t*)*state;

  nr_free(txn);
  return 0;  // tells cmocka teardown completed, 0==OK
}

static void test_add_attribute_null_txn(void** state NRUNUSED) {
  bool ret = true;
  nrobj_t* value = nro_new_int(1);
  ret = newrelic_add_attribute(NULL, "key", value);
  nro_delete(value);
  assert_false(ret);
}

static void test_add_attribute_null_key(void** state) {
  bool ret = true;
  newrelic_txn_t* txn;
  nrobj_t* value = nro_new_int(1);
  txn = (newrelic_txn_t*)*state;
  ret = newrelic_add_attribute(txn, NULL, value);
  nro_delete(value);
  assert_false(ret);
}

static void test_add_attribute_null_obj(void** state) {
  bool ret = true;
  newrelic_txn_t* txn;
  txn = (newrelic_txn_t*)*state;
  ret = newrelic_add_attribute(txn, "key", NULL);
  assert_false(ret);
}

static void test_add_attribute_failure(void** state) {
  bool ret = true;
  newrelic_txn_t* txn;
  nrobj_t* value = nro_new_int(1);
  txn = (newrelic_txn_t*)*state;
  will_return(__wrap_nr_txn_add_user_custom_parameter, NR_FAILURE);
  ret = newrelic_add_attribute(txn, "key", value);
  nro_delete(value);
  assert_false(ret);
}

static void test_add_attribute_success(void** state) {
  bool ret = true;
  newrelic_txn_t* txn;
  nrobj_t* value = nro_new_int(1);
  txn = (newrelic_txn_t*)*state;
  will_return(__wrap_nr_txn_add_user_custom_parameter, NR_SUCCESS);
  ret = newrelic_add_attribute(txn, "key", value);
  nro_delete(value);
  assert_true(ret);
}

static void test_add_attribute_string_null_value(void** state) {
  bool ret = true;
  newrelic_txn_t* txn;
  txn = (newrelic_txn_t*)*state;
  ret = newrelic_add_attribute_string(txn, "key", NULL);
  assert_false(ret);
}

int main(void) {
  const struct CMUnitTest attribute_tests[] = {
      cmocka_unit_test(test_add_attribute_null_txn),
      cmocka_unit_test(test_add_attribute_null_key),
      cmocka_unit_test(test_add_attribute_null_obj),
      cmocka_unit_test(test_add_attribute_failure),
      cmocka_unit_test(test_add_attribute_success),
      cmocka_unit_test(test_add_attribute_string_null_value),
  };

  return cmocka_run_group_tests(attribute_tests, group_setup, group_teardown);
}
