#include "libnewrelic.h"
#include "datastore.h"
#include "segment.h"
#include "stack.h"

#include "util_logging.h"
#include "util_memory.h"
#include "util_sql.h"
#include "util_strings.h"

static char* newrelic_create_datastore_segment_metrics(nrtxn_t* txn,
                                                       nrtime_t duration,
                                                       const char* product,
                                                       const char* collection,
                                                       const char* operation) {
  char* operation_metric = NULL;
  char* rollup_metric = NULL;
  char* scoped_metric = NULL;
  char* statement_metric = NULL;

  nrm_force_add(txn->unscoped_metrics, "Datastore/all", duration);

  rollup_metric = nr_formatf("Datastore/%s/all", product);
  nrm_force_add(txn->unscoped_metrics, rollup_metric, duration);

  operation_metric
      = nr_formatf("Datastore/operation/%s/%s", product, operation);

  if (collection) {
    nrm_add(txn->unscoped_metrics, operation_metric, duration);
    statement_metric = nr_formatf("Datastore/statement/%s/%s/%s", product,
                                  collection, operation);
    scoped_metric = statement_metric;
  } else {
    scoped_metric = operation_metric;
  }

  nrm_add(txn->scoped_metrics, scoped_metric, duration);

  scoped_metric = nr_strdup(scoped_metric);

  nr_free(operation_metric);
  nr_free(rollup_metric);
  nr_free(statement_metric);

  return scoped_metric;
}

void newrelic_destroy_datastore_segment(
    newrelic_datastore_segment_t** segment_ptr) {
  newrelic_datastore_segment_t* segment;

  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    return;
  }

  segment = *segment_ptr;
  nr_free(segment->collection);
  nr_free(segment->operation);
  nr_realfree((void**)segment_ptr);
}

newrelic_datastore_segment_t* newrelic_start_datastore_segment(
    newrelic_txn_t* transaction,
    const newrelic_datastore_segment_params_t* params) {
  nr_datastore_t ds_type = NR_DATASTORE_OTHER;
  newrelic_datastore_segment_t* segment = NULL;
  char* sql_obfuscated = NULL;

  /* Any omitted fields are zero-initialised, but we'll call these ones out
   * because there's no code path to set them at present. */
  nr_segment_datastore_t sp
      = {.input_query_json = NULL, .explain_plan_json = NULL};

  /* Affirm required function parameters and datastore parameters are not
   * NULL */
  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT,
              "cannot start a datastore segment on a NULL transaction");
    return NULL;
  }
  if (NULL == params) {
    nrl_error(NRL_INSTRUMENT, "params cannot be NULL");
    return NULL;
  }
  if (NULL == params->product) {
    nrl_error(NRL_INSTRUMENT, "product param cannot be NULL");
    return NULL;
  }

  /* Perform slash validation on product, collection, operation, and host */
  if (!newrelic_validate_segment_param(params->product, "product")) {
    return NULL;
  }
  if (!newrelic_validate_segment_param(params->collection, "collection")) {
    return NULL;
  }
  if (!newrelic_validate_segment_param(params->operation, "operation")) {
    return NULL;
  }
  if (!newrelic_validate_segment_param(params->host, "host")) {
    return NULL;
  }

  /* Set up the C agent wrapper struct. */
  segment = nr_zalloc(sizeof(newrelic_datastore_segment_t));
  segment->collection = nr_strdup_or(params->collection, "other");
  segment->operation = nr_strdup_or(params->operation, "other");
  segment->txn = transaction;

  /* Start the generic segment. */
  segment->segment = nr_segment_start(transaction, NULL, NULL);

  /* Get the datastore product type, since we need it to figure out SQL
   * behaviour. */
  ds_type = nr_datastore_from_string(params->product);

  /* While it is type-safe to allow an empty-string product parameter, having
   * such mangles some of the New Relic UI.  Check for an empty string and
   * replace it with a sensible default. */
  sp.component
      = '\0' != params->product[0] ? params->product : NEWRELIC_DATASTORE_OTHER;

  /* If the ds_type is the default, log that the datastore segment has
   * been created for an unsupported datastore product. Logging this fact here
   * may help to uncover future mysteries in supporting the agent. */
  if (NR_DATASTORE_OTHER == ds_type) {
    nrl_info(NRL_INSTRUMENT, "instrumenting unsupported datastore product");
  }

  /* Build out the appropriate nr_datastore_instance_t.  The axiom calls do the
   * work of taking care that a NULL port_or_path_id value is set to its
   * default value, "unknown" */
  nr_datastore_instance_set_host(&sp.instance, params->host);
  nr_datastore_instance_set_port_path_or_id(&sp.instance,
                                            params->port_path_or_id);
  nr_datastore_instance_set_database_name(&sp.instance, params->database_name);

  /* Is this an SQL datastore type?  Then add the query information. */
  if (nr_datastore_is_sql(ds_type)) {
    /* Perform obfuscation if required. */
    switch (nr_txn_sql_recording_level(transaction)) {
      case NR_SQL_RAW:
        sp.sql = params->query;
        break;

      case NR_SQL_OBFUSCATED: {
        sql_obfuscated = nr_sql_obfuscate(params->query);
        sp.sql_obfuscated = sql_obfuscated;

        break;
      }

      case NR_SQL_NONE:
      default:
        break;
    }
  }

  /* Finally, set the segment type. */
  nr_segment_set_datastore(segment->segment, &sp);

  /* Clean up and return. */
  nr_datastore_instance_destroy_fields(&sp.instance);
  nr_free(sql_obfuscated);

  return segment;
}

