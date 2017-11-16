/*
 * This file contains internal object data structures.
 */
#ifndef UTIL_OBJECT_PRIVATE_HDR
#define UTIL_OBJECT_PRIVATE_HDR

/*
 * This header file exposes internal functions that are only made visible for unit testing.
 * Other clients are forbidden.
 */
#include "util_object.h"

extern nrobj_t *nro_assert (nrobj_t *obj, nrotype_t type);

#endif /* UTIL_OBJECT_PRIVATE_HDR */
