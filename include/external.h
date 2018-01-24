#ifndef LIBNEWRELIC_EXTERNAL_H
#define LIBNEWRELIC_EXTERNAL_H

#include "node_external.h"
#include "nr_txn.h"
 
typedef struct _newrelic_external_segment_t {
  nrtxn_t *txn;
  nr_node_external_params_t params;
} newrelic_external_segment_t;

bool newrelic_validate_external_param(const char* in,
                                      const char* name,
                                      bool accept_null);

#endif /* LIBNEWRELIC_EXTERNAL_H */
