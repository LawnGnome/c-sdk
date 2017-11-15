#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "lib_guzzle_common.h"
#include "lib_guzzle4.h"
#include "lib_guzzle6.h"
#include "nr_header.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

char *
nr_guzzle_create_async_context_name (const char *prefix, zval *obj)
{
  char *name = NULL;
  char *ename = NULL;
  int ename_len = 0;

  if (!nr_php_is_zval_valid_object (obj)) {
    return NULL;
  }

  /* TODO(aharvey): replace with asprintf when we can use it. */
  ename_len = spprintf (&ename, 0, "%s #%d", prefix, Z_OBJ_HANDLE_P (obj));
  name = nr_strndup (ename, ename_len);
  efree (ename);

  return name;
}

static int
nr_guzzle_stack_iterator (zval *frame, int *in_guzzle_ptr,
                          zend_hash_key *key NRUNUSED TSRMLS_DC)
{
  int idx;
  zval *klass = NULL;

  NR_UNUSED_TSRMLS;

  if (0 == nr_php_is_zval_valid_array (frame)) {
    return ZEND_HASH_APPLY_KEEP;
  }
  if (NULL == in_guzzle_ptr) {
    return ZEND_HASH_APPLY_KEEP;
  }

  klass = nr_php_zend_hash_find (Z_ARRVAL_P (frame), "class");

  if (0 == nr_php_is_zval_non_empty_string (klass)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  idx = nr_strncaseidx (Z_STRVAL_P (klass), "guzzle", Z_STRLEN_P (klass));
  if (idx >= 0) {
    *in_guzzle_ptr = 1;
  }

  return ZEND_HASH_APPLY_KEEP;
}

int
nr_guzzle_in_call_stack (TSRMLS_D)
{
  int in_guzzle = 0;
  zval *stack;

  if (0 == NRINI (guzzle_enabled)) {
    return 0;
  }

  stack = nr_php_backtrace (TSRMLS_C);

  if (nr_php_is_zval_valid_array (stack)) {
    nr_php_zend_hash_zval_apply (Z_ARRVAL_P (stack),
                                 (nr_php_zval_apply_t) nr_guzzle_stack_iterator,
                                 &in_guzzle TSRMLS_CC);
  }

  nr_php_zval_free (&stack);

  return in_guzzle;
}

int
nr_guzzle_does_zval_implement_has_emitter (zval *obj TSRMLS_DC)
{
  return nr_php_object_instanceof_class (obj, "GuzzleHttp\\Event\\HasEmitterInterface" TSRMLS_CC);
}


static void
nr_guzzle_obj_destroy (nrtxntime_t *time)
{
  nr_free (time);
}

void
nr_guzzle_obj_add (const zval *obj TSRMLS_DC)
{
  nrtxntime_t *start = (nrtxntime_t *) nr_malloc (sizeof (nrtxntime_t));

  /*
   * Create the guzzle_objs hash table if we haven't already done so.
   */
  if (NULL == NRPRG (guzzle_objs)) {
    NRPRG (guzzle_objs) = nr_hashmap_create ((nr_hashmap_dtor_func_t) nr_guzzle_obj_destroy);
  }

  nr_txn_set_time (NRPRG (txn), start);

  /*
   * Ditto for the async context.
   */
  if (NULL == NRPRG (guzzle_ctx)) {
    NRPRG (guzzle_ctx) = nr_async_context_create (start->when);
  }

  /*
   * We store the start times indexed by the object handle for the Request
   * object: Zend object handles are unsigned ints while HashTable objects
   * support unsigned longs as indexes, so this is safe regardless of
   * architecture, and saves us having to transform the object handle into a
   * string to use string keys.
   */
  nr_hashmap_index_update (NRPRG (guzzle_objs), (uint64_t) Z_OBJ_HANDLE_P (obj),
                           start);
}

nr_status_t
nr_guzzle_obj_find_and_remove (const zval *obj, nrtxntime_t *start TSRMLS_DC)
{
  if ((NULL != NRPRG (guzzle_objs)) && (NULL != NRPRG (guzzle_ctx))) {
    uint64_t index = (uint64_t) Z_OBJ_HANDLE_P (obj);
    nrtxntime_t *saved;

    saved = (nrtxntime_t *) nr_hashmap_index_get (NRPRG (guzzle_objs), index);
    if (saved) {
      nrtime_t duration;
      nrtxntime_t stop = { .stamp = 0, .when = 0 };

      /*
       * Copy the start time, since we're about to delete the hashmap value.
       */
      nr_memcpy (start, saved, sizeof (nrtxntime_t));

      /*
       * Remove the object handle from the hashmap containing active requests.
       */
      nr_hashmap_index_delete (NRPRG (guzzle_objs), index);

      /*
       * Add the duration of the request to the amount of time we've spent
       * doing async work.
       */
      nr_txn_set_time (NRPRG (txn), &stop);
      duration = nr_time_duration (start->when, stop.when);
      nr_async_context_add (NRPRG (guzzle_ctx), duration);

      /*
       * If there are no more objects in the cache, then this was the last of a
       * set of requests, and we should close off the context and add the time
       * that was actually spent doing async work to the transaction's
       * async_duration.
       */
      if (0 == nr_hashmap_count (NRPRG (guzzle_objs))) {
        nrtime_t async_duration;

        nr_async_context_end (NRPRG (guzzle_ctx), stop.when);
        async_duration = nr_async_context_get_duration (NRPRG (guzzle_ctx));
        nr_txn_add_async_duration (NRPRG (txn), async_duration);

        nr_async_context_destroy (&NRPRG (guzzle_ctx));
      }

      return NR_SUCCESS;
    }
  }

  nrl_verbosedebug (NRL_INSTRUMENT,
                    "Guzzle: object %d not found in tracked list",
                    Z_OBJ_HANDLE_P (obj));
  return NR_FAILURE;
}

/*
 * Purpose : Sets a header on an object implementing either the Guzzle 3 or 4
 *           MessageInterface.
 *
 * Params  : 1. The header to set.
 *           2. The value to set the header to.
 *           3. The request object.
 */
static void
nr_guzzle_request_set_header (const char *header, const char *value,
                              zval *request TSRMLS_DC)
{
  zval *header_param = NULL;
  zval *retval = NULL;
  zval *value_param = NULL;

  if ((NULL == header) || (NULL == value) || (NULL == request)) {
    return;
  }

  header_param = nr_php_zval_alloc ();
  nr_php_zval_str (header_param, header);
  value_param = nr_php_zval_alloc ();
  nr_php_zval_str (value_param, value);

  retval = nr_php_call (request, "setHeader", header_param, value_param);

  nr_php_zval_free (&header_param);
  nr_php_zval_free (&retval);
  nr_php_zval_free (&value_param);
}

void
nr_guzzle_request_set_outbound_headers (zval *request TSRMLS_DC)
{
  char *x_newrelic_id = NULL;
  char *x_newrelic_transaction = NULL;
  char *x_newrelic_synthetics = NULL;

  nr_header_outbound_request (NRPRG (txn), &x_newrelic_id, &x_newrelic_transaction, &x_newrelic_synthetics);

  if (NRPRG (txn) && NRTXN (special_flags.debug_cat)) {
    nrl_verbosedebug (NRL_CAT,
      "CAT: outbound request: transport='Guzzle' %s=" NRP_FMT " %s=" NRP_FMT,
      X_NEWRELIC_ID,          NRP_CAT (x_newrelic_id),
      X_NEWRELIC_TRANSACTION, NRP_CAT (x_newrelic_transaction));
  }

  nr_guzzle_request_set_header (X_NEWRELIC_ID, x_newrelic_id, request TSRMLS_CC);
  nr_guzzle_request_set_header (X_NEWRELIC_TRANSACTION, x_newrelic_transaction,
                                request TSRMLS_CC);
  nr_guzzle_request_set_header (X_NEWRELIC_SYNTHETICS, x_newrelic_synthetics,
                                request TSRMLS_CC);

  nr_free (x_newrelic_id);
  nr_free (x_newrelic_transaction);
  nr_free (x_newrelic_synthetics);
}

char *
nr_guzzle_response_get_header (const char *header, zval *response TSRMLS_DC)
{
  zval *param = nr_php_zval_alloc ();
  zval *retval = NULL;
  char *value = NULL;

  nr_php_zval_str (param, header);

  retval = nr_php_call (response, "getHeader", param);
  if (NULL == retval) {
    nrl_verbosedebug (NRL_INSTRUMENT, "Guzzle: Response::getHeader() returned NULL");
  } else if (nr_php_is_zval_valid_string (retval)) {
    /*
     * Guzzle 4 and 5 return an empty string if the header could not be found.
     */
    if (Z_STRLEN_P (retval) > 0) {
      value = nr_strndup (Z_STRVAL_P (retval), Z_STRLEN_P (retval));
    }
  } else if (nr_php_object_instanceof_class (retval,
                                             "Guzzle\\Http\\Message\\Header" TSRMLS_CC)) {
    /*
     * Guzzle 3 returns an object that we can cast to a string, so let's do
     * that. We'll call __toString() directly rather than going through PHP's
     * convert_to_string() function, as that will generate a notice if the
     * cast fails for some reason.
     */
    zval *zv_str = nr_php_call (retval, "__toString");

    if (nr_php_is_zval_non_empty_string (zv_str)) {
      value = nr_strndup (Z_STRVAL_P (zv_str), Z_STRLEN_P (zv_str));
    } else if (NULL != zv_str) {
      nrl_verbosedebug (NRL_INSTRUMENT,
                        "Guzzle: Header::__toString() returned a non-string of type %d",
                        Z_TYPE_P (zv_str));
    } else {
      /*
       * We should never get NULL as the retval from nr_php_call, but just in
       * case...
       */
      nrl_verbosedebug (NRL_INSTRUMENT,
                        "Guzzle: Header::__toString() returned a NULL retval");
    }

    nr_php_zval_free (&zv_str);
  } else {
    nrl_verbosedebug (NRL_INSTRUMENT,
                      "Guzzle: unexpected Response::getHeader() return of type %d",
                      Z_TYPE_P (retval));
  }

  nr_php_zval_free (&param);
  nr_php_zval_free (&retval);

  return value;
}

NR_PHP_WRAPPER_START (nr_guzzle_client_construct)
{
  int is_guzzle_45 = 0;
  zval *this_var = nr_php_scope_get (NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  (void) wraprec;
  NR_UNUSED_SPECIALFN;

  is_guzzle_45 = nr_guzzle_does_zval_implement_has_emitter (this_var TSRMLS_CC);
  nr_php_scope_release (&this_var);

  if (is_guzzle_45) {
    NR_PHP_WRAPPER_DELEGATE (nr_guzzle4_client_construct);
  } else {
    NR_PHP_WRAPPER_DELEGATE (nr_guzzle6_client_construct);
  }
} NR_PHP_WRAPPER_END
