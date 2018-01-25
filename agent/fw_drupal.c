#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_drupal_common.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "nr_header.h"
#include "util_hash.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * Set the WT name to "(cached page)"
 */
NR_PHP_WRAPPER (nr_drupal_name_wt_as_cached_page)
{
  const char *buf = "(cached page)";

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_DRUPAL);

  nr_txn_set_path ("Drupal", NRPRG (txn), buf, NR_PATH_TYPE_ACTION, NR_NOT_OK_TO_OVERWRITE);
  NR_PHP_WRAPPER_CALL;
} NR_PHP_WRAPPER_END

/*
 * Name the WT based on the QDrupal QForm name
 */
NR_PHP_WRAPPER (nr_drupal_qdrupal_name_the_wt)
{
  zval *arg1 = 0;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_DRUPAL);

  arg1 = nr_php_arg_get (1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (nr_php_is_zval_non_empty_string (arg1)) {
    const char prefix[] = "qdrupal_qform/";
    int n = (int)sizeof (prefix);
    char *action = (char *)nr_alloca (Z_STRLEN_P (arg1) + n + 2);

    nr_strcpy (action, prefix);
    nr_strxcpy (action + n, Z_STRVAL_P (arg1), Z_STRLEN_P (arg1));

    nr_txn_set_path ("QDrupal", NRPRG (txn), action, NR_PATH_TYPE_ACTION, NR_NOT_OK_TO_OVERWRITE);
  } else if (0 != arg1) {
    nrl_verbosedebug (NRL_FRAMEWORK, "QDrupal: type=%d", Z_TYPE_P (arg1));
  }
  nr_php_arg_release (&arg1);
  NR_PHP_WRAPPER_CALL;
} NR_PHP_WRAPPER_END

static char *
nr_drupal_http_request_get_response_header (zval **return_value TSRMLS_DC)
{
  zval *headers = 0;
  ulong key_num = 0;
  nr_php_string_hash_key_t *key_str = NULL;
  zval *val = NULL;

  if ((0 == NRPRG (txn)) ||
      (0 == NRPRG (txn)->options.cross_process_enabled)) {
    return NULL;
  }

  if (NULL == return_value) {
    return NULL;
  }

  if (!nr_php_is_zval_valid_object (*return_value)) {
    return NULL;
  }

  headers = nr_php_get_zval_object_property (*return_value, "headers" TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_array (headers)) {
    return NULL;
  }

  /*
   * In Drupal 7 the header names are lowercased and in Drupal 6 they are
   * unaltered.  Therefore we do a case-insensitive lookup.
   */
  ZEND_HASH_FOREACH_KEY_VAL (Z_ARRVAL_P (headers), key_num, key_str, val) {
    (void) key_num;

    if ((NULL == key_str) || (0 == nr_php_is_zval_non_empty_string (val))) {
      continue;
    }

    if (0 == nr_strnicmp (ZEND_STRING_VALUE (key_str),
                          NR_PSTR (X_NEWRELIC_APP_DATA))) {
      return nr_strndup (Z_STRVAL_P (val), Z_STRLEN_P (val));
    }
  } ZEND_HASH_FOREACH_END ();

  return NULL;
}

/*
 * Drupal 6:
 *   drupal_http_request ($url, $headers = array(), $method = 'GET',
 *                        $data = NULL, $retry = 3, $timeout = 30.0)
 *
 * Drupal 7:
 *   drupal_http_request ($url, array $options = array())
 *
 */
