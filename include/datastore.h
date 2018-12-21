/*!
 * @file datastore.h
 *
 * @brief Type definitions, constants, and function declarations necessary to
 * support datastore segments, or axiom datastore nodes, for the C agent.
 */
#ifndef LIBNEWRELIC_DATASTORE_H
#define LIBNEWRELIC_DATASTORE_H

#include "nr_txn.h"

/*! @brief The internal type used to represent a datastore segment. */
struct _newrelic_datastore_segment_t {
  /*! The transaction the datastore segment was created on. */
  nrtxn_t* txn;

  /*! The internal segment. */
  nr_segment_t* segment;

  char* collection;
  char* operation;
};

/*!
 * @brief Free memory allocated to a datastore segment.
 *
 * @param [in,out] segment_ptr The address of a datastore segment to destroy.
 * Before the function returns, any segment_ptr memory is freed;
 * segment_ptr is set to NULL to avoid any potential double free errors.
 *
 */
void newrelic_destroy_datastore_segment(
    newrelic_datastore_segment_t** segment_ptr);

#endif /* LIBNEWRELIC_DATASTORE_H */
