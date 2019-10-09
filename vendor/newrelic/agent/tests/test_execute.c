#include "tlib_php.h"

#include "php_agent.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

/*
 * This isn't a true unit test; instead, it's a regression test for PHP-2191 to
 * verify that the reusable segments vector is properly reinitialised for each
 * new transaction. It uses the unit test machinery because that's the easiest
 * way to set up the conditions for the test.
 */
static void test_reusable_segments(TSRMLS_D) {
  unsigned int calls = 0;
  const unsigned int limit = 100;

  tlib_php_request_start();

  /*
   * Basically, within a single request, we want to create and discard some
   * segments, then restart the transaction, then create some new segments.
   * Since we need to exercise nr_php_segment_start() and
   * nr_php_segment_discard(), we can't do this directly, but we can run a fast
   * PHP function until we know we have a reusable segment.
   *
   * So, let's define a fast function.
   */
  tlib_php_request_eval("function speedy_gonzales() {}" TSRMLS_CC);

  /*
   * Now let's run it until we have at least one reusable segment.
   */
  while (0 == nr_vector_size(NRTXNGLOBAL(reusable_segments))) {
    tlib_php_request_eval("speedy_gonzales();" TSRMLS_CC);

    /*
     * This shouldn't take more than one iteration, but if the machine is too
     * slow or overloaded to run a PHP function in less than 2 microseconds,
     * we'll put a circuit breaker in that just fails the test for further
     * investigation.
     */
    calls += 1;
    tlib_pass_if_true(
        "unable to generate a discarded segment; is the machine too slow to "
        "run a PHP function under the interesting threshold?",
        calls < limit, "calls=%u limit=%u", calls, limit);
  }

  /*
   * Restart the transaction.
   */
  tlib_pass_if_status_success("ending the transaction should succeed",
                              nr_php_txn_end(1, 0 TSRMLS_CC));
  tlib_pass_if_status_success(
      "starting a new transaction should succeed",
      nr_php_txn_begin("PHP Application", NULL TSRMLS_CC));

  /*
   * Now call speedy_gonzales() again a bunch of times.
   */
  for (calls = 0; calls < limit; calls++) {
    tlib_php_request_eval("speedy_gonzales();" TSRMLS_CC);
  }

  /*
   * If we got here and Valgrind didn't scream, we're done.
   */
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);
  test_reusable_segments(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
