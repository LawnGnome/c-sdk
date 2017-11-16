#include "nr_axiom.h"

#include "nr_async_context.h"
#include "nr_async_context_private.h"

#include "tlib_main.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

static void
test_create_destroy (void)
{
  nr_async_context_t *context;

  /*
   * Test : Basic operation.
   */
  context = nr_async_context_create (1);
  tlib_pass_if_not_null ("context", context);
  tlib_pass_if_time_equal ("context start",    1, context->start);
  tlib_pass_if_time_equal ("context stop",     0, context->stop);
  tlib_pass_if_time_equal ("context duration", 0, context->async_duration);
  nr_async_context_destroy (&context);
  tlib_pass_if_null ("context destroyed", context);
}

static void
test_add (void)
{
  nr_async_context_t *context = nr_async_context_create (1);

  /*
   * Test : Bad parameters.
   */
  nr_async_context_add (NULL, 0);

  nr_async_context_add (context, 0);
  tlib_pass_if_time_equal ("duration", 0, context->async_duration);

  /*
   * Test : Normal operation.
   */
  nr_async_context_add (context, 42);
  tlib_pass_if_time_equal ("duration", 42, context->async_duration);

  nr_async_context_destroy (&context);
}

static void
test_end (void)
{
  nr_async_context_t *context = nr_async_context_create (1);

  /*
   * Test : Bad parameters.
   */
  nr_async_context_end (NULL, 0);

  /*
   * Test : Normal operation.
   */
  nr_async_context_end (context, 42);
  tlib_pass_if_time_equal ("stop", 42, context->stop);

  nr_async_context_end (context, 22);
  tlib_pass_if_time_equal ("stop", 22, context->stop);

  nr_async_context_destroy (&context);
}

static void
test_get_duration (void)
{
  nr_async_context_t *context = nr_async_context_create (1);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_time_equal ("NULL context", 0, nr_async_context_get_duration (NULL));
  tlib_pass_if_time_equal ("unstopped context", 0, nr_async_context_get_duration (context));

  /*
   * Test : Normal operation.
   */
  nr_async_context_add (context, 2);
  nr_async_context_end (context, 42);
  tlib_pass_if_time_equal ("wallclock > async_duration", 0, nr_async_context_get_duration (context));

  nr_async_context_add (context, 39);
  tlib_pass_if_time_equal ("wallclock == async_duration", 0, nr_async_context_get_duration (context));

  nr_async_context_add (context, 10);
  tlib_pass_if_time_equal ("wallclock < async_duration", 10, nr_async_context_get_duration (context));

  nr_async_context_destroy (&context);
}

void
test_main (void *p NRUNUSED)
{
  test_create_destroy ();
  test_add ();
  test_end ();
  test_get_duration ();
}
