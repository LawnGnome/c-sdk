#include <stdio.h>
#include "libnewrelic.h"
#include "nr_txn.h"
#include "transaction.h"

bool newrelic_record_custom_metric(newrelic_txn_t* transaction,
                                   const char* metric_name,
                                   double milliseconds) {
  if (NULL == transaction || NULL == metric_name) {
    return false;
  }

  return NR_SUCCESS
         == nr_txn_add_custom_metric(transaction->txn, metric_name,
                                     milliseconds);
}
