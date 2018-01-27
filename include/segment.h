/*!
 * @file Common functionality shared by external and datastore segment support.
 *
 */

#ifndef LIBNEWRELIC_SEGMENT_H
#define LIBNEWRELIC_SEGMENT_H

#include <stdbool.h>
#include "nr_txn.h"

typedef struct _newrelic_segment_t {
  nrtxn_t*    transaction;
  char*       name;
  nrtxntime_t start;
  nrtime_t    kids_duration;
  nrtime_t*   kids_duration_save;
} newrelic_segment_t;

/*!
 * @brief Validate segment parameter.
 *
 * This function ensures that any given parameter does not include a slash. As
 * segment parameter values are generally used in metric names, slashes will
 * break the APM UI.
 *
 * @param [in] in   The parameter value.
 * @param [in] name The parameter name (used in any error message).
 * @return True if the parameter value is valid, false otherwise. A message at
 *         level LOG_ERROR will be logged if validation fails.
 */
bool newrelic_validate_segment_param(const char* in, const char* name);

#endif
