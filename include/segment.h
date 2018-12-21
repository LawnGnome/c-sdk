/*!
 * @file segment.h
 *
 * @brief Common functionality shared by external and datastore segment support.
 */
#ifndef LIBNEWRELIC_SEGMENT_H
#define LIBNEWRELIC_SEGMENT_H

#include <stdbool.h>
#include "nr_txn.h"

typedef struct _newrelic_segment_t {
  nr_segment_t* segment;
  nrtxn_t* transaction;

  /* Fields related to exclusive time calculation. */
  nrtime_t kids_duration;
  nrtime_t* kids_duration_save;

  /* Type fields. Which union is valid depends on segment->type, which is the
   * source of truth for what type of segment this is.
   *
   * Although there is only one type that currently requires fields at this
   * level, we have a union to minimise future refactoring. */
  union {
    struct {
      char* collection;
      char* operation;
    } datastore;
  } type;
} newrelic_segment_t;

/*!
 * @brief Create a new segment.
 *
 * This function ensures that segments are created correctly, particularly
 * around exclusive time calculations.
 *
 * @param [in] txn The transaction to create the segment on.
 * @return A segment, or NULL if an error occurred.
 *
 * @warning As this is an internal function, a NULL check is _not_ performed on
 *          txn! Additionally, the transaction must be locked before calling
 *          this function.
 */
extern newrelic_segment_t* newrelic_segment_create(nrtxn_t* txn);

extern void newrelic_segment_destroy(newrelic_segment_t** segment_ptr);

/*!
 * @brief Validate segment parameter.
 *
 * This function ensures that any given parameter does not include a slash. As
 * segment parameter values are generally used in metric names, slashes will
 * break the APM UI.
 *
 * @param [in] in   The parameter value.
 * @param [in] name The parameter name (used in any error message).
 *
 * @return True if the parameter value is valid, false otherwise. A message at
 * level LOG_ERROR will be logged if validation fails.
 */
bool newrelic_validate_segment_param(const char* in, const char* name);

#endif /* LIBNEWRELIC_SEGMENT_H */
