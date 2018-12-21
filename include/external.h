/*!
 * @file external.h
 *
 * @brief Type definitions, constants, and function declarations necessary to
 * support external segments in the C Agent.
 */
#ifndef LIBNEWRELIC_EXTERNAL_H
#define LIBNEWRELIC_EXTERNAL_H

#include "segment.h"

extern bool newrelic_end_external_segment(newrelic_segment_t* segment);

#endif /* LIBNEWRELIC_EXTERNAL_H */
