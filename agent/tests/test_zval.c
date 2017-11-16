#include "tlib_php.h"

#include "php_agent.h"
#include "php_zval.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = -1, .state_size = 0};

static void
test_valid_callable (const char *expr TSRMLS_DC)
{
  zval *zv = tlib_php_request_eval_expr (expr TSRMLS_CC);

  tlib_fail_if_int_equal (expr, 0, nr_php_is_zval_valid_callable (zv TSRMLS_CC));
  nr_php_zval_free (&zv);
}

static void
test_is_zval_valid_callable (TSRMLS_D)
{
  size_t i;
  zval **invalid_zvals;

  tlib_php_request_start ();

  tlib_pass_if_int_equal ("NULL zval", 0, nr_php_is_zval_valid_callable (NULL TSRMLS_CC));

  invalid_zvals = tlib_php_zvals_of_all_types (TSRMLS_C);
  for (i = 0; invalid_zvals[i]; i++) {
    tlib_pass_if_int_equal ("non-callable zval", 0, nr_php_is_zval_valid_callable (invalid_zvals[i] TSRMLS_CC));
  }
  tlib_php_free_zval_array (&invalid_zvals);

  test_valid_callable ("'date'" TSRMLS_CC);
  test_valid_callable ("array(new ReflectionFunction('date'), 'isDisabled')" TSRMLS_CC);
  test_valid_callable ("array('ReflectionFunction', 'export')" TSRMLS_CC);
  test_valid_callable ("'ReflectionFunction::export'" TSRMLS_CC);
  test_valid_callable ("function () {}" TSRMLS_CC);

#ifdef PHP7
  test_valid_callable ("new class { function __invoke() {} }" TSRMLS_CC);
#endif /* PHP7 */

  tlib_php_request_end ();
}

void
test_main (void *p NRUNUSED)
{
#if defined(ZTS) && !defined(PHP7)
  void ***tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create ("" PTSRMLS_CC);

  test_is_zval_valid_callable (TSRMLS_C);

  tlib_php_engine_destroy (TSRMLS_C);
}
