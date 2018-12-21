/*!
 * @file external.h
 *
 * @brief Type definitions, constants, and function declarations necessary to
 * support external segments in the C Agent.
 */
#ifndef LIBNEWRELIC_EXTERNAL_H
#define LIBNEWRELIC_EXTERNAL_H

#include "node_external.h"
#include "nr_txn.h"

/*! @brief The internal type used to represent an external segment. */
struct _newrelic_external_segment_t {
  /*! The transaction the external segment was created on. */
  nrtxn_t* txn;

  /*! The internal segment. */
  nr_segment_t* segment;
};

/*!
 * @brief Free memory allocated to an external segment.
 *
 * @param [in,out] segment_ptr The address of an external segment to destroy.
 * Before the function returns, any segment_ptr memory is freed;
 * segment_ptr is set to NULL to avoid any potential double free errors.
 */
void newrelic_destroy_external_segment(
    newrelic_external_segment_t** segment_ptr);

#endif /* LIBNEWRELIC_EXTERNAL_H */
