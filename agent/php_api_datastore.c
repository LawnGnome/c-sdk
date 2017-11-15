#include "php_agent.h"

#include <stdbool.h>

#include "nr_datastore.h"
#include "php_api.h"
#include "php_api_datastore.h"
#include "php_api_datastore_private.h"
#include "php_call.h"
#include "php_hash.h"
#include "util_logging.h"
#include "util_sql.h"

static const char *
get_array_string (const zval *zv, const char *key) {
  const zval *value = nr_php_zend_hash_find (Z_ARRVAL_P (zv), key);

  if (!nr_php_is_zval_valid_string (value)) {
    return NULL;
  }
  return Z_STRVAL_P (value);
}

nr_datastore_instance_t *
nr_php_api_datastore_create_instance_from_params (const zval *params)
{
  const char *database_name = get_array_string (params, "databaseName");
  const char *host = get_array_string (params, "host");
  const char *port_path_or_id = get_array_string (params, "portPathOrId");

  return nr_datastore_instance_create (host, port_path_or_id, database_name);
}

zval *
nr_php_api_datastore_validate (const HashTable *params)
{
  size_t i;
  zval *validated_params = nr_php_zval_alloc ();

  array_init (validated_params);

  for (i = 0; i < num_datastore_validators; i++) {
    const char *key = datastore_validators[i].key;
    zval *orig = nr_php_zend_hash_find (params, key);

    if (NULL == orig) {
      if (datastore_validators[i].required) {
        zend_error (E_WARNING, "Missing datastore parameter: %s", key);

        nr_php_zval_free (&validated_params);
        return NULL;
      } else if (datastore_validators[i].default_value) {
        char *default_value = nr_alloca (sizeof (datastore_validators[i].default_value));

        nr_strcpy (default_value, datastore_validators[i].default_value);
        nr_php_add_assoc_string (validated_params, key, default_value);
      }
    } else {
      zval *copy = nr_php_zval_alloc ();

      ZVAL_DUP (copy, orig);

      /*
       * This call can result in errors bubbling back to the user, but since
       * they're indicative of type issues, that's OK. We don't have a way to
       * capture this anyway.
       */
      convert_to_explicit_type (copy, datastore_validators[i].final_type);
      nr_php_add_assoc_zval (validated_params, key, copy);

      nr_php_zval_free (&copy);
    }
  }

  return validated_params;
}

#ifdef TAGS
void zif_newrelic_record_datastore_segment (void);  /* ctags landing pad only */
void     newrelic_record_datastore_segment (void);  /* ctags landing pad only */
#endif
PHP_FUNCTION (newrelic_record_datastore_segment)
{
  zend_fcall_info_cache fcc;
  zend_fcall_info fci;
  zval *input_params = NULL;
  nr_slowsqls_labelled_query_t input_query = {
    .name = NULL,
    .query = NULL,
  };
  bool instrument = true;
  nr_node_datastore_params_t node_params = {
    .callbacks = {
      .backtrace = nr_php_backtrace_callback,
    },
  };
  zval *retval = NULL;
  zval *validated_params = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  RETVAL_FALSE;

  if (0 == nr_php_recording (TSRMLS_C)) {
    /*
     * As it's possible for recording to be disabled if the application just
     * hasn't connected yet, we still want to execute the callback to avoid
     * causing queries to silently fail. See also PHP-1398.
     */
    instrument = false;
  } else {
    nr_php_api_add_supportability_metric ("record_datastore_segment" TSRMLS_CC);
  }

  if (FAILURE == zend_parse_parameters (ZEND_NUM_ARGS () TSRMLS_CC, "fa", &fci, &fcc, &input_params)) {
    /*
     * This is the one true early return: in all other cases, we'll still
     * execute the callback, but if the parameters are straight up invalid
     * we'll just let zend_parse_parameters() warn the user and return false
     * like a good internal function.
     */
    nrl_warning (NRL_API, "unable to parse parameters to newrelic_record_datastore_segment; %d parameters received", ZEND_NUM_ARGS ());
    return;
  }

  if (instrument) {
    validated_params = nr_php_api_datastore_validate (Z_ARRVAL_P (input_params));
    if (NULL == validated_params) {
      /*
       * In this case, nr_php_api_datastore_validate() will have generated a user
       * visible warning, so we don't need to log anything.
       */
      instrument = false;
    }
  }

  if (instrument) {
    nr_txn_set_time (NRPRG (txn), &node_params.start);
  }
  retval = nr_php_call_fcall_info (fci, fcc);
  ZVAL_ZVAL (return_value, retval, 0, 1);
#ifdef PHP7
    /*
     * Calling ZVAL_ZVAL with dtor set to true in PHP 7 won't free the
     * surrounding wrapper.
     */
  efree (retval);
#endif /* PHP7 */

  /*
   * Bail early if an error occurred earlier and we're not instrumenting the
   * call.
   */
  if (!instrument) {
    goto end;
  }

  nr_txn_set_time (NRPRG (txn), &node_params.stop);

  /*
   * Now we can build up the datastore node parameters.
   */
  node_params.collection = get_array_string (validated_params, "collection");
  node_params.operation = get_array_string (validated_params, "operation");
  node_params.instance = nr_php_api_datastore_create_instance_from_params (validated_params);

  node_params.datastore.string = get_array_string (validated_params, "product");
  node_params.datastore.type = nr_datastore_from_string (node_params.datastore.string);

  node_params.sql.sql = get_array_string (validated_params, "query");

  input_query.name = get_array_string (validated_params, "inputQueryLabel");
  input_query.query = get_array_string (validated_params, "inputQuery");
  if (input_query.name && input_query.query) {
    node_params.sql.input_query = &input_query;
  }

  nr_txn_end_node_datastore (NRPRG (txn), &node_params, NULL);

end:
  nr_datastore_instance_destroy (&node_params.instance);
  nr_php_zval_free (&validated_params);
}

