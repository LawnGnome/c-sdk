#include "nr_axiom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "nr_datastore.h"
#include "nr_slowsqls.h"
#include "nr_txn.h"
#include "node_datastore.h"
#include "node_datastore_private.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_sql.h"
#include "util_strings.h"

static char *
create_metrics (nrtxn_t *txn, nrtime_t duration, const char *product,
                const char *collection, const char *operation)
{
  char *operation_metric = NULL;
  char *rollup_metric = NULL;
  char *scoped_metric = NULL;
  char *statement_metric = NULL;

  nrm_force_add (txn->unscoped_metrics, "Datastore/all", duration);

  rollup_metric = nr_formatf ("Datastore/%s/all", product);
  nrm_force_add (txn->unscoped_metrics, rollup_metric, duration);

  operation_metric = nr_formatf ("Datastore/operation/%s/%s", product, operation);

  if (collection) {
    nrm_add (txn->unscoped_metrics, operation_metric, duration);
    statement_metric = nr_formatf ("Datastore/statement/%s/%s/%s", product, collection, operation);
    scoped_metric = statement_metric;
  } else {
    scoped_metric = operation_metric;
  }

  nrm_add (txn->scoped_metrics, scoped_metric, duration);

  scoped_metric = nr_strdup (scoped_metric);

  nr_free (operation_metric);
  nr_free (rollup_metric);
  nr_free (statement_metric);

  return scoped_metric;
}

static void
handle_instance (nrtxn_t *txn, nrtime_t duration,
                 const nr_datastore_instance_t *instance, const char *product,
                 nrobj_t *data_hash)
{
  if (instance) {
    if (txn->options.instance_reporting_enabled) {
      char *instance_metric = NULL;

      instance_metric = nr_formatf ("Datastore/instance/%s/%s/%s",
        product, instance->host, instance->port_path_or_id);
      nrm_add (txn->unscoped_metrics, instance_metric, duration);
      nro_set_hash_string (data_hash, "host", instance->host);
      nro_set_hash_string (data_hash, "port_path_or_id", instance->port_path_or_id);

      nr_free (instance_metric);
    }

    if (txn->options.database_name_reporting_enabled) {
      nro_set_hash_string (data_hash, "database_name", instance->database_name);
    }
  }
}

