/*
 * This file handles the initialization that happens at the beginning of
 * each request.
 */
#include "php_agent.h"
#include "php_autorum.h"
#include "php_error.h"
#include "php_header.h"
#include "php_output.h"
#include "nr_datastore_instance.h"
#include "nr_txn.h"
#include "nr_rum.h"
#include "nr_slowsqls.h"
#include "util_logging.h"
#include "util_strings.h"

static void
nr_php_datastore_instance_destroy (nr_datastore_instance_t *instance)
{
  nr_datastore_instance_destroy (&instance);
}

/*
 * There are some agent initialization tasks that need to be performed after
 * all modules' MINIT functions have been called and the PHP VM is fully up
 * and running. This variable (protected by a mutex) detects that and calls
 * the late initialization function once per process.
 */
static int done_first_rinit_work = 0;
nrthread_mutex_t first_rinit_mutex = NRTHREAD_MUTEX_INITIALIZER;

#ifdef TAGS
void zm_activate_newrelic (void);  /* ctags landing pad only */
#endif
PHP_RINIT_FUNCTION (newrelic)
{
  nr_status_t ret;

  (void)type;
  (void)module_number;

  NRPRG (current_framework) = NR_FW_UNSET;
  NRPRG (framework_version) = 0;
  NRPRG (need_rshutdown_cleanup) = 0;
  NRPRG (php_cur_stack_depth) = 0;
  NRPRG (deprecated_capture_request_parameters) = NRINI (capture_params);
  NRPRG (sapi_headers) = NULL;

  if ((0 == NR_PHP_PROCESS_GLOBALS (enabled)) || (0 == NRINI (enabled))) {
    return SUCCESS;
  }

  if (0 == done_first_rinit_work) {
    nrt_mutex_lock (&first_rinit_mutex); {
      /*
       * Yes we check this again in case another thread snuck in and started
       * doing late initialization already.
       */
      if (0 == done_first_rinit_work) {
        nr_php_late_initialization (TSRMLS_C);
        done_first_rinit_work = 1;
      }
    } nrt_mutex_unlock (&first_rinit_mutex);
  }

  nrl_verbosedebug (NRL_INIT, "RINIT processing started");

  nr_php_exception_filters_init (&NRPRG (exception_filters));
  nr_php_exception_filters_add (&NRPRG (exception_filters), nr_php_ignore_exceptions_ini_filter);

  /*
   * Trigger the _SERVER and _REQUEST auto-globals to initialize.
   *
   * The _SERVER globals can be accessed through PG (http_globals)[TRACK_VARS_SERVER])
   * See nr_php_get_server_global
   *
   * The _REQUEST globals can be accessed through zend_hash_find (&EG (symbol_table), NR_HSTR ("_REQUEST"), ...
   */
  nr_php_zend_is_auto_global (NR_PSTR ("_SERVER") TSRMLS_CC);
  nr_php_zend_is_auto_global (NR_PSTR ("_REQUEST") TSRMLS_CC);

  nr_php_capture_sapi_headers (TSRMLS_C);

  ret = nr_php_txn_begin (0, 0 TSRMLS_CC);
  if (NR_SUCCESS != ret) {
    return SUCCESS;
  }

  /*
   * Install the cross process buffer handler:  See the documentation of
   * nr_php_header_output_handler for explanation of its purpose and the
   * the conditionals.
   */
  if ((NR_STATUS_CROSS_PROCESS_START == NRTXN (status.cross_process)) &&
      nr_php_has_request_header ("HTTP_X_NEWRELIC_ID" TSRMLS_CC)) {
    nr_php_output_install_handler ("New Relic header",
                                   nr_php_header_output_handler TSRMLS_CC);
  }

  if (nr_rum_do_autorum (NRPRG (txn))) {
    nr_php_output_install_handler ("New Relic auto-RUM",
                                   nr_php_rum_output_handler TSRMLS_CC);
  }

  /*
   * Add an exception handler so we can better handle uncaught exceptions.
   */
  nr_php_error_install_exception_handler (TSRMLS_C);

  /*
   * Instrument extensions if we've been asked to and it hasn't already
   * happened.
   */
  if ((NR_PHP_PROCESS_GLOBALS (instrument_extensions)) &&
      (NULL == NRPRG (extensions))) {
    NRPRG (extensions) = nr_php_extension_instrument_create ();
    nr_php_extension_instrument_rescan (NRPRG (extensions) TSRMLS_CC);
  }

  /*
   * Compile regex for WordPress: see PHP-1092 for the reasoning behind the
   * hook sanitization regex.
   */
  NRPRG (wordpress_hook_regex) = nr_regex_create ("(^([a-z_-]+[_-])([0-9a-f_.]+[0-9][0-9a-f.]+)(_{0,1}.*)$|(.*))", NR_REGEX_CASELESS, 0);

  NRPRG (mysql_last_conn) = NULL;
  NRPRG (pgsql_last_conn) = NULL;
  NRPRG (datastore_connections) = nr_hashmap_create ((nr_hashmap_dtor_func_t) nr_php_datastore_instance_destroy);

  NRPRG (need_rshutdown_cleanup) = 1;

  nrl_verbosedebug (NRL_INIT, "RINIT processing done");
  return SUCCESS;
}
