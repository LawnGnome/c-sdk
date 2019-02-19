#include "libnewrelic.h"
#include "external.h"
#include "segment.h"
#include "transaction.h"

#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_url.h"

static char* newrelic_create_external_segment_metrics(nr_segment_t* segment) {
  const char* domain;
  int domainlen = 0;
  char* metric_name;
  const char* uri = segment->typed_attributes.external.uri;

  /* Rollup metric */
  nr_segment_add_metric(segment, "External/all", false);

  domain = nr_url_extract_domain(uri, nr_strlen(uri), &domainlen);
  if ((0 == domain) || (domainlen <= 0)) {
    domain = "<unknown>";
    domainlen = nr_strlen(domain);
  }

  metric_name = nr_formatf("External/%.*s/all", domainlen, domain);

  /* Scoped metric */
  nr_segment_add_metric(segment, metric_name, true);

  return metric_name;
}

newrelic_segment_t* newrelic_start_external_segment(
    newrelic_txn_t* transaction,
    const newrelic_external_segment_params_t* params) {
  newrelic_segment_t* segment = NULL;

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

  nrt_mutex_lock(&transaction->lock);
  {
    segment = newrelic_segment_create(transaction->txn);
    if (NULL == segment) {
      goto unlock_and_end;
    }

    nr_segment_set_external(segment->segment,
                            &((nr_segment_external_t){
                                .transaction_guid = NULL,
                                .uri = params->uri,
                                .library = params->library,
                                .procedure = params->procedure,
                            }));

  unlock_and_end:;
  }
  nrt_mutex_unlock(&transaction->lock);

  return segment;
}

bool newrelic_end_external_segment(newrelic_segment_t* segment) {
  char* name;

  /* Sanity check that the external segment is really an external segment. */
  if (nrunlikely(NR_SEGMENT_EXTERNAL != segment->segment->type)) {
    nrl_error(NRL_INSTRUMENT,
              "unexpected external segment type: expected %d; got %d",
              (int)NR_SEGMENT_EXTERNAL, (int)segment->segment->type);
    return false;
  }

  /* Create metrics. */
  name = newrelic_create_external_segment_metrics(segment->segment);
  nr_segment_set_name(segment->segment, name);
  nr_free(name);

  return true;
}
