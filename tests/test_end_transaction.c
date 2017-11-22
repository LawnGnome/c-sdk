#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "nr_txn.h"
#include "test.h"

static void test_end_transaction_null(void** state NRUNUSED) {
  bool ret = true;
  ret = newrelic_end_transaction(NULL);
  assert_false(ret);
}

static void test_end_transaction_null_transaction(void** state NRUNUSED) {
  bool ret = true;
  newrelic_txn_t* txn = 0;
  ret = newrelic_end_transaction(&txn);
  assert_false(ret);
}

static void test_end_transaction_ignored(void** state NRUNUSED) {
  bool ret;
  newrelic_txn_t* txn;
  txn = (newrelic_txn_t*)malloc(sizeof(newrelic_txn_t));
  txn->status.ignore = 0;
  ret = newrelic_end_transaction(&txn);
  assert_false(ret);
  free(txn);
}

static void test_end_transaction_valid(void** state NRUNUSED) {
  bool ret;
  newrelic_txn_t* txn;
  txn = (newrelic_txn_t*)malloc(sizeof(newrelic_txn_t));
  txn->status.ignore = 1;
  ret = newrelic_end_transaction(&txn);
  assert_true(ret);
}

int main(void) {
  const struct CMUnitTest transaction_tests[] = {
      cmocka_unit_test(test_end_transaction_null),
      cmocka_unit_test(test_end_transaction_null_transaction),
      cmocka_unit_test(test_end_transaction_ignored),
      cmocka_unit_test(test_end_transaction_valid),
  };

  return cmocka_run_group_tests(transaction_tests, NULL, NULL);
}
