#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"

/*
 * This instruments Silex 1.x, and at the time of writing (March 30, 2015),
 * also instruments the pre-alpha Silex 2.0.
 *
 * This support is intentionally simple: while Silex permits many components to
 * be swapped out, the framework itself is so simple that it's not unreasonable
 * to expect users who do so to also make newrelic_name_transaction calls. At
 * any rate, the call we need to instrument is only defined on HttpKernel, not
 * the HttpKernelInterface contract it implements, so a user who's replaced the
 * kernel probably won't reimplement this method and will instead handle their
 * routing some other way.
 */

NR_PHP_WRAPPER(nr_silex_name_the_wt) {
  zval* attributes = NULL;
  zval* name = NULL;
  char* path = NULL;
  zval* request = NULL;
  zval* route = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SILEX);

  request = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_object_instanceof_class(
          request, "Symfony\\Component\\HttpFoundation\\Request" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: first parameter isn't a Request object", __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  /*
   * The approach here is straightforward: the first parameter to
   * HttpKernel::handleRaw() is a Request object, which has an attributes
   * property, which contains a _route property (accessible via a get() method)
   * that is either a good autogenerated name (based on the route pattern) or
   * the controller name the user provided explictly via the Controller::bind()
   * method.
   *
   * Either way, let's grab it and name the transaction from it.
   */

  attributes = nr_php_get_zval_object_property(request, "attributes" TSRMLS_CC);
  if (!nr_php_object_instanceof_class(
          attributes,
          "Symfony\\Component\\HttpFoundation\\ParameterBag" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: Request::$attributes isn't a ParameterBag object",
                     __func__);
    goto end;
  }

  name = nr_php_zval_alloc();
  nr_php_zval_str(name, "_route");

  route = nr_php_call(attributes, "get", name);
  if (!nr_php_is_zval_non_empty_string(route)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: _route is not a valid string",
                     __func__);
    goto end;
  }

  path = nr_strndup(Z_STRVAL_P(route), Z_STRLEN_P(route));

  /*
   * This is marked as not OK to overwrite as we're unwinding the stack (due to
   * this being a post-handler), and we want the innermost name to win to
   * handle forwarded subrequests.
   */
  nr_txn_set_path("Silex", NRPRG(txn), path, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  /* FALLTHROUGH */

end:
  nr_php_zval_free(&name);
  nr_free(path);
  nr_php_zval_free(&route);
  nr_php_arg_release(&request);
}
NR_PHP_WRAPPER_END

void nr_silex_enable(TSRMLS_D) {
  NR_UNUSED_TSRMLS;

  nr_php_wrap_user_function(
      NR_PSTR("Symfony\\Component\\HttpKernel\\HttpKernel::handleRaw"),
      nr_silex_name_the_wt TSRMLS_CC);
}
