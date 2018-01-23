#ifndef LIBNEWRELIC_EXTERNAL_H
#define LIBNEWRELIC_EXTERNAL_H

#include "nr_txn.h"
#include "util_object.h"
 
typedef struct _newrelic_external_segment_t {
  nrtxn_t *txn;
  nrtxntime_t start;
  char* library;
  char* procedure;
  char* uri; // cleaned
} newrelic_external_segment_t;

bool newrelic_validate_external_param(const char* in,
                                      const char* name,
                                      bool accept_null);

#endif /* LIBNEWRELIC_EXTERNAL_H */