NR_PHP_WRAPPER (nr_drupal_http_request_exec)
{
  zval *arg1 = 0;
  zval **return_value = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_DRUPAL);

  NRPRG (drupal_http_request_depth) += 1;

  /*
   * Grab the URL for the external metric, which is the first parameter in all
   * versions of Drupal.
   */
  arg1 = nr_php_arg_get (1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_non_empty_string (arg1)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  return_value = nr_php_get_return_value_ptr (TSRMLS_C);

  /*
   * We only want to create a metric here if this isn't a recursive call to
   * drupal_http_request() caused by the original call returning a redirect.
   * (See also: PHP-521.)
   *
   * Previously we jumped through hoops looking at the PHP call stack due to
   * our own instrumentation making recursive calls to drupal_http_request() in
   * some circumstances, but we can now simply look at how many
   * drupal_http_request() calls are on the stack by checking a counter.
   */
  if (1 == NRPRG (drupal_http_request_depth)) {
    nr_node_external_params_t external_params = {
      .library = "Drupal",
      .url     = Z_STRVAL_P (arg1),
      .urllen  = (size_t) Z_STRLEN_P (arg1),
    };

    nr_txn_set_time (NRPRG (txn), &external_params.start);

    /*
     * Our wrapper for drupal_http_request (which we installed in
     * nr_drupal_replace_http_request()) will take care of adding the request
     * headers, so let's just go ahead and call the function.
     */
    NR_PHP_WRAPPER_CALL;

    nr_txn_set_time (NRPRG (txn), &external_params.stop);

    external_params.encoded_response_header = nr_drupal_http_request_get_response_header (return_value TSRMLS_CC);

    if (NRPRG (txn) && NRTXN (special_flags.debug_cat)) {
      nrl_verbosedebug (NRL_CAT,
        "CAT: outbound response: transport='Drupal 6-7' %s=" NRP_FMT,
        X_NEWRELIC_APP_DATA, NRP_CAT (external_params.encoded_response_header));
    }

    nr_txn_end_node_external (NRPRG (txn), &external_params);

    nr_free (external_params.encoded_response_header);
  } else {
    NR_PHP_WRAPPER_CALL;
  }

end:
  nr_php_arg_release (&arg1);
  NRPRG (drupal_http_request_depth) -= 1;
} NR_PHP_WRAPPER_END

static void
nr_drupal_name_the_wt (const zend_function *func TSRMLS_DC)
{
  char *action = 0;

  if ((NULL == func) || (NULL == nr_php_function_name (func))) {
    return;
  }

  action = nr_strndup (nr_php_function_name (func),
                       (int) nr_php_function_name_length (func));

  nr_txn_set_path ("Drupal", NRPRG (txn), action, NR_PATH_TYPE_ACTION, NR_NOT_OK_TO_OVERWRITE);

  nr_free (action);
}

/*
 * Purpose : Given a function that is a hook function in a module, determine
 *           which component is the module and which is the hook, given that we
 *           know the hook from the module_invoke_all() call.
 */
static nr_status_t
module_invoke_all_parse_module_and_hook (char **module_ptr,
                                         size_t *module_len_ptr,
                                         const char *hook,
                                         size_t hook_len,
                                         const zend_function *func)
{
  const char *module_hook = NULL;
  size_t module_hook_len;
  char *module = NULL;
  size_t module_len;

  *module_ptr = NULL;
  *module_len_ptr = 0;

  if (NULL == func) {
    nrl_verbosedebug (NRL_FRAMEWORK, "%s: func is NULL", __func__);
    return NR_FAILURE;
  }

  module_hook = nr_php_function_name (func);
  module_hook_len = (size_t) nr_php_function_name_length (func);

  if ((0 == module_hook) || (module_hook_len <= 0)) {
    nrl_verbosedebug (NRL_FRAMEWORK, "%s: cannot get function name", __func__);
    return NR_FAILURE;
  }

  if (hook_len >= module_hook_len) {
    nrl_verbosedebug (NRL_FRAMEWORK,
                      "%s: hook length (%zu) is greater than the full module "
                      "hook function length (%zu); "
                      "hook='%.*s'; module_hook='%.*s'",
                      __func__, hook_len, module_hook_len,
                      NRSAFELEN (hook_len), NRSAFESTR (hook),
                      NRSAFELEN (module_hook_len), NRSAFESTR (module_hook));
    return NR_FAILURE;
  }

  module_len = (size_t) nr_strnidx (module_hook, hook, module_hook_len) - 1; /* Subtract 1 for underscore separator */

  if (module_len == 0) {
    nrl_verbosedebug (NRL_FRAMEWORK,
                      "%s: cannot find hook in module hook; "
                      "hook='%.*s'; module_hook='%.*s'",
                      __func__,
                      NRSAFELEN (hook_len), NRSAFESTR (hook),
                      NRSAFELEN (module_hook_len), NRSAFESTR (module_hook));
    return NR_FAILURE;
  }

  module = nr_strndup (module_hook, module_len);

  *module_ptr = module;
  *module_len_ptr = (size_t) module_len;

  return NR_SUCCESS;
}

