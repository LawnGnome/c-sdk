#include "libnewrelic.h"
#include "datastore.h"
#include "segment.h"
#include "stack.h"

#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

void newrelic_destroy_datastore_segment(
    newrelic_datastore_segment_t** segment_ptr) {
  newrelic_datastore_segment_t* segment;

  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    return;
  }

  segment = *segment_ptr;

  nr_datastore_instance_destroy(&(segment->params.instance));

  nr_free(segment->params.collection);
  nr_free(segment->params.operation);
  nr_free(segment->params.datastore.string);
  nr_free(segment->params.sql.sql);
  nr_realfree((void**)segment_ptr);

  return;
}

newrelic_datastore_segment_t* newrelic_start_datastore_segment(
    newrelic_txn_t* transaction,
    const newrelic_datastore_segment_params_t* params) {
  nr_datastore_t ds_type = NR_DATASTORE_OTHER;
  newrelic_datastore_segment_t* segment = NULL;

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

  /* Zero-allocate to ensure that the nr_node_datastore_params_t embedded
   * within the newrelic_datastore_segment_t struct is correctly initialized. */
  segment = nr_zalloc(sizeof(newrelic_datastore_segment_t));

  /* Indicate the datastore product type */
  ds_type = nr_datastore_from_string(params->product);
  segment->params.datastore.type = ds_type;

  /* While it is type-safe to allow an empty-string product parameter, having
   * such mangles some of the New Relic UI.  Check for an empty string and
   * replace it with a sensible default. */
  if ('\0' == params->product[0]) {
    segment->params.datastore.string = nr_strdup(NEWRELIC_DATASTORE_OTHER);
  } else {
    segment->params.datastore.string = nr_strdup(params->product);
  }

  /* If the ds_type is the default, log that the datastore segment has
   * been created for an unsupported datastore product. Logging this fact here
   * may help to uncover future mysteries in supporting the agent. */
  if (NR_DATASTORE_OTHER == ds_type) {
    nrl_info(NRL_INSTRUMENT, "instrumenting unsupported datastore product");
  }

  /* Copy over what was provided in params */
  segment->params.collection = nr_strdup_or(params->collection, "other");
  segment->params.operation = nr_strdup_or(params->operation, "other");

  /* Build out the appropriate nr_datastore_instance_t.  The axiom call
   * does the work of taking care that a NULL port_or_path_id value
   * is set to its default value, "unknown" */
  segment->params.instance = nr_datastore_instance_create(
      params->host, params->port_path_or_id, params->database_name);

  /* Is this an SQL datastore type?  Then add the query information. */
  if (nr_datastore_is_sql(ds_type)) {
    segment->params.sql.sql = nr_strdup(params->query);

    /* There's no need to separately initialize these fields to NULL
     * given the above nr_zalloc() call; do so deliberately to
     * indicate that these fields are being ignored for now. */
    segment->params.sql.plan_json = NULL;
    segment->params.sql.input_query = NULL;
  }

  // Set callback functions.
  segment->params.callbacks.backtrace = newrelic_get_stack_trace_as_json;
  segment->params.callbacks.modify_table_name = NULL; /* Ignore for now */

  // Set the start time.
  nr_txn_set_time(transaction, &segment->params.start);
  segment->txn = transaction;

  return segment;
}

bool newrelic_end_datastore_segment(
    newrelic_txn_t* transaction,
    newrelic_datastore_segment_t** segment_ptr) {
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

  /* Stop the transaction and save the node. */
  nr_txn_set_time(transaction, &segment->params.stop);
  nr_txn_end_node_datastore(transaction, &segment->params, NULL);

  status = true;

end:
  newrelic_destroy_datastore_segment(segment_ptr);
  return status;
}
