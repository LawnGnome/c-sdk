#ifndef LIBNEWRELIC_CONFIG_H
#define LIBNEWRELIC_CONFIG_H

#include "nr_txn.h"

nrtxnopt_t* newrelic_get_default_options(void);

/*!
 * @brief Convert a C agent configuration into transaction options.
 *
 * @param [in] config The configuration to convert. If NULL, default options
 *                    will be returned.
 */
nrtxnopt_t* newrelic_get_transaction_options(const newrelic_config_t* config);

#endif /* LIBNEWRELIC_CONFIG_H */