/*
 * Purpose : Wrap the given function using the current module_invoke_all()
 *           context (encapsulated within the drupal_module_invoke_all_hook and
 *           drupal_module_invoke_all_hook_len per request global fields).
 */
static void
nr_drupal_wrap_hook_within_module_invoke_all (const zend_function *func TSRMLS_DC)
{
  nr_status_t rv;
  char *module = 0;
  size_t module_len = 0;

  /*
   * Since this function is only called if the immediate caller is
   * module_invoke_all(), the drupal_module_invoke_all_hook global should be
   * available.
   */
  if (NULL == NRPRG (drupal_module_invoke_all_hook)) {
    nrl_verbosedebug (NRL_FRAMEWORK,
                      "%s: cannot extract module name without knowing the hook",
                      __func__);
    return;
  }

  rv = module_invoke_all_parse_module_and_hook (&module, &module_len,
                                                NRPRG (drupal_module_invoke_all_hook),
                                                NRPRG (drupal_module_invoke_all_hook_len),
                                                func);

  if (NR_SUCCESS != rv) {
    return;
  }

  nr_php_wrap_user_function_drupal (
    nr_php_function_name (func), nr_php_function_name_length (func),
    module, module_len,
    NRPRG (drupal_module_invoke_all_hook),
    NRPRG (drupal_module_invoke_all_hook_len) TSRMLS_CC);

  nr_free (module);
}

/*
 * Purpose : Wrap calls to call_user_func_array for two reasons identified
 *           by specific call stacks.
 *
 * Transaction naming:
 *
 *   1. call_user_func_array
 *   2. menu_execute_active_handler
 *
 * Module/Hook metric generation:
 *
 *   1. call_user_func_array
 *   2. module_invoke_all
 *
 */
static void
nr_drupal_call_user_func_array_callback (zend_function *func,
                                         const zend_function *caller NRUNUSED TSRMLS_DC)
{
  const char *caller_name;

  if (nrunlikely (NULL == caller)) {
    return;
  }

  if (!nr_drupal_is_framework (NRPRG (current_framework))) {
    return;
  }

  caller_name = nr_php_function_name (caller);

  /*
   * If the caller was module_invoke_all, then perform hook/module
   * instrumentation. This caller is checked first, since it occurs most
   * frequently.
   */
  if (NRINI (drupal_modules)) {
    if (0 == nr_strncmp (caller_name, NR_PSTR ("module_invoke_all"))) {
      nr_drupal_wrap_hook_within_module_invoke_all (func TSRMLS_CC);
      return;
    }
  }

  if (0 == nr_strncmp (caller_name, NR_PSTR ("menu_execute_active_handler"))) {
    nr_drupal_name_the_wt (func TSRMLS_CC);
  }
}

/*
 * Purpose : Wrap view::execute in order to create Drupal Views metrics.
 */
