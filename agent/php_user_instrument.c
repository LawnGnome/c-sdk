#include "php_agent.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "lib_guzzle_common.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * All of the fuss of zend_try .. zend_catch .. zend_end_try
 * is isolated into a handful of functions below.
 *
 * These are standalone functions so that the setjmp/longjmp
 * entailed in the implementation of them has a well defined stack frame,
 * without any variable sized objects in that stack frame, thus
 * giving longjmp a simple well defined place to come back to.
 * Having these standalone functions eliminates gcc compiler warning messages -Wclobbered.
 *
 * These functions call through to the wrapped handler in various
 * ways. These C function does not create another stack frame so the
 * instrumentation is invisible to all the php introspection functions,
 * e.g., stack dumps, etc.
 *
 * The zend internal exception throwing mechanism (which triggers the
 * zend_try, zend_catch and zend_end_try code blocks) is used when:
 *   (a) there's an internal error in the zend engine, including:
 *     (1) bad code byte;
 *     (2) corrupted APC cache;
 *   (b) the PHP program calls exit.
 *   (c) An internall call to zend_error_cb, as for example empirically due to one of:
 *        E_ERROR, E_PARSE, E_CORE_ERROR, E_CORE_WARNING, E_COMPILE_ERROR, E_COMPILE_WARNING
 * Cases (b) and (c) are interesting, as it is not really an error condition,
 * but merely a fast path out of the interpreter.
 *
 * Note that zend exceptions are NOT thrown when PHP throws exceptions;
 * PHP exceptions are handled at a higher layer.
 *
 * Note that if the wrapped function throws a zend exception,
 * the New Relic post dispatch handler is not called.
 *
 * Many functions here call zend_bailout to continue handling fatal PHP errors,
 * Since zend_bailout calls longjmp it never returns.
 */
