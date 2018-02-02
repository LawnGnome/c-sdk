#include "test.h"
#include "libnewrelic.h"
#include "config.h"

#include "nr_txn.h"
#include "util_memory.h"

/*! @brief Provides a fake transaction as the group state. */
int txn_group_setup(void** state) {
  newrelic_txn_t* txn = 0;
  nrtxnopt_t* opts = 0;
  txn = (newrelic_txn_t*)nr_zalloc(sizeof(newrelic_txn_t));
  opts = newrelic_get_default_options();
  txn->options = *opts;
  nr_free(opts);
  txn->status.recording = 1;

  *state = txn;
  return 0;  // tells cmocka setup completed, 0==OK
}

/*! @brief Cleans up the fake transaction provided in the group state. */
int txn_group_teardown(void** state) {
  newrelic_txn_t* txn = 0;
  txn = (newrelic_txn_t*)*state;

  nr_free(txn);
  return 0;  // tells cmocka teardown completed, 0==OK
}
