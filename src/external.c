#include "libnewrelic.h"
#include "external.h"
#include "segment.h"

#include "node_external.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_url.h"

static char* newrelic_create_external_segment_metrics(
    nrtxn_t* txn,
    const nr_segment_t* segment,
    nrtime_t duration) {
  const char* domain;
  int domainlen = 0;
  char* metric_name;
  const char* uri = segment->typed_attributes.external.uri;

  /* Rollup metric */
  nrm_force_add(txn->unscoped_metrics, "External/all", duration);

  domain = nr_url_extract_domain(uri, nr_strlen(uri), &domainlen);
  if ((0 == domain) || (domainlen <= 0)) {
    domain = "<unknown>";
    domainlen = nr_strlen(domain);
  }

  metric_name = nr_formatf("External/%.*s/all", domainlen, domain);

  /* Scoped metric */
  nrm_add(txn->scoped_metrics, metric_name, duration);

  return metric_name;
}

void newrelic_destroy_external_segment(
    newrelic_external_segment_t** segment_ptr) {
  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    return;
  }

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
  segment->segment = nr_segment_start(transaction, NULL, NULL);
  segment->txn = transaction;

  nr_segment_set_external(segment->segment, &((nr_segment_external_t){
                                                .transaction_guid = NULL,
                                                .uri = params->uri,
                                                .library = params->library,
                                                .procedure = params->procedure,
                                            }));

  return segment;
}

bool newrelic_end_external_segment(newrelic_txn_t* transaction,
                                   newrelic_external_segment_t** segment_ptr) {
  nrtime_t duration;
  char* name;
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

  /* Sanity check that the external segment is really an external segment. */
  if (NR_SEGMENT_EXTERNAL != segment->segment->type) {
    nrl_error(NRL_INSTRUMENT,
              "unexpected external segment type: expected %d; got %d",
              (int)NR_SEGMENT_EXTERNAL, (int)segment->segment->type);
    goto end;
  }

  /* Stop the transaction and save the node. */
  nr_segment_end(segment->segment);
  duration = nr_time_duration(segment->segment->start_time,
                              segment->segment->stop_time);

  /* Create metrics. */
  name = newrelic_create_external_segment_metrics(segment->txn,
                                                  segment->segment, duration);
  nr_segment_set_name(segment->segment, name);
  nr_free(name);

  status = true;

end:
  newrelic_destroy_external_segment(segment_ptr);
  return status;
}
