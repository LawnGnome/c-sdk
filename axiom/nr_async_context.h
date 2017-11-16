/*
 * Functions relating to handling async contexts: periods of time within a
 * transaction where one or more asynchronous operations can take place for
 * which we want to calculate how much time was spent "off the main thread":
 * that is, additional execution time over and above the wallclock time.
 */
#ifndef NR_ASYNC_CONTEXT_HDR
#define NR_ASYNC_CONTEXT_HDR

#include "util_time.h"

/*
 * The opaque async context type.
 */
typedef struct _nr_async_context_t nr_async_context_t;

/*
 * Purpose : Create an async context.
 *
 * Params  : 1. The start time.
 *
 * Returns : A new async context, which must be destroyed with
 *           nr_async_context_destroy().
 */
extern nr_async_context_t *
nr_async_context_create (nrtime_t start);

/*
 * Purpose : Destroy an async context.
 *
 * Params  : 1. A pointer to the async context.
 */
extern void
nr_async_context_destroy (nr_async_context_t **context_ptr);

/*
 * Purpose : Add the duration of an asynchronous operation to the async
 *           context.
 *
 * Params  : 1. The async context.
 *           2. The duration to add.
 */
extern void
nr_async_context_add (nr_async_context_t *context, nrtime_t duration);

/*
 * Purpose : Set the end time of the async context.
 *
 * Params  : 1. The async context.
 *           2. The end time.
 */
extern void
nr_async_context_end (nr_async_context_t *context, nrtime_t stop);

/*
 * Purpose : Calculate the "off the main thread" duration, as defined above.
 *
 * Params  : 1. The async context.
 *
 * Returns : The duration.
 *
 * Notes   : If nr_async_context_end() hasn't been called before this function,
 *           the result will always be 0.
 */
extern nrtime_t
nr_async_context_get_duration (const nr_async_context_t *context);

#endif /* NR_ASYNC_CONTEXT_HDR */
