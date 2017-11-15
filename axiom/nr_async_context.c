#include "nr_axiom.h"

#include <stddef.h>

#include "nr_async_context.h"
#include "nr_async_context_private.h"
#include "util_memory.h"

nr_async_context_t *
nr_async_context_create (nrtime_t start)
{
  nr_async_context_t *context;

  context = (nr_async_context_t *) nr_zalloc (sizeof (nr_async_context_t));
  context->start = start;

  return context;
}

void
nr_async_context_destroy (nr_async_context_t **context_ptr)
{
  nr_realfree ((void **) context_ptr);
}

void
nr_async_context_add (nr_async_context_t *context, nrtime_t duration)
{
  if (NULL == context) {
    return;
  }

  context->async_duration += duration;
}

void
nr_async_context_end (nr_async_context_t *context, nrtime_t stop)
{
  if (NULL == context) {
    return;
  }

  context->stop = stop;
}

nrtime_t
nr_async_context_get_duration (const nr_async_context_t *context)
{
  nrtime_t wallclock;

  if (NULL == context) {
    return 0;
  }

  wallclock = nr_time_duration (context->start, context->stop);
  if (wallclock > context->async_duration) {
    return 0;
  }
  return context->async_duration - wallclock;
}
