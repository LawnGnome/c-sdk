#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "test.h"
#include "nr_txn.h"
#include "transaction.h"
#include "util_memory.h"

/* Declare prototypes for mocks */
nr_status_t __wrap_nr_cmd_txndata_tx(int daemon_fd, const nrtxn_t* txn);

/**
 * Purpose: Mock to catch transaction calls to the daemon.  The mock()
 * function used inside this function returns a queued value.
 */
nr_status_t __wrap_nr_cmd_txndata_tx(int daemon_fd NRUNUSED,
                                     const nrtxn_t* txn NRUNUSED) {
  return (nr_status_t)mock();
}

static newrelic_txn_t* mock_txn(void) {
  newrelic_txn_t* txn = nr_malloc(sizeof(newrelic_txn_t));

  nrt_mutex_init(&txn->lock, 0);
  txn->txn = nr_zalloc(sizeof(nrtxn_t));

  return txn;
}

static void destroy_mock_txn(newrelic_txn_t** txn_ptr) {
  newrelic_txn_t* txn;

  if (NULL == txn_ptr || NULL == *txn_ptr) {
    return;
  }

  txn = *txn_ptr;
  nrt_mutex_destroy(&txn->lock);
  nr_free(txn->txn);
  nr_realfree((void**)txn_ptr);
}

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

static void test_end_transaction_ignored_fail(void** state NRUNUSED) {
  bool ret;
  newrelic_txn_t* txn = mock_txn();
  txn->txn->status.ignore = 0;
  will_return(__wrap_nr_cmd_txndata_tx, NR_FAILURE);
  ret = newrelic_end_transaction(&txn);
  assert_false(ret);
  destroy_mock_txn(&txn);
}

static void test_end_transaction_ignored_success(void** state NRUNUSED) {
  bool ret;
  newrelic_txn_t* txn = mock_txn();
  txn->txn->status.ignore = 0;
  will_return(__wrap_nr_cmd_txndata_tx, NR_SUCCESS);
  ret = newrelic_end_transaction(&txn);
  assert_true(ret);
  destroy_mock_txn(&txn);
}

static void test_end_transaction_valid(void** state NRUNUSED) {
  bool ret;
  newrelic_txn_t* txn = mock_txn();
  txn->txn->status.ignore = 1;
  ret = newrelic_end_transaction(&txn);
  assert_true(ret);
  destroy_mock_txn(&txn);
}

int main(void) {
  const struct CMUnitTest transaction_tests[] = {
      cmocka_unit_test(test_end_transaction_null),
      cmocka_unit_test(test_end_transaction_null_transaction),
      cmocka_unit_test(test_end_transaction_ignored_fail),
      cmocka_unit_test(test_end_transaction_ignored_success),
      cmocka_unit_test(test_end_transaction_valid),
  };

  return cmocka_run_group_tests(transaction_tests, NULL, NULL);
}
