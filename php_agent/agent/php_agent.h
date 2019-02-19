/*
 * This file is the top level include file for PHP agent code.
 *
 * This includes the agent-agnostic headers, as well as a bunch of the more
 * commonly included PHP files. This should be included by all files that are
 * part of the PHP agent. It also defines all global variables and PHP specific
 * data types. It should be the very first thing included in all files that are
 * PHP specific.
 */

#include <stdbool.h>

#ifndef PHP_AGENT_HDR
#define PHP_AGENT_HDR

#ifndef NR_AGENT_PHP
#define NR_AGENT_PHP
#endif

/*
 * config.h is generated by standard the PHP build pipeline: phpize and
 * configure. This must be included first because some of the defines toggle
 * platform- specific code in axiom.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "nr_axiom.h"
#include "nr_app.h"
#include "nr_utilization.h"
#include "php_includes.h"
#include "php_compat.h"
#include "php_newrelic.h"
#include "php_zval.h"
#include "util_memory.h"
#include "util_strings.h"

#define NR_PHP_INI_DEFAULT_PORT "/tmp/.newrelic.sock"

/*
 * Configuration section. Set various compile-time defaults and maximums.
 */

#define NR_PHP_APP_NAME_DEFAULT "PHP Application"

/*
 * As of PHP {5.4, 5.5} the maximum flag bit is (from Zend/zend_compile.h)
 *      ZEND_ACC_DONE_PASS_TWO    0x08000000
 * As of  PHP {5.6} the maximum flag bit is:
 *      ZEND_ACC_HAS_TYPE_HINTS   0x10000000
 * Our flag bit starts at:
 *      NR_PHP_ACC_INSTRUMENTED   0x40000000
 * so we are safe.
 *
 * The reason we set this flag is because in PHP 5.2.5 and
 * lower, the reserved resources are not guaranteed to be initialized to
 * NULL. The only way to initialize them to null is to be a zend_extension
 * and define an op_array_ctor_func_t. We don't want to be a zend_extension
 * yet, so we use this flag instead.
 *
 * We only set this flag if we truly instrumented the function by putting
 * a pointer to a wraprec in the reserved resource offset.
 *
 * In PHP 7, we rely entirely on the reserved pointer, as there are no
 * remaining bits in the function flags that are unused.
 *
 * TODO(aharvey): consider if we can remove this altogether, now that PHP 5.2
 * support has been dropped; the key will be whether ionCube has updated its
 * extensions to prevent the crashes we saw without this flag.
 */
#ifndef PHP7
#define NR_PHP_ACC_INSTRUMENTED 0x40000000
#endif /* !PHP7 */

/*
 * Purpose : Checks if the given feature flag is enabled.
 *
 * Params  : 1. The feature flag to check.
 *
 * Returns : Non-zero if the flag is enabled, or zero if it's disabled.
 */
#define NR_PHP_FEATURE(flag) (0 != NR_PHP_PROCESS_GLOBALS(feature_flags).flag)

/*
 * Purpose : Produce a JSON string, which is an array of single stack frame
 *           entries.
 *
 * Params  : 1. An optional zval of the point from which to do the trace.
 *              If this is NULL, nr_php_backtrace is used to capture the
 *              current VM position.
 *
 * Returns : A newly allocated JSON stack trace string or NULL on error.
 */
extern char* nr_php_backtrace_to_json(zval* itrace TSRMLS_DC);

/*
 * Purpose : Create a dump of the current VM position in a zval array in the
 *           format provided by PHP.  This is a wrapper over
 *           zend_fetch_debug_backtrace to abstact PHP version differences.
 *           Not all stack frames are included: in PHP 5.4 and above, the
 *           number is limited to NR_PHP_BACKTRACE_LIMIT.
 *
 * Returns : An array zval containing the stack trace, which must be freed with
 *           nr_php_zval_free().
 */
#define NR_PHP_BACKTRACE_LIMIT 20
extern zval* nr_php_backtrace(TSRMLS_D);

/*
 * Purpose : Write a dump of the current VM position to a file descriptor.
 *
 * Params  : 1. The destination file descriptor.
 *           2. The maximum number of execution frames to write.
 */
extern void nr_php_backtrace_fd(int fd, int limit TSRMLS_DC);

/*
 * Purpose : Same as nr_php_backtrace_to_json but designed as a callback
 * parameter for nr_segment_datastore_end().
 */
