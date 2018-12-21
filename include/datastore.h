/*!
 * @file datastore.h
 *
 * @brief Type definitions, constants, and function declarations necessary to
 * support datastore segments, or axiom datastore nodes, for the C agent.
 */
#ifndef LIBNEWRELIC_DATASTORE_H
#define LIBNEWRELIC_DATASTORE_H

#include "segment.h"

extern void newrelic_destroy_datastore_segment_fields(
    newrelic_segment_t* segment);

extern bool newrelic_end_datastore_segment(newrelic_segment_t* segment);

#endif /* LIBNEWRELIC_DATASTORE_H */