void
nr_txn_end_node_datastore (
  nrtxn_t *txn,
  const nr_node_datastore_params_t *params,
  char **return_scoped_metric)
{
  char *backtrace_json = NULL;
  const char *datastore_string = NULL;
  nrtime_t duration;
  char *input_query_json = NULL;
  const char *operation = NULL;
  nrobj_t *data_hash = NULL;
  char *collection_allocated = NULL;
  const char *collection = NULL;
  bool is_sql = false;
  char *scoped_metric = NULL;
  const char *sql = NULL;
  char *sql_allocated = NULL;
  char *input_query_name = NULL;
  char *input_query_query = NULL;
  nr_slowsqls_labelled_query_t input_query_allocated = { NULL, NULL };
  const nr_slowsqls_labelled_query_t *input_query = NULL;

  if (return_scoped_metric) {
    *return_scoped_metric = NULL;
  }

  if (NULL == params) {
    return;
  }

  /*
   * This also checks if the transaction is non-NULL.
   */
  if (0 == nr_txn_valid_node_end (txn, &params->start, &params->stop)) {
    return;
  }

  if (nr_datastore_is_sql (params->datastore.type)) {
    /*
     * If the datastore type is known to be SQL, we can go ahead and try to
     * extract the collection and operation from the input SQL, if it was given.
     */
    is_sql = true;
    datastore_string = nr_datastore_as_string (params->datastore.type);

    if ((NULL == params->collection) || (NULL == params->operation)) {
      collection_allocated = nr_txn_node_sql_get_operation_and_table (txn,
                                                                      &operation,
                                                                      params->sql.sql,
                                                                      params->callbacks.modify_table_name);
      collection = collection_allocated;
    }
  } else {
    /*
     * Otherwise, let's ensure the datastore string is set correctly: if
     * NR_DATASTORE_OTHER is the type, then we should use the string parameter,
     * otherwise we use the string representation of the type and ignore the
     * string parameter, even if it was given, since we want to minimise the
     * risk of MGI.
     */
    datastore_string = (NR_DATASTORE_OTHER == params->datastore.type)
                       ? params->datastore.string
                       : nr_datastore_as_string (params->datastore.type);
  }

  /*
   * At this point, there's no way to have a NULL datastore string without the
   * input parameters being straight up invalid, so we'll just log and get out.
   */
  if (NULL == datastore_string) {
    nrl_verbosedebug (NRL_SQL,
                      "%s: unable to get datastore string from type %d",
                      __func__, (int) params->datastore.type);
    goto end;
  }

  /*
   * Otherwise, we need to add the datastore string to those the transaction has
   * seen so that the correct rollup metrics are created when the transaction
   * ends.
   */
  nr_string_add (txn->datastore_products, datastore_string);

  /*
   * If the collection or operation are set in the parameters, we'll always use
   * those, even if we extracted them from the SQL earlier.
   */
  if (params->collection) {
    collection = params->collection;
  }
  nr_string_add (txn->datastore_products, datastore_string);

  if (params->operation) {
    operation = params->operation;
  } else if (NULL == operation) {
    /*
     * The operation is a bit special: if it's not set, then we should set it to
     * "other".
     */
    operation = "other";
  }

  duration = nr_time_duration (params->start.when, params->stop.when);
  nr_txn_adjust_exclusive_time (txn, duration);

  if (params->sql.input_query) {
    input_query = params->sql.input_query;
  }

  /*
   * Add the metrics that we can reasonably add at this point.
   *
   * The allWeb and allOther rollup metrics are created at the end of the
   * transaction since the background status may change.
   *
   * See: https://source.datanerd.us/agents/agent-specs/blob/master/Datastore-Metrics-PORTED.md
   */
  scoped_metric = create_metrics (txn, duration, datastore_string, collection,
                                  operation);
  if (return_scoped_metric) {
    *return_scoped_metric = nr_strdup (scoped_metric);
  }

  /*
   * Now we're done with the metrics, we'll begin setting up the data hash for
   * the potential trace node.
   */
  data_hash = nro_new_hash ();

  /*
   * Generate a backtrace if the query was slow enough and we have a callback
   * that allows us to do so.
   */
  if (params->callbacks.backtrace && nr_txn_node_datastore_stack_worthy (txn, duration)) {
    backtrace_json = (params->callbacks.backtrace) ();
    nro_set_hash_jstring (data_hash, "backtrace", backtrace_json);
  }

  /*
   * If a datastore instance was provided, we need to add the relevant metrics
   * and hash values.
   */
  handle_instance (txn, duration, params->instance, datastore_string, data_hash);

  /*
   * Add the explain plan, if we have one.
   */
  if (params->sql.plan_json) {
    nro_set_hash_jstring (data_hash, "explain_plan", params->sql.plan_json);
  }

  /*
   * If the datastore is a SQL datastore and we have a query, then we need to
   * add the query to the data hash, being mindful of the user's obfuscation and
   * security settings. This is also the point we'll handle any input query that
   * was used.
   *
   * We set these to function scoped variables because we can also use these in
   * any slowsql that we save.
   */
  if (is_sql) {
    switch (nr_txn_sql_recording_level (txn)) {
      case NR_SQL_RAW:
        sql = params->sql.sql;
        nro_set_hash_string (data_hash, "sql", sql);
        break;

      case NR_SQL_OBFUSCATED:
        sql_allocated = nr_sql_obfuscate (params->sql.sql);
        sql = sql_allocated;
        nro_set_hash_string (data_hash, "sql_obfuscated", sql);

        /*
         * If it's set, we have to replace input_query with the obfuscated
         * version of the input_query.
         */
        if (params->sql.input_query) {
          input_query_allocated.query = input_query_query = nr_sql_obfuscate (params->sql.input_query->query);
          input_query_allocated.name = input_query_name = nr_strdup (params->sql.input_query->name);
          input_query = &input_query_allocated;
        }
        break;

      case NR_SQL_NONE: /* FALLTHROUGH */
      default:
        break;
    }
  }

  if (input_query) {
    nrobj_t *obj = nro_new_hash ();

    nro_set_hash_string (obj, "label", input_query->name);
    nro_set_hash_string (obj, "query", input_query->query);
    input_query_json = nro_to_json (obj);
    nro_set_hash_jstring (data_hash, "input_query", input_query_json);

    nro_delete (obj);
  }

  nr_txn_save_trace_node (txn, &params->start, &params->stop, scoped_metric,
                          params->async_context, data_hash);

  // TODO: add a special flag to allow this to fire even if it's not SQL.
  if (is_sql && nr_txn_node_potential_slowsql (txn, duration)) {
    nr_slowsqls_params_t slowsqls_params = {
      .sql              = sql,
      .duration         = duration,
      .stacktrace_json  = backtrace_json,
      .metric_name      = scoped_metric,
      .plan_json        = params->sql.plan_json,
      .input_query_json = input_query_json,
      .instance         = params->instance,
      .instance_reporting_enabled      = txn->options.instance_reporting_enabled,
      .database_name_reporting_enabled = txn->options.database_name_reporting_enabled,
    };

    nr_slowsqls_add (txn->slowsqls, &slowsqls_params);
  }

end:
  nro_delete (data_hash);
  nr_free (collection_allocated);
  nr_free (backtrace_json);
  nr_free (sql_allocated);
  nr_free (scoped_metric);
  nr_free (input_query_name);
  nr_free (input_query_query);
  nr_free (input_query_json);
}

