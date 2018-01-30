#ifndef LIBNEWRELIC_EXTERNAL_H
#define LIBNEWRELIC_EXTERNAL_H

#include "node_external.h"
#include "nr_txn.h"
 
/*! @brief The internal type used to represent an external segment. */
struct _newrelic_external_segment_t {
  /*! The transaction the external segment was created on. */
  nrtxn_t *txn;

  /*! The external node parameters. */
  nr_node_external_params_t params;
};

/*! @brief Destroy an external segment. */
void newrelic_destroy_external_segment(newrelic_external_segment_t** segment_ptr);

#endif /* LIBNEWRELIC_EXTERNAL_H */