int
nr_zend_call_orig_execute (NR_EXECUTE_PROTO TSRMLS_DC)
{
  volatile int zcaught = 0;
  zend_try {
    NR_PHP_PROCESS_GLOBALS (orig_execute) (NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  } zend_catch {
    zcaught = 1;
  } zend_end_try ();
  return zcaught;
}

int
nr_zend_call_orig_execute_special (nruserfn_t *wraprec, NR_EXECUTE_PROTO TSRMLS_DC)
{
  volatile int zcaught = 0;
  zend_try {
    if (wraprec->special_instrumentation) {
      wraprec->special_instrumentation (wraprec, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    } else {
      NR_PHP_PROCESS_GLOBALS (orig_execute) (NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    }
  } zend_catch {
    zcaught = 1;
  } zend_end_try ();
  return zcaught;
}

nruserfn_t *
nr_php_op_array_get_wraprec (const zend_op_array *op_array)
{
  return (nruserfn_t *)op_array->reserved[NR_PHP_PROCESS_GLOBALS (zend_offset)];
}

/*
 * Wrap an existing user-defined (written in PHP) function with an
 * instrumentation function. Actually, what we do is just set a pointer in the
 * reserved resources section of the op_array: the pointer points to the
 * wraprec. Non-wrapped functions have a 0 pointer in that field and thus
 * the execution function can quickly determine whether a user-defined function
 * is instrumented.
 *
 * nr_php_wrap_user_function_internal is the usual function that is used;
 * nr_php_wrap_zend_function is available for situations where we don't want to
 * (or can't) match by name and have the zend_function available. (The main use
 * case for this is to allow instrumenting closures, but it's useful anywhere
 * we're dealing with a callable rather than a name.)
 */
static void
nr_php_wrap_zend_function (zend_function *func, nruserfn_t *wraprec TSRMLS_DC)
{
  func->op_array.reserved[NR_PHP_PROCESS_GLOBALS (zend_offset)] = wraprec;
#ifndef PHP7
  func->common.fn_flags |= NR_PHP_ACC_INSTRUMENTED;
#endif /* PHP7 */
  wraprec->is_wrapped = 1;

  if (wraprec->declared_callback) {
    (wraprec->declared_callback) (TSRMLS_C);
  }
}

static void
nr_php_wrap_user_function_internal (nruserfn_t *wraprec TSRMLS_DC)
{
  zend_function *orig_func = 0;

  if (0 == NR_PHP_PROCESS_GLOBALS (done_instrumentation)) {
    return;
  }

  if (wraprec->is_wrapped) {
    return;
  }

  if (nrunlikely (-1 == NR_PHP_PROCESS_GLOBALS (zend_offset))) {
    return;
  }

  if (0 == wraprec->classname) {
    orig_func = nr_php_find_function (wraprec->funcnameLC TSRMLS_CC);
  } else {
    zend_class_entry *orig_class = 0;

    orig_class = nr_php_find_class (wraprec->classnameLC TSRMLS_CC);
    orig_func = nr_php_find_class_method (orig_class, wraprec->funcnameLC);
  }

  if (NULL == orig_func) {
    /* It could be in a file not yet loaded, no reason to log anything. */
    return;
  }

  if (ZEND_USER_FUNCTION != orig_func->type) {
    nrl_verbosedebug (NRL_INSTRUMENT, "%s%s%s is not a user function",
      wraprec->classname ? wraprec->classname : "",
      wraprec->classname ? "::" : "",
      wraprec->funcname);

    /*
     * Prevent future wrap attempts for performance and to prevent spamming the
     * logs with this message.
     */
    wraprec->is_disabled = 1;
    return;
  }

  nr_php_wrap_zend_function (orig_func, wraprec TSRMLS_CC);
}

static nruserfn_t *
nr_php_user_wraprec_create (void)
{
  return (nruserfn_t *) nr_zalloc (sizeof (nruserfn_t));
}

static nruserfn_t *
nr_php_user_wraprec_create_named (const char *full_name, int full_name_len)
{
  int i;
  const char *name;
  const char *klass;
  int name_len;
  int klass_len;
  nruserfn_t *wraprec;

  if (0 == full_name) {
    return 0;
  }
  if (full_name_len <= 0) {
    return 0;
  }

  name = full_name;
  name_len = full_name_len;
  klass = 0;
  klass_len = 0;

  /* If class::method, then break into two strings */
  for (i = 0; i < full_name_len; i++) {
    if ((':' == full_name[i]) && (':' == full_name[i + 1])) {
      klass = full_name;
      klass_len = i;
      name = full_name + i + 2;
      name_len = full_name_len - i - 2;
    }
  }

  /* Create the wraprecord */
  wraprec = nr_php_user_wraprec_create ();
  wraprec->funcname = nr_strndup (name, name_len);
  wraprec->funcnamelen = name_len;
  wraprec->funcnameLC = nr_string_to_lowercase (wraprec->funcname);

  if (klass) {
    wraprec->classname = nr_strndup (klass, klass_len);
    wraprec->classnamelen = klass_len;
    wraprec->classnameLC = nr_string_to_lowercase (wraprec->classname);
    wraprec->is_method = 1;
  }

  wraprec->supportability_metric =
    nr_txn_create_fn_supportability_metric (wraprec->funcname, wraprec->classname);

  return wraprec;
}

static void
nr_php_user_wraprec_destroy (nruserfn_t **wraprec_ptr)
{
  nruserfn_t *wraprec;

  if (0 == wraprec_ptr) {
    return;
  }
  wraprec = *wraprec_ptr;
  if (0 == wraprec) {
    return;
  }

  nr_free (wraprec->supportability_metric);
  nr_free (wraprec->drupal_module);
  nr_free (wraprec->drupal_hook);
  nr_free (wraprec->classname);
  nr_free (wraprec->funcname);
  nr_free (wraprec->classnameLC);
  nr_free (wraprec->funcnameLC);
  nr_realfree ((void **)wraprec_ptr);
}

static int
nr_php_user_wraprec_is_match (const nruserfn_t *w1, const nruserfn_t *w2)
{
  if ((0 == w1) && (0 == w2)) {
    return 1;
  }
  if ((0 == w1) || (0 == w2)) {
    return 0;
  }
  if (0 != nr_strcmp (w1->funcnameLC, w2->funcnameLC)) {
    return 0;
  }
  if (0 != nr_strcmp (w1->classnameLC, w2->classnameLC)) {
    return 0;
  }
  return 1;
}

static void
nr_php_add_custom_tracer_common (nruserfn_t *wraprec)
{
  /* Add the wraprecord to the list. */
  wraprec->next = nr_wrapped_user_functions;
  nr_wrapped_user_functions = wraprec;
}

#define NR_PHP_UNKNOWN_FUNCTION_NAME "{unknown}"
nruserfn_t *
nr_php_add_custom_tracer_callable (zend_function *func TSRMLS_DC)
{
  char *name = NULL;
  nruserfn_t *wraprec;

  if ((NULL == func) || (ZEND_USER_FUNCTION != func->type)) {
    return NULL;
  }

  /*
   * For logging purposes, let's create a name if we're logging at verbosedebug.
   */
  if (nrl_should_print (NRL_VERBOSEDEBUG, NRL_INSTRUMENT)) {
    name = nr_php_function_debug_name (func);
  }

  /*
   * We must check NR_PHP_ACC_INSTRUMENTED before attempting to dereference the
   * wraprec due to badly behaved Zend extensions potentially overwriting our
   * reserved pointer in the op array. (Cough, cough, ionCube. Piece of shit.)
   *
   * TODO(aharvey): figure out a new approach for PHP 7, since we can no longer
   * rely on a function flag.
   */
#ifndef PHP7
  if (NR_PHP_ACC_INSTRUMENTED & func->common.fn_flags) {
#endif /* !PHP7 */
    wraprec = nr_php_op_array_get_wraprec (&func->op_array);
    if (wraprec) {
      nrl_verbosedebug (NRL_INSTRUMENT,
                        "reusing custom wrapper for callable '%s'",
                        name);
      nr_free (name);
      return wraprec;
    }
#ifndef PHP7
  }
#endif /* !PHP7 */

  wraprec = nr_php_user_wraprec_create ();
  wraprec->is_transient = 1;

  nrl_verbosedebug (NRL_INSTRUMENT, "adding custom for callable '%s'", name);
  nr_free (name);

  nr_php_wrap_zend_function (func, wraprec TSRMLS_CC);
  nr_php_add_custom_tracer_common (wraprec);

  return wraprec;
}

nruserfn_t *
nr_php_add_custom_tracer_named (const char *namestr, size_t namestrlen TSRMLS_DC)
{
  nruserfn_t *wraprec;
  nruserfn_t *p;

  wraprec = nr_php_user_wraprec_create_named (namestr, namestrlen);
  if (0 == wraprec) {
    return 0;
  }

  /* Make sure that we are not duplicating an existing wraprecord */
  p = nr_wrapped_user_functions;

  while (0 != p) {
    if (nr_php_user_wraprec_is_match (p, wraprec)) {

      nrl_verbosedebug (NRL_INSTRUMENT, "reusing custom wrapper for '" NRP_FMT_UQ "%.5s" NRP_FMT_UQ "'",
        NRP_PHP (wraprec->classname),
        (0 == wraprec->classname) ? "" : "::",
        NRP_PHP (wraprec->funcname));

      nr_php_user_wraprec_destroy (&wraprec);
      nr_php_wrap_user_function_internal (p TSRMLS_CC);
      return p;         /* return the wraprec we are duplicating */
    }
    p = p->next;
  }

  nrl_verbosedebug (NRL_INSTRUMENT, "adding custom for '" NRP_FMT_UQ "%.5s" NRP_FMT_UQ "'",
    NRP_PHP (wraprec->classname),
    (0 == wraprec->classname) ? "" : "::",
    NRP_PHP (wraprec->funcname));

  nr_php_wrap_user_function_internal (wraprec TSRMLS_CC);
  nr_php_add_custom_tracer_common (wraprec);

  return wraprec;       /* return the new wraprec */
}

/*
 * Reset the user instrumentation records because we're starting a new
 * transaction and so we'll be loading all new user code.
 */
void
nr_php_reset_user_instrumentation (void)
{
  nruserfn_t *p = nr_wrapped_user_functions;

  while (0 != p) {
    p->is_wrapped = 0;
    p = p->next;
  }
}

/*
 * Remove any transient wraprecs. This must only be called on request shutdown!
 */
void
nr_php_remove_transient_user_instrumentation (void)
{
  nruserfn_t *p = nr_wrapped_user_functions;
  nruserfn_t *prev = NULL;

  while (NULL != p) {
    if (p->is_transient) {
      nruserfn_t *trans = p;

      if (prev) {
        prev->next = p->next;
      } else {
        nr_wrapped_user_functions = p->next;
      }

      p = p->next;
      nr_php_user_wraprec_destroy (&trans);
    } else {
      prev = p;
      p = p->next;
    }
  }
}

/*
 * Wrap all the interesting user functions with instrumentation.
 */
void
nr_php_add_user_instrumentation (TSRMLS_D)
{
  nruserfn_t *p = nr_wrapped_user_functions;

  while (0 != p) {
    if ((0 == p->is_wrapped) && (0 == p->is_disabled)) {
      nr_php_wrap_user_function_internal (p TSRMLS_CC);
    }
    p = p->next;
  }
}

void
nr_php_add_transaction_naming_function (const char *namestr, int namestrlen TSRMLS_DC)
{
  nruserfn_t *wraprec = nr_php_add_custom_tracer_named (namestr, namestrlen TSRMLS_CC);

  if (0 != wraprec) {
    wraprec->is_names_wt_simple = 1;
  }
}

void
nr_php_add_custom_tracer (const char *namestr, int namestrlen TSRMLS_DC)
{
  nruserfn_t *wraprec = nr_php_add_custom_tracer_named (namestr, namestrlen TSRMLS_CC);

  if (0 != wraprec) {
    wraprec->create_metric = 1;
    wraprec->is_user_added = 1;
  }
}

void
nr_php_add_exception_function (zend_function *func TSRMLS_DC)
{
  nruserfn_t *wraprec = nr_php_add_custom_tracer_callable (func TSRMLS_CC);

  if (wraprec) {
    wraprec->is_exception_handler = 1;
  }
}

void
nr_php_remove_exception_function (zend_function *func)
{
  nruserfn_t *wraprec;

  if ((NULL == func) ||
      (ZEND_USER_FUNCTION != func->type) ||
      (0 == nr_php_user_function_is_instrumented (func))) {
    return;
  }

  wraprec = nr_php_op_array_get_wraprec (&func->op_array);
  if (wraprec) {
    wraprec->is_exception_handler = 0;
  }
}

void
nr_php_destroy_user_wrap_records (void)
{
  nruserfn_t *next_user_wraprec;

  next_user_wraprec = nr_wrapped_user_functions;
  while (next_user_wraprec) {
    nruserfn_t *wraprec = next_user_wraprec;

    next_user_wraprec = wraprec->next;
    nr_php_user_wraprec_destroy (&wraprec);
  }

  nr_wrapped_user_functions = NULL;
}

int
nr_php_user_function_is_instrumented (const zend_function *function)
{
#ifdef PHP7
  int offset = NR_PHP_PROCESS_GLOBALS (zend_offset);

  return (NULL != function->op_array.reserved[offset]);
#else
  return (function->common.fn_flags & NR_PHP_ACC_INSTRUMENTED);
#endif /* PHP7 */
}

/*
 * This is a similar list, but for the dynamically added user-defined functions
 * rather than the statically defined internal/binary functions above.
 */
nruserfn_t *nr_wrapped_user_functions = 0;

void
nr_php_user_function_add_declared_callback (const char *namestr, int namestrlen,
                                            nruserfn_declared_t callback TSRMLS_DC)
{
  nruserfn_t *wraprec = nr_php_add_custom_tracer_named (namestr, namestrlen TSRMLS_CC);

  if (0 != wraprec) {
    wraprec->declared_callback = callback;

    /*
     * Immediately fire the callback if the function is already wrapped.
     */
    if (wraprec->is_wrapped && callback) {
      (callback) (TSRMLS_C);
    }
  }
}
