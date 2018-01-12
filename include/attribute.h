#ifndef LIBNEWRELIC_ATTRIBUTE_H
#define LIBNEWRELIC_ATTRIBUTE_H

#include "nr_txn.h"
#include "util_object.h"

bool newrelic_add_attribute(newrelic_txn_t* transaction,
                            const char* key,
                            nrobj_t* obj);

#endif /* LIBNEWRELIC_ATTRIBUTE_H */
