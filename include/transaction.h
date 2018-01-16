#ifndef LIBNEWRELIC_TRANSACTION_H
#define LIBNEWRELIC_TRANSACTION_H

newrelic_txn_t* newrelic_start_transaction(newrelic_app_t* app,
                                           const char* name,
                                           bool is_web_transaction);

#endif /* LIBNEWRELIC_TRANSACTION_H */
