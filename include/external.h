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

  /*! The external node parameters. */
  nr_node_external_params_t params;
};

/*!
 * @brief Free memory allocated to an external segment.
 *
 * param [in] segment_ptr The address of an external segment to destroy.
 */
void newrelic_destroy_external_segment(
    newrelic_external_segment_t** segment_ptr);

#endif /* LIBNEWRELIC_EXTERNAL_H */