NR_PHP_WRAPPER (nr_drupal_wrap_view_execute)
{
  zval *this_var = 0;
  zval *name_property = 0;
  int name_len;
  char *name = 0;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_DRUPAL);

  this_var = nr_php_scope_get (NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (!nr_php_is_zval_valid_object (this_var)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  name_property = nr_php_get_zval_object_property (this_var, "name" TSRMLS_CC);
  if (0 == nr_php_is_zval_non_empty_string (name_property)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  name_len = Z_STRLEN_P (name_property);
  name = nr_strndup (Z_STRVAL_P (name_property), name_len);

  zcaught = nr_drupal_do_view_execute (name, name_len, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  was_executed = 1;

leave:
  nr_free (name);
  nr_php_scope_release (&this_var);
} NR_PHP_WRAPPER_END

NR_PHP_WRAPPER (nr_drupal_cron_run)
{
  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_DRUPAL);

  nr_txn_set_as_background_job (NRPRG (txn), "drupal_cron_run called");

  NR_PHP_WRAPPER_CALL;
} NR_PHP_WRAPPER_END

static void
nr_drupal_replace_http_request (TSRMLS_D)
{
  zend_function *orig;
  zend_function *wrapper;

  orig = nr_php_find_function ("drupal_http_request" TSRMLS_CC);
  wrapper = nr_php_find_function ("newrelic_drupal_http_request" TSRMLS_CC);

  /*
   * Add a function that will replace drupal_http_request() and ensure that we
   * add our request headers for CAT.
   *
   * There is an oddity in here: this function looks like it makes a recursive
   * call to newrelic_drupal_http_request(), but in fact that will be the
   * original drupal_http_request(), as we'll swap the implementations.
   *
   * We can't do this until the original drupal_http_request() is defined,
   * which may not be the case immediately if the framework has been forced.
   */
  if (orig && !wrapper) {
    int argc = (int)orig->common.num_args;

    /*
     * Drupal 6 and 7 have slightly different APIs, so we'll use different
     * wrappers for each. This is slightly tricky in practice. The Pressflow
     * fork of Drupal 6 has backported features from Drupal 7 that cause the
     * agent to detect it as Drupal 7 rather than Drupal 6. Therefore, the
     * detected framework version can't be used to determine which variant
     * of drupal_http_request to replace. Instead, differentiate the two
     * variants based on their function signatures.
     */
    if (6 == argc) { /* The Drupal 6 variant accepts six arguments. */
      int retval;

      retval = zend_eval_string (
        "function newrelic_drupal_http_request($url, $headers = array(), $method = 'GET', $data = null, $retry = 3, $timeout = 30.0) {"
        "  $metadata = newrelic_get_request_metadata('Drupal 6');"
        "  if (is_array($headers)) {"
        "    $headers = array_merge($headers, $metadata);"
        "  } elseif (is_null($headers)) {"
        "    $headers = $metadata;"
        "  }"
        "  $result = newrelic_drupal_http_request($url, $headers, $method, $data, $retry, $timeout);"
        "  return $result;"
        "}"
        , NULL, "newrelic/drupal6" TSRMLS_CC);

      if (SUCCESS != retval) {
        nrl_warning (NRL_FRAMEWORK, "%s: error evaluating Drupal 6 code", __func__);
      }
    } else if (2 == argc) { /* The Drupal 7 variant accepts two arguments. */
      int retval;

      retval = zend_eval_string (
        "function newrelic_drupal_http_request($url, array $options = array()) {"
        "  $metadata = newrelic_get_request_metadata('Drupal 7');"
        /*
         * array_key_exists() is used instead of isset() because isset() will
         * return false if $options['headers'] exists but is null. As noted
         * below, we need to pass the value through and not accidentally set it
         * to a valid value.
         */
        "  if (array_key_exists('headers', $options)) {"
        "    if (is_array($options['headers'])) {"
        "      $options['headers'] += $metadata;"
        "    }"
        /*
         * We do nothing here if $options['headers'] is set but invalid (ie not
         * an array) because drupal_http_request() will generate an
         * "unsupported operand types" fatal error that we don't want to squash
         * by accident (since we don't want to change behaviour).
         */
        "  } else {"
        "    $options['headers'] = $metadata;"
        "  }"
        "  $result = newrelic_drupal_http_request($url, $options);"
        "  return $result;"
        "}"
        , NULL, "newrelic/drupal7" TSRMLS_CC);

      if (SUCCESS != retval) {
        nrl_warning (NRL_FRAMEWORK, "%s: error evaluating Drupal 7 code", __func__);
      }
    } else {
      nrl_info (NRL_FRAMEWORK, "%s: unable to determine drupal_http_request"
        " variant: num_args=%d", __func__, argc);
    }

    wrapper = nr_php_find_function ("newrelic_drupal_http_request" TSRMLS_CC);
    nr_php_swap_user_functions (orig, wrapper);
  }
}

NR_PHP_WRAPPER (nr_drupal_wrap_module_invoke)
{
  nrtxntime_t start;
  nrtxntime_t stop;
  int module_len;
  int hook_len;
  char *module = 0;
  char *hook = 0;
  nrtime_t duration;
  nrtime_t exclusive;
  nrtime_t kids_duration = 0;
  nrtime_t *kids_duration_save = NRPRG (cur_drupal_module_kids_duration);
  zval *arg1 = 0;
  zval *arg2 = 0;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_DRUPAL);

  arg1 = nr_php_arg_get (1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  arg2 = nr_php_arg_get (2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if ((0 == nr_php_is_zval_non_empty_string (arg1)) ||
      (0 == nr_php_is_zval_non_empty_string (arg2))) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  module_len = Z_STRLEN_P (arg1);
  module = nr_strndup (Z_STRVAL_P (arg1), module_len);
  hook_len = Z_STRLEN_P (arg2);
  hook = nr_strndup (Z_STRVAL_P (arg2), hook_len);

  start.stamp = 0;
  start.when = 0;
  nr_txn_set_time (NRPRG (txn), &start);

  NRPRG (cur_drupal_module_kids_duration) = &kids_duration;
  NR_PHP_WRAPPER_CALL;
  NRPRG (cur_drupal_module_kids_duration) = kids_duration_save;

  stop.stamp = 0;
  stop.when = 0;
  if (NR_SUCCESS != nr_txn_set_stop_time (NRPRG (txn), &start, &stop)) {
    goto leave;
  }

  if (nrlikely (stop.when > start.when)) {
    duration = stop.when - start.when;
  } else {
    duration = 0;
  }

  if (nrlikely (duration > kids_duration)) {
    exclusive = duration - kids_duration;
  } else {
    exclusive = 0;
  }

  if (kids_duration_save) {
    *kids_duration_save += duration;
  }

  nr_drupal_create_metric (NRPRG (txn), NR_PSTR (NR_DRUPAL_MODULE_PREFIX), module, module_len, duration, exclusive);
  nr_drupal_create_metric (NRPRG (txn), NR_PSTR (NR_DRUPAL_HOOK_PREFIX), hook, hook_len, duration, exclusive);

leave:
    nr_free (module);
    nr_free (hook);
    nr_php_arg_release (&arg1);
    nr_php_arg_release (&arg2);
} NR_PHP_WRAPPER_END

NR_PHP_WRAPPER (nr_drupal_wrap_module_invoke_all)
{
  zval *hook;
  char *prev_hook;
  nr_string_len_t prev_hook_len;

  (void) wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_DRUPAL);

  hook = nr_php_arg_get (1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_non_empty_string (hook)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  prev_hook = NRPRG (drupal_module_invoke_all_hook);
  prev_hook_len = NRPRG (drupal_module_invoke_all_hook_len);
  NRPRG (drupal_module_invoke_all_hook) = nr_strndup (Z_STRVAL_P (hook),
                                                      Z_STRLEN_P (hook));
  NRPRG (drupal_module_invoke_all_hook_len) = Z_STRLEN_P (hook);

  NR_PHP_WRAPPER_CALL;

  nr_free (NRPRG (drupal_module_invoke_all_hook));
  NRPRG (drupal_module_invoke_all_hook) = prev_hook;
  NRPRG (drupal_module_invoke_all_hook_len) = prev_hook_len;

leave:
  nr_php_arg_release (&hook);
} NR_PHP_WRAPPER_END

/*
 * Enable the drupal instrumentation
 */
void
nr_drupal_enable (TSRMLS_D)
{
  nr_php_add_call_user_func_array_pre_callback (nr_drupal_call_user_func_array_callback TSRMLS_CC);
  nr_php_wrap_user_function (NR_PSTR ("QFormBase::Run"), nr_drupal_qdrupal_name_the_wt TSRMLS_CC);
  nr_php_wrap_user_function (NR_PSTR ("drupal_page_cache_header"), nr_drupal_name_wt_as_cached_page TSRMLS_CC);
  nr_php_wrap_user_function (NR_PSTR ("drupal_cron_run"), nr_drupal_cron_run TSRMLS_CC);
  nr_php_wrap_user_function (NR_PSTR ("drupal_http_request"), nr_drupal_http_request_exec TSRMLS_CC);

  /*
   * The drupal_modules config setting controls instrumentation of modules,
   * hooks, and views.
   */
  if (NRINI (drupal_modules)) {
    nr_php_wrap_user_function (NR_PSTR ("module_invoke"), nr_drupal_wrap_module_invoke TSRMLS_CC);
    nr_php_wrap_user_function (NR_PSTR ("module_invoke_all"), nr_drupal_wrap_module_invoke_all TSRMLS_CC);
    nr_php_wrap_user_function (NR_PSTR ("view::execute"), nr_drupal_wrap_view_execute TSRMLS_CC);
  }

  nr_php_user_function_add_declared_callback (NR_PSTR ("drupal_http_request"),
                                              nr_drupal_replace_http_request TSRMLS_CC);
}
