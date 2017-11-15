#include "php_agent.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_drupal_common.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_memory.h"
#include "util_strings.h"

int
nr_drupal_do_view_execute (const char *name, int name_len, NR_EXECUTE_PROTO TSRMLS_DC)
{
  int zcaught;
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtime_t duration;
  nrtime_t exclusive;
  nrtime_t kids_duration = 0;
  nrtime_t *kids_duration_save = NRPRG (cur_drupal_view_kids_duration);

  start.stamp = 0;
  start.when = 0;
  nr_txn_set_time (NRPRG (txn), &start);

  NRPRG (cur_drupal_view_kids_duration) = &kids_duration;
  zcaught = nr_zend_call_orig_execute (NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  NRPRG (cur_drupal_view_kids_duration) = kids_duration_save;

  stop.stamp = 0;
  stop.when = 0;
  if (NR_SUCCESS != nr_txn_set_stop_time (NRPRG (txn), &start, &stop)) {
    return zcaught;
  }

  if (stop.when > start.when) {
    duration = stop.when - start.when;
  } else {
    duration = 0;
  }

  if (duration > kids_duration) {
    exclusive = duration - kids_duration;
  } else {
    exclusive = 0;
  }

  if (kids_duration_save) {
    *kids_duration_save += duration;
  }

  nr_drupal_create_metric (NRPRG (txn), NR_PSTR (NR_DRUPAL_VIEW_PREFIX), name, name_len, duration, exclusive);

  return zcaught;
}

void
nr_drupal_create_metric (nrtxn_t *txn, const char *prefix, int prefix_len, const char *suffix, int suffix_len, nrtime_t duration, nrtime_t exclusive)
{
  char *name = 0;
  char *nm = 0;

  name = (char *)nr_alloca (prefix_len + suffix_len + 2);
  nm = nr_strxcpy (name, prefix, prefix_len);
  nr_strxcpy (nm, suffix, suffix_len);

  nrm_add_ex (txn->unscoped_metrics, name, duration, exclusive);
}

int
nr_drupal_is_framework (nrframework_t fw)
{
  return ((NR_FW_DRUPAL == fw) || (NR_FW_DRUPAL8 == fw));
}

/*
 * Purpose : Wrap a module hook function to generate module and hook metrics.
 */
NR_PHP_WRAPPER (nr_drupal_wrap_module_hook)
{
  if (!nr_drupal_is_framework (NRPRG (current_framework))) {
    NR_PHP_WRAPPER_LEAVE;
  }

  /*
   * We can't infer the module and hook names from the function name, since a
   * function such as a_b_c is ambiguous (is the module a or a_b?). Instead,
   * we'll see if they're defined in the wraprec.
   */
  if ((NULL != wraprec->drupal_hook) && (NULL != wraprec->drupal_module)) {
    nrtxntime_t start;
    nrtxntime_t stop;
    nrtime_t duration;
    nrtime_t exclusive;
    nrtime_t kids_duration = 0;
    nrtime_t *kids_duration_save = NRPRG (cur_drupal_module_kids_duration);

    nr_txn_set_time (NRPRG (txn), &start);

    NRPRG (cur_drupal_module_kids_duration) = &kids_duration;
    NR_PHP_WRAPPER_CALL;
    NRPRG (cur_drupal_module_kids_duration) = kids_duration_save;

    stop.stamp = 0;
    stop.when = 0;
    if (NR_SUCCESS != nr_txn_set_stop_time (NRPRG (txn), &start, &stop)) {
      NR_PHP_WRAPPER_LEAVE;
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

    nr_drupal_create_metric (NRPRG (txn), NR_PSTR (NR_DRUPAL_MODULE_PREFIX), wraprec->drupal_module, wraprec->drupal_module_len, duration, exclusive);
    nr_drupal_create_metric (NRPRG (txn), NR_PSTR (NR_DRUPAL_HOOK_PREFIX), wraprec->drupal_hook, wraprec->drupal_hook_len, duration, exclusive);
  } else {
    NR_PHP_WRAPPER_CALL;
  }
} NR_PHP_WRAPPER_END

nruserfn_t *
nr_php_wrap_user_function_drupal (
  const char *name,   int namelen,
  const char *module, nr_string_len_t module_len,
  const char *hook,   nr_string_len_t hook_len TSRMLS_DC)
{
  nruserfn_t *wraprec;

  /*
   * TODO(aharvey): figure out if there's ever a scenario in which the hook and
   * module names can change below, because if not, we can skip doing the
   * free/malloc cycle each time as long as they're not null.
   */

  wraprec = nr_php_wrap_user_function (name, namelen, nr_drupal_wrap_module_hook TSRMLS_CC);
  if (wraprec) {
    /*
     * As wraprecs can be reused, we need to free any previous hook or module
     * to avoid memory leaks.
     */
    nr_free (wraprec->drupal_hook);
    nr_free (wraprec->drupal_module);

    wraprec->drupal_hook = nr_strndup (hook, hook_len);
    wraprec->drupal_hook_len = hook_len;
    wraprec->drupal_module = nr_strndup (module, module_len);
    wraprec->drupal_module_len = module_len;
  }

  return wraprec;
}

void
nr_drupal_hook_instrument (const char *module, size_t module_len,
                           const char *hook, size_t hook_len TSRMLS_DC)
{
  size_t function_name_len = 0;
  char *function_name = NULL;

  /*
   * Construct the name of the function we need to instrument from the module
   * and hook names.
   */
  function_name_len = module_len + hook_len + 2;
  function_name = nr_alloca (function_name_len);

  nr_strxcpy (function_name, module, module_len);
  nr_strcat (function_name, "_");
  nr_strncat (function_name, hook, hook_len);

  /*
   * Actually instrument the function.
   */
  nr_php_wrap_user_function_drupal (
    function_name, function_name_len - 1,
    module, module_len,
    hook, hook_len TSRMLS_CC);
}
