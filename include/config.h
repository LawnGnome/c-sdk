#ifndef LIBNEWRELIC_CONFIG_H
#define LIBNEWRELIC_CONFIG_H

#include "nr_txn.h"

/*!
 * @brief Given a string "off", "raw" or "obfuscated" return the
 * corresponding nr_tt_recordsql value.
 *
 * @return  If the string is NULL or any other value, return
 * the default NR_SQL_OBFUSCATED. Otherwise:
 *  - If "off", return NR_SQL_NONE.
 *  - If "raw", return NR_SQL_RAW.
 *  - If "obfuscated", return NR_SQL_OBFUSCATED.
 */
nr_tt_recordsql_t newrelic_validate_recordsql(const char* setting);

/*!
 * @brief Create a set of default agent configuration options
 *
 * @return A newly allocated nrtxnopt_t struct, which must be released
 * 				 with nr_free() when no longer required.
 */
nrtxnopt_t* newrelic_get_default_options(void);

/*!
 * @brief Convert a C agent configuration into transaction options.
 *
 * @param [in] config The configuration to convert. If NULL, default options
 *                    will be returned.
 * @return A newly allocated nrtxnopt_t struct, which must be released with
 *         nr_free() when no longer required.
 */
nrtxnopt_t* newrelic_get_transaction_options(const newrelic_config_t* config);

#endif /* LIBNEWRELIC_CONFIG_H */
