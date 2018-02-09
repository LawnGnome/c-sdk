/*!
 * @file transaction.h
 *
 * @brief Function declarations necessary to support starting transactions
 * in the C agent.
 */
#ifndef LIBNEWRELIC_TRANSACTION_H
#define LIBNEWRELIC_TRANSACTION_H

/*!
 * @brief Start a transaction
 *
 * @param [in] app New Relic application information.
 * @param [in] name The name of the transaction; may be NULL.
 * @param [in] is_web_transaction true if the transaction is a web transaction;
 * false otherwise.
 *
 * @return A pointer to an active transaction; NULL if the transaction could not
 * be started.
 */
newrelic_txn_t* newrelic_start_transaction(newrelic_app_t* app,
                                           const char* name,
                                           bool is_web_transaction);

#endif /* LIBNEWRELIC_TRANSACTION_H */
