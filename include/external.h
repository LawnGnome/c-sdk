#ifndef LIBNEWRELIC_EXTERNAL_H
#define LIBNEWRELIC_EXTERNAL_H

#include "node_external.h"
#include "nr_txn.h"
 
/*! @brief The internal type used to represent an external segment. */
typedef struct _newrelic_external_segment_t {
  /*! The transaction the external segment was created on. */
  nrtxn_t *txn;

  /*! The external node parameters. */
  nr_node_external_params_t params;
} newrelic_external_segment_t;

/*!
 * @brief Validate an external parameter.
 *
 * This function ensures that any given parameter does not include a slash. As
 * external parameter values are generally used in metric names, slashes will
 * break the APM UI.
 *
 * @param [in] in   The parameter value.
 * @param [in] name The parameter name (used in any error message).
 * @return True if the parameter value is valid, false otherwise. A message at
 *         level LOG_ERROR will be logged if validation fails.
 */
bool newrelic_validate_external_param(const char* in, const char* name);

#endif /* LIBNEWRELIC_EXTERNAL_H */