extern char* nr_php_backtrace_callback(void);

/*
 * Purpose : Register / unregister all INI variables.
 *
 * Params  : 1. The PHP module number passed to MINIT/MSHUTDOWN.
 *           2. TSRMLS_CC
 *
 * Returns : Nothing.
 */
extern void nr_php_register_ini_entries(int module_number TSRMLS_DC);
extern void nr_php_unregister_ini_entries(int module_number TSRMLS_DC);

/*
 * Some agent functions require an object handle. At present, this is defined
 * as unsigned int on all versions of PHP, but that isn't a requirement of the
 * language specification.
 */
typedef unsigned int nr_php_object_handle_t;

/*
 * Purpose : Extract the named entity from a PHP object zval.
 *
 * Params  : 1. The object to extract the property from.
 *           2. The name of the object.
 *
 * Returns : The specified element or NULL if it was not found.
 */
extern zval* nr_php_get_zval_object_property(zval* object,
                                             const char* cname TSRMLS_DC);

/*
 * Purpose : Extract the named property from a PHP object zval in a particular
 *           class context. (Useful for getting private properties from
 *           ancestor classes.)
 *
 * Params  : 1. The object to extract the property from.
 *           2. The class entry.
 *           3. The name of the property.
 *
 * Returns : The specified element or NULL if it was not found.
 */
extern zval* nr_php_get_zval_object_property_with_class(zval* object,
                                                        zend_class_entry* ce,
                                                        const char* cname
                                                            TSRMLS_DC);

/*
 * Purpose : Determine if an object has the specified method or not.
 *
 * Params  : 1. The object to check.
 *           2. The name of the method to check (must be all lower case).
 *
 * Returns : 0 or 1
 *
 * Notes   : The method name must be lower case because the Zend Engine
 *           uses the lower case form as the lookup key in the class entry's
 *           method table.
 *
 * Warning : This function will return 1 if object implements the __call
 *           magic method. Invoking the method could therefore still result
 *           in a BadMethodCallException.
 */
extern int nr_php_object_has_method(zval* object, const char* lcname TSRMLS_DC);

/*
 * Purpose : Determine whether an object has a concrete implementation of
 *           the specified method or not.
 *
 * Params  : 1. The object to check (must be a user class).
 *           2. The name of the method to check (must be lower case).
 *
 * Returns : Non-zero if object has a concrete implementation of the
 *           specified method; otherwise, zero.
 *
 * Notes   : The method name must be lower case because the Zend Engine
 *           uses the lower case form as the lookup key in the class entry's
 *           method table.
 */
extern int nr_php_object_has_concrete_method(zval* object,
                                             const char* lcname TSRMLS_DC);

/*
 * Purpose : Do all the substantive work to start a new transaction. This will
 *           be called by both the start transaction API and the normal RINIT
 *           handler.
 *
 * Params  : 1. The application name to use or NULL for default.
 *           2. The license to use or NULL for default.
 *
 * Returns : NR_SUCCESS if a transaction was started, NR_FAILURE otherwise.
 */
extern nr_status_t nr_php_txn_begin(const char* appnames,
                                    const char* license TSRMLS_DC);

/*
 * Purpose : Perform the transaction ending tasks that have to be performed at
 *           RSHUTDOWN: specifically, this includes setting parameters that
 *           depends on variables that will be shutdown before the
 *           post-deactivate hook is fired.
 */
extern void nr_php_txn_shutdown(TSRMLS_D);

/*
 * Purpose : Do all the substantive work to end a transaction. This will be
 *           called by both the end transaction API and the post-deactivate
 *           handler. If nr_php_txn_shutdown hasn't already been called, it
 *           will be called as part of this function.
 *
 * Params  : 1. Whether or not to ignore the current transaction.
 *           2. Whether the function is being called from a post-deactivate
 *              handler.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.  (Currently always NR_SUCCESS)
 */
extern nr_status_t nr_php_txn_end(int ignoretxn,
                                  int in_post_deactivate TSRMLS_DC);

/*
 * Purpose : Check if the given extension is loaded.
 *
 * Params  : 1. The extension name.
 *
 * Returns : Non-zero if the extension is loaded; zero otherwise.
 */
extern int nr_php_extension_loaded(const char* name);