int
nr_txn_node_potential_explain_plan (const nrtxn_t *txn, nrtime_t duration)
{
  if (NULL == txn) {
    return 0;
  }

  return (txn->options.ep_enabled &&
          nr_txn_node_potential_slowsql (txn, duration));
}

int
nr_txn_node_potential_slowsql (const nrtxn_t *txn, nrtime_t duration)
{
  if (0 == txn) {
    return 0;
  }
  if (NR_SQL_NONE == txn->options.tt_recordsql) {
    return 0;
  }
  if (0 == txn->options.tt_slowsql) {
    return 0;
  }

  if (duration >= txn->options.ep_threshold) {
    return 1;
  }

  return 0;
}

char *
nr_txn_node_sql_get_operation_and_table (
  const nrtxn_t *txn,
  const char **operation_ptr,
  const char *sql,
  nr_modify_table_name_fn_t modify_table_name_fn)
{
  char *table = NULL;

  if (operation_ptr) {
    *operation_ptr = NULL;
  }
  if (NULL == txn) {
    return NULL;
  }
  if (txn->special_flags.no_sql_parsing) {
    return NULL;
  }

  nr_sql_get_operation_and_table (sql, operation_ptr, &table, txn->special_flags.show_sql_parsing);
  if (NULL == table) {
    return NULL;
  }

  if (modify_table_name_fn) {
    modify_table_name_fn (table);
  }

  return table;
}

int
nr_txn_node_datastore_stack_worthy (const nrtxn_t *txn, nrtime_t duration)
{
  if (0 == txn) {
    return 0;
  }

  if ((txn->options.ss_threshold > 0) && (duration >= txn->options.ss_threshold)) {
    return 1;
  }

  if (0 != txn->options.tt_slowsql) {
    if (duration >= txn->options.ep_threshold) {
      return 1;
    }
  }

  return 0;
}
