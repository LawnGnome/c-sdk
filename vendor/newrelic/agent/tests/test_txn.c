#include "tlib_php.h"

#include "php_agent.h"
#include "php_header.h"
#include "php_txn_private.h"

static void test_handle_fpm_error(void) {
  nrobj_t* agent_attributes;
  char* sapi_name;
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  /*
   * We're setting up our own engine instance because we need to control the
   * attribute configuration.
   */
  tlib_php_engine_create(
      "newrelic.transaction_events.attributes.include=request.uri" PTSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  nr_php_txn_handle_fpm_error(NULL TSRMLS_CC);

  /*
   * Test : Non-FPM. (By default, the unit tests report the SAPI as embed, so
   *        the PHP-FPM behaviour won't be triggered.)
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_URI, (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * The next few tests will all pretend to be FPM, so let's set the SAPI name
   * accordingly.
   */
  sapi_name = sapi_module.name;
  sapi_module.name = "fpm-fcgi";

  /*
   * Test : FPM, but with at least one frame called.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  tlib_php_request_eval("$a = 1 + 1; // create a PHP call frame" TSRMLS_CC);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_URI, (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * Test : FPM, but with a non-URI path set.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_ACTION,
                         (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * Test : FPM, with the specific case that should result in a status code
   *        based transaction name: a fallback URI path, plus a zero call count
   *        in PHP (since no user function or file is ever executed in the
   *        request).
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_sapi_headers(TSRMLS_C)->http_response_code = 404;
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be updated", "404",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be updated",
                         (int)NR_PATH_TYPE_STATUS_CODE,
                         (int)NRTXN(status).path_type);

  agent_attributes = nr_attributes_agent_to_obj(NRTXN(attributes),
                                                NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_not_null("agent attributes must be defined", agent_attributes);
  tlib_pass_if_str_equal(
      "agent attributes must include a request.uri with the original path",
      "foo", nro_get_hash_string(agent_attributes, "request.uri", NULL));
  nro_delete(agent_attributes);

  tlib_php_request_end();

  /*
   * Put the SAPI name back to what it should be.
   */
  sapi_module.name = sapi_name;

  tlib_php_engine_destroy(TSRMLS_C);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_handle_fpm_error();
}