/*
 * Purpose : Create a friendly name from a callable variable.
 *
 * Params  : 1. The callable.
 *
 * Returns : A string containing the name, or NULL if the callable is
 *           malformed. The caller must nr_free the string after use.
 */
extern char* nr_php_callable_to_string(zval* callable TSRMLS_DC);

/*
 * Purpose : Lookup a function in the global function table. In PHP, this is a
 *           wrapper around zend_hash_find (EG (function_table), ...) that
 *           provides an additional layer of type safety and prevents warnings
 *           due to differences in const-correctness across PHP versions.
 *
 * Params  : 1. The name of the function to locate. This must be lowercase, as
 *              PHP converts all class names to lowercase as part of
 *              implementing case insensitivity for class names.
 *
 * Returns : A pointer to the function, if found; otherwise, NULL.
 */
extern zend_function* nr_php_find_function(const char* name TSRMLS_DC);

/*
 * Purpose : Lookup a class object in the global class table. In PHP, this is a
 *           wrapper around zend_hash_find (EG (class_table), ...) that provides
 *           an additional layer of type safety and prevents warnings due to
 *           differences in const-correctness across PHP versions.
 *
 * Params  : 1. The name of the class to locate. This must be lowercase, as PHP
 *              converts all class names to lowercase as part of implementing
 *              case insensitivity for class names.
 *
 * Returns : A pointer to the class entry, if found; otherwise, NULL.
 */
extern zend_class_entry* nr_php_find_class(const char* name TSRMLS_DC);

/*
 * Purpose : Lookup a method in a class. This is a wrapper around
 *           zend_hash_find ("class function table", ...) that provides an
 *           additional layer of type safety and prevents warnings due to
 *           differences in const-correctness across PHP versions.
 *
 * Params  : 1. A pointer to an object class.
 *           2. The name of the method to locate in "klass". As in the functions
 *              above, this must be lowercase.
 *
 * Returns : A pointer to the desired class method, if found; otherwise, NULL.
 */
extern zend_function* nr_php_find_class_method(const zend_class_entry* klass,
                                               const char* name);

/*
 * Purpose : Check if the given class entry is an instanceof a particular class.
 *
 * Params  : 1. A pointer to a class entry.
 *           2. The name of the class to verify the object is an instance of.
 *
 * Returns : 0 or 1.
 */
extern int nr_php_class_entry_instanceof_class(const zend_class_entry* ce,
                                               const char* class_name
                                                   TSRMLS_DC);

/*
 * Purpose : Check if the given object is an instanceof a particular class.
 *
 * Params  : 1. A pointer to an object.
 *           2. The name of the class to verify the object is an instance of.
 *
 * Returns : 0 or 1.
 */
extern int nr_php_object_instanceof_class(const zval* object,
                                          const char* class_name TSRMLS_DC);

/*
 * Purpose : Wrap zend_is_callable_ex and get the zend_function for a callable.
 *
 * Params  : 1. The callable zval to check.
 *
 * Returns : A pointer to the zend_function, or NULL if the zval isn't
 *           callable.
 */
extern zend_function* nr_php_zval_to_function(zval* zv TSRMLS_DC);

/*
 * Purpose : Get the return value for the current function.
 *
 * Params  : 1. The execute data or op array for the current function.
 *
 * Returns : A pointer to a zval.
 *
 * Note    : If you need to replace the return value pointer, this function
 *           won't help you!
 */
static inline zval* nr_php_get_return_value(NR_EXECUTE_PROTO TSRMLS_DC) {
#ifdef PHP7
  if (NULL == execute_data) {
    /*
     * This function shouldn't be called from outside a function context, so
     * this should never actually happen in practice.
     */
    return NULL;
  }

  return execute_data->return_value;
#else
  zval** return_value_ptr_ptr = EG(return_value_ptr_ptr);

  NR_UNUSED_SPECIALFN;

  return return_value_ptr_ptr ? *return_value_ptr_ptr : NULL;
#endif /* PHP7 */
}

/*
 * Purpose : Get a parameter to a user function
 *
 * Params  : 1. The index of the requested parameter. WATCH OUT: STARTS AT ONE.
 *
 * Returns : The zval parameter, or 0 if the index was out of bounds, or some
 *           other "should not happen" situation. Note that this may be an
 *           IS_REFERENCE zval in PHP 7 if the function expects a reference.
 *
 * Warning : Directly modifying the returned zval in PHP 7 may result in weird,
 *           hard to debug issues.
 *
 * IMPORTANT : This should only be called before the call to the current user
 *             function being executed.  Most of the time you should be using
 *             nr_php_arg_get.  See php_wrapper.h.
 */