bool newrelic_end_datastore_segment(
    newrelic_txn_t* transaction,
    newrelic_datastore_segment_t** segment_ptr) {
  nrtime_t duration;
  char* name;
  bool status = false;
  newrelic_datastore_segment_t* segment = NULL;

  /* Validate our inputs. We can only early return from the segment check
   * because the documented behaviour is to destroy any segment that's passed
   * in. */
  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    nrl_error(NRL_INSTRUMENT, "cannot end a NULL datastore segment");
    return false;
  }
  segment = *segment_ptr;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT,
              "cannot end a datastore segment on a NULL transaction");
    goto end;
  }

  /* Sanity check that the datastore segment is being ended on the same
   * transaction it was started on. */
  if (transaction != segment->txn) {
    nrl_error(NRL_INSTRUMENT,
              "cannot end a datastore segment on a different transaction to "
              "the one it was created on");
    goto end;
  }

  /* Sanity check that the datastore segment is really a datastore segment. */
  if (NR_SEGMENT_DATASTORE != segment->segment->type) {
    nrl_error(NRL_INSTRUMENT,
              "unexpected datastore segment type: expected %d; got %d",
              (int)NR_SEGMENT_DATASTORE, (int)segment->segment->type);
    goto end;
  }

  /* Stop the transaction and save the node. */
  nr_segment_end(segment->segment);
  duration = nr_time_duration(segment->segment->start_time,
                              segment->segment->stop_time);

  /* Create metrics. */
  name = newrelic_create_datastore_segment_metrics(
      transaction, duration,
      segment->segment->typed_attributes.datastore.component,
      segment->collection, segment->operation);
  nr_segment_set_name(segment->segment, name);
  nr_free(name);

  /* Add backtrace, if required. */
  if (nr_txn_node_datastore_stack_worthy(transaction, duration)) {
    char* backtrace_json = newrelic_get_stack_trace_as_json();

    if (backtrace_json) {
      // TODO: this needs a proper setter in axiom.
      nro_set_hash_jstring(segment->segment->user_attributes, "backtrace",
                           backtrace_json);
      nr_free(backtrace_json);
    }
  }

  status = true;

end:
  newrelic_destroy_datastore_segment(segment_ptr);
  return status;
}
