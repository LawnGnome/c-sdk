/*
 * This file contains functions common to both Drupal frameworks.
 *
 * We support both Drupal 6/7 (FW_DRUPAL) and Drupal 8 (FW_DRUPAL8) within the
 * agent. These framework versions are significantly different internally and
 * have hence been implemented as separate frameworks, but share some code.
 */
#ifndef FW_DRUPAL_COMMON_HDR
#define FW_DRUPAL_COMMON_HDR

#include "php_user_instrument.h"

#define NR_DRUPAL_MODULE_PREFIX "Framework/Drupal/Module/"
#define NR_DRUPAL_HOOK_PREFIX "Framework/Drupal/Hook/"
#define NR_DRUPAL_VIEW_PREFIX "Framework/Drupal/ViewExecute/"

/*
 * Purpose : Create a Drupal metric.
 *
 * Params  : 1. The transaction to create the metric on.
 *           2. The prefix to use when creating the metric.
 *           3. The length of the prefix.
 *           4. The suffix to use when creating the metric.
 *           5. The length of the suffix.
 *           6. The total duration of the metric.
 *           7. The exclusive duration of the metric.
 */
extern void nr_drupal_create_metric(nrtxn_t* txn,
                                    const char* prefix,
                                    int prefix_len,
                                    const char* suffix,
                                    int suffix_len,
                                    nrtime_t duration,
                                    nrtime_t exclusive);

/*
 * Purpose : Call the original Drupal view execute function and create the
 *           appropriate view metric.
 *
 * Params  : 1. The view name.
 *           2. The length of the view name.
 *           3. The original execute data.
 *
 * Returns : Non-zero if zend_bailout needs to be called.
 */
extern int nr_drupal_do_view_execute(const char* name,
                                     int name_len,
                                     NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose : Determine whether the given framework is a Drupal framework.
 *
 * Params  : 1. The framework to test.
 *
 * Returns : Non-zero if the framework is Drupal.
 */
extern int nr_drupal_is_framework(nrframework_t fw);

/*
 * Purpose : Wrap a user function with Drupal module and hook metadata.
 *
 * Params  : 1. The function name.
 *           2. The length of the function name.
 *           3. The module name.
 *           4. The length of the module name.
 *           5. The hook name.
 *           6. The length of the hook name.
 *
 * Returns : The user function wrapper, or NULL on failure.
 */
extern nruserfn_t* nr_php_wrap_user_function_drupal(const char* name,
                                                    int namelen,
                                                    const char* module,
                                                    nr_string_len_t module_len,
                                                    const char* hook,
                                                    nr_string_len_t hook_len
                                                        TSRMLS_DC);

/*
 * Purpose : Instrument the given module and hook.
 *
 * Params  : 1. The module name.
 *           2. The module name length.
 *           3. The hook name.
 *           4. The hook name length.
 */
extern void nr_drupal_hook_instrument(const char* module,
                                      size_t module_len,
                                      const char* hook,
                                      size_t hook_len TSRMLS_DC);

#endif /* FW_DRUPAL_COMMON_HDR */