extern zval* nr_php_get_user_func_arg(size_t requested_arg_index,
                                      NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose : Return the number of arguments given to the current user function.
 *
 * Returns : The number of arguments.
 */
extern size_t nr_php_get_user_func_arg_count(NR_EXECUTE_PROTO TSRMLS_DC);

static inline zend_function* nr_php_execute_function(
    NR_EXECUTE_PROTO TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
  if (NULL == execute_data) {
    return NULL;
  }
#else
  if (NULL == op_array_arg) {
    return NULL;
  }
#endif

#ifdef PHP7
  return execute_data->func;
#elif ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
  return execute_data->function_state.function;
#else
  return op_array_arg->prototype ? op_array_arg->prototype
                                 : (zend_function*)op_array_arg;
#endif /* PHP7 */
}

static inline zval* nr_php_execute_scope(zend_execute_data* execute_data) {
  if (NULL == execute_data) {
    return NULL;
  }

#ifdef PHP7
  return &execute_data->This;
#else
  return execute_data->object;
#endif
}

static inline zend_op_array* nr_php_current_op_array(TSRMLS_D) {
#ifdef PHP7
  zend_function* func = EG(current_execute_data)->func;

  return func ? &func->op_array : NULL;
#else
  return EG(current_execute_data)->op_array;
#endif /* PHP7 */
}

/*
 * Purpose : Get the execute data for the function that called the current user
 *           function.
 *
 * Params  : The number of frames to advance up (1 returns the actual caller).
 *
 * Returns : A pointer to the execute data.
 */
extern zend_execute_data* nr_php_get_caller_execute_data(NR_EXECUTE_PROTO,
                                                         ssize_t offset
                                                             TSRMLS_DC);

/*
 * Purpose : Get the function which called the current user function.
 *
 * Params  : The number of frames to advance up (1 returns the actual caller).
 */
extern const zend_function* nr_php_get_caller(NR_EXECUTE_PROTO,
                                              ssize_t offset TSRMLS_DC);

/*
 * Purpose : Return a PHP variable from the active variable scope.
 *
 * Params  : 1. The name of the variable.
 *
 * Returns : The variable, or NULL if the variable can't be found.
 */
extern zval* nr_php_get_active_php_variable(const char* name TSRMLS_DC);

/*
 * Purpose : Silence errors, but save the previous state.
 *
 * Returns : The previous error reporting setting.
 */
extern int nr_php_silence_errors(TSRMLS_D);

/*
 * Purpose : Restore the previous error reporting setting returned by
 *           nr_php_silence_errors.
 *
 * Params  : 1. The previous error reporting setting.
 */
extern void nr_php_restore_errors(int error_reporting TSRMLS_DC);

/*
 * Purpose : Acquire a named constant from PHP.
 *
 * Returns : A zval containing the constant value or NULL if the constant is
 *           not found.  The caller must call nr_php_zval_free() on the zval
 *           after use.
 */
extern zval* nr_php_get_constant(const char* name TSRMLS_DC);

/*
 * Purpose : Acquire a named class constant from PHP.
 *
 * Returns : A zval containing the constant value or NULL if the constant is
 *           not found. The caller must call nr_php_zval_free() on the zval
 *           after use.
 */
extern zval* nr_php_get_class_constant(const zend_class_entry* ce,
                                       const char* name);

/*
 * Purpose : Determine if the given zval has the same value as the PHP constant
 *           of the given name.
 *
 * Returns : 1 if there is equality and 0 otherwise.
 */
extern int nr_php_is_zval_named_constant(const zval* zv,
                                         const char* name TSRMLS_DC);

/*
 * Purpose : Wrapper for zend_is_auto_global.
 *
 * Params  : 1. The name of the superglobal.
 *           2. The length of the superglobal name.
 *
 * Returns : Non-zero if the superglobal exists; zero otherwise.
 */
extern int nr_php_zend_is_auto_global(const char* name,
                                      size_t name_len TSRMLS_DC);

/*
 * Purpose : Determine the license key that should be used.  The license key
 *           precedence order is (from highest priority to least priority):
 *
 *           * API license key provided as parameter.
 *           * NRINI license key provided as newrelic.license in INI file.
 *           * Upgrade from 2.9 license key in upgrade_please.key file.
 *
 *           If one of these is null or empty, the next is checked.  The
 *           license key is only returned if it has the proper length.
 *
 * Params  : 1. Any license key that has been provided by an API call.
 *
 * Returns : A pointer to the license key to use, or 0 if no proper license key
 *           was found.
 */
extern const char* nr_php_use_license(const char* api_license TSRMLS_DC);

/*
 * Purpose : Get a $_SERVER global value. See:
 *           http://www.php.net/manual/en/reserved.variables.server.php
 *
 * Notes   : This function requires that the _SERVER automatic
 *           globals be triggered.  This is done with the call
 *           "zend_is_auto_global (NR_PSTR ("_SERVER") TSRMLS_CC);"
 *           in RINIT.
 */
extern char* nr_php_get_server_global(const char* name TSRMLS_DC);

/*
 * Purpose : Determine whether the given ini setting has been set by the
 *           user, or if the default value is being used instead.
 *
 * Returns : 1 if the setting has been explicitly set by the user
 *           and 0 otherwise (indicating that the default is
 *           being used).  Returns 0 if 'name' is a null pointer.
 */
extern int nr_php_ini_setting_is_set_by_user(const char* name);

/*
 * Purpose : Removes an interface from the given class entry.
 *
 * Params  : 1. The class entry.
 *           2. The interface's class entry.
 */
extern void nr_php_remove_interface_from_class(zend_class_entry* class_ce,
                                               const zend_class_entry* iface_ce
                                                   TSRMLS_DC);

/*
 * Purpose : Swap the implementations of two user functions.
 *
 * Params  : 1. The first function.
 *           2. The second function.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_php_swap_user_functions(zend_function* a,
                                              zend_function* b);

/*
 * Purpose : Extract the class name from a "class::method" string.  The returned
 *           string is newly allocated and must be freed by the caller.
 * Returns : A string owned by the caller, to be freed by the caller.
 *
 * nr_php_class_name_from_full_name ("alpha")       => NULL
 * nr_php_class_name_from_full_name ("alpha::beta") => "alpha"
 */
extern char* nr_php_class_name_from_full_name(const char* full_name);

/*
 * Purpose : Extract the function name from a "function" or "class::method"
 *           string.  The returned string is newly allocated and must be
 *           freed by the caller.
 * Returns : A string owned by the caller, to be freed by the caller.
 *
 * nr_php_function_name_from_full_name ("alpha")       => "alpha"
 * nr_php_function_name_from_full_name ("alpha::beta") => "beta"
 */
extern char* nr_php_function_name_from_full_name(const char* full_name);

/*
 * Purpose : Create and return a unique name describing the given zend_function,
 *           including doing sensible things for closures.
 *
 * Params  : 1. The function to name.
 *
 * Returns : An allocated string, ownership of which passes to the caller, or
 *           NULL if the function is invalid.
 *
 * Note    : This function is intended for internal logging purposes only, and
 *           does not generate names that are usable in metrics or traces.
 */
extern char* nr_php_function_debug_name(const zend_function* func);

/*
 * Purpose : Extract the filename from a zend_function.
 *
 * Returns : A pointer to the filename or NULL if the callable was not a user
 *           function or if no filename was found.
 */
extern const char* nr_php_function_filename(zend_function* func);

static inline zend_class_entry* nr_php_zend_register_internal_class_ex(
    zend_class_entry* ce,
    zend_class_entry* parent_ce TSRMLS_DC) {
#ifdef PHP7
  return zend_register_internal_class_ex(ce, parent_ce);
#else
  return zend_register_internal_class_ex(ce, parent_ce, NULL TSRMLS_CC);
#endif /* PHP7 */
}

static inline char* nr_php_zend_ini_string(char* name,
                                           nr_string_len_t name_len,
                                           int orig) {
#ifdef PHP7
  return zend_ini_string(name, name_len, orig);
#else
  return zend_ini_string(name, name_len + 1, orig);
#endif /* PHP7 */
}

static inline const char* NRPURE
nr_php_class_entry_name(const zend_class_entry* ce) {
#ifdef PHP7
  return (ce->name && ce->name->len) ? ce->name->val : NULL;
#else
  return ce->name;
#endif
}

static inline nr_string_len_t NRPURE
nr_php_class_entry_name_length(const zend_class_entry* ce) {
#ifdef PHP7
  return ce->name ? ce->name->len : 0;
#else
  return NRSAFELEN(ce->name_length);
#endif
}

static inline const char* NRPURE
nr_php_function_name(const zend_function* func) {
#ifdef PHP7
  return (func->common.function_name && func->common.function_name->len)
             ? func->common.function_name->val
             : NULL;
#else
  return func->common.function_name;
#endif
}

static inline nr_string_len_t NRPURE
nr_php_function_name_length(const zend_function* func) {
#ifdef PHP7
  return func->common.function_name ? func->common.function_name->len : 0;
#else
  /*
   * No NRSAFELEN macro here as nr_strlen can't return a negative value anyway
   * (it simply casts the size_t returned by strlen() to an int.
   */
  return nr_strlen(func->common.function_name);
#endif
}

static inline const char* NRPURE
nr_php_op_array_file_name(const zend_op_array* op_array) {
#ifdef PHP7
  return (op_array->filename && op_array->filename->len)
             ? op_array->filename->val
             : NULL;
#else
  return op_array->filename;
#endif
}

static inline const char* NRPURE
nr_php_op_array_function_name(const zend_op_array* op_array) {
#ifdef PHP7
  return (op_array->function_name && op_array->function_name->len)
             ? op_array->function_name->val
             : NULL;
#else
  return op_array->function_name;
#endif
}

static inline const char* NRPURE
nr_php_op_array_scope_name(const zend_op_array* op_array) {
#ifdef PHP7
  if (op_array->scope && op_array->scope->name && op_array->scope->name->len) {
    return op_array->scope->name->val;
  }
#else
  if (op_array->scope) {
    return op_array->scope->name;
  }
#endif

  return NULL;
}

static inline const char* NRPURE
nr_php_ini_entry_name(const zend_ini_entry* entry) {
#ifdef PHP7
  return (entry->name && entry->name->len) ? entry->name->val : NULL;
#else
  return entry->name;
#endif
}
static inline nr_string_len_t NRPURE
nr_php_ini_entry_name_length(const zend_ini_entry* entry) {
#ifdef PHP7
  return entry->name ? entry->name->len : 0;
#else
  return NRSAFELEN(entry->name_length - 1);
#endif
}

#ifdef PHP7
#define NR_PHP_INTERNAL_FN_THIS getThis()
#define NR_PHP_USER_FN_THIS getThis()
#else
#define NR_PHP_INTERNAL_FN_THIS getThis()
#define NR_PHP_USER_FN_THIS EG(This)
#endif

/*
 * Purpose : Wrap the native PHP json_decode function for those times when we
 *           need a more robust JSON decoder than nro_create_from_json.
 *
 * Params  : 1. A string zval containing the JSON to be decoded.
 *
 * Returns : A zval owned by the caller containing the decoded JSON value, or
 *           NULL if an error occurred.
 */
extern zval* nr_php_json_decode(zval* json TSRMLS_DC);

/*
 * Purpose : Wrap the native PHP json_encode function for those times when we
 *           need a more robust JSON encoder than nro_to_json.
 *
 * Params  : 1. A mixed zval containing the structure to be encoded..
 *
 * Returns : A zval owned by the caller containing the encoded JSON string, or
 *           NULL if an error occurred.
 */
extern zval* nr_php_json_encode(zval* zv TSRMLS_DC);

/*
 * Purpose : Wrap PHP's parse_str() function.
 *
 * Params  : 1. The query string to parse.
 *           2. The length of the string.
 *
 * Returns : An array zval owned by the caller containing the parsed components
 *           of the given query string.
 */
extern zval* nr_php_parse_str(const char* str, size_t len TSRMLS_DC);

/*
 * Purpose : A helper function to identify
 *           thrown by the called user function and returns it via an out
 *           parameter, then clears the exception from PHP.
 *
 * Params  : 1. The zend_function, ideally retrieved via nr_php_execute_function
 *
 * Returns : Boolean int: 1 == static method, 0 == instance method
 */
extern bool nr_php_function_is_static_method(const zend_function* func);

#endif /* PHP_AGENT_HDR */
