#include "libnewrelic.h"
#include "external.h"
#include "segment.h"

#include "node_external.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

void newrelic_destroy_external_segment(
    newrelic_external_segment_t** segment_ptr) {
  newrelic_external_segment_t* segment;

  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    return;
  }

  segment = *segment_ptr;

  nr_free(segment->params.library);
  nr_free(segment->params.procedure);
  nr_free(segment->params.url);
  nr_realfree((void**)segment_ptr);
}

newrelic_external_segment_t* newrelic_start_external_segment(
    newrelic_txn_t* transaction,
    const newrelic_external_segment_params_t* params) {
  newrelic_external_segment_t* segment = NULL;

  /* Validate our inputs. */
  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT,
              "cannot start an external segment on a NULL transaction");
    return NULL;
  }

  if (NULL == params) {
    nrl_error(NRL_INSTRUMENT, "params cannot be NULL");
    return NULL;
  }

  if (!newrelic_validate_segment_param(params->library, "library")) {
    return NULL;
  }

  if (!newrelic_validate_segment_param(params->procedure, "procedure")) {
    return NULL;
  }

  if (NULL == params->uri) {
    nrl_error(NRL_INSTRUMENT, "uri cannot be NULL");
    return NULL;
  }

  /* We zero-allocate to ensure that the nr_node_external_params_t embedded
   * within the newrelic_external_segment_t struct is correctly initialised. */
  segment = nr_zalloc(sizeof(newrelic_external_segment_t));

  /* Set the fields that we care about. */
  nr_txn_set_time(transaction, &segment->params.start);
  segment->txn = transaction;
  segment->params.library = params->library ? nr_strdup(params->library) : NULL;
  segment->params.procedure =
      params->procedure ? nr_strdup(params->procedure) : NULL;
  segment->params.url = nr_strdup(params->uri);
  segment->params.urllen = nr_strlen(segment->params.url);

  return segment;
}

bool newrelic_end_external_segment(newrelic_txn_t* transaction,
                                   newrelic_external_segment_t** segment_ptr) {
  bool status = false;
  newrelic_external_segment_t* segment = NULL;

  /* Validate our inputs. We can only early return from the segment check
   * because the documented behaviour is to destroy any segment that's passed
   * in. */
  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    nrl_error(NRL_INSTRUMENT, "cannot end a NULL external segment");
    return false;
  }
  segment = *segment_ptr;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT,
              "cannot end an external segment on a NULL transaction");
    goto end;
  }

  /* Sanity check that the external segment is being ended on the same
   * transaction it was started on. Transitioning an external segment between
   * transactions would be problematic, since times are transaction-specific.
   * */
  if (transaction != segment->txn) {
    nrl_error(NRL_INSTRUMENT,
              "cannot end an external segment on a different transaction to "
              "the one it was created on");
    goto end;
  }

  /* Stop the transaction and save the node. */
  nr_txn_set_time(transaction, &segment->params.stop);
  nr_txn_end_node_external(transaction, &segment->params);

  status = true;

end:
  newrelic_destroy_external_segment(segment_ptr);
  return status;
}
