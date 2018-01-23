#include "libnewrelic.h"
#include "external.h"

#include "node_external.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

bool newrelic_validate_external_param(const char* in, const char* name, bool accept_null) {
  if ((false == accept_null) && (NULL == in)) {
    nrl_error(NRL_INSTRUMENT, "%s cannot be NULL", name);
    return false;
  }

  /* Because external parameters are used in metric names, they cannot include
   * slashes. */
  if (nr_strchr(in, '/')) {
    nrl_error(NRL_INSTRUMENT, "%s cannot include a slash", name);
    return false;
  }

  return true;
}

newrelic_external_segment_t* newrelic_start_external_segment(newrelic_txn_t* transaction,
                                                             const newrelic_external_segment_params_t *params) {
  newrelic_external_segment_t* segment = NULL;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "cannot start an external segment on a NULL transaction");
    return NULL;
  }

  if (NULL == params) {
    nrl_error(NRL_INSTRUMENT, "params cannot be NULL");
    return NULL;
  }

  if (!newrelic_validate_external_param(params->library, "library", true)) {
    return NULL;
  }

  if (!newrelic_validate_external_param(params->procedure, "procedure", false)) {
    return NULL;
  }

  if (NULL == params->uri) {
    nrl_error(NRL_INSTRUMENT, "uri cannot be NULL");
    return NULL;
  }

  segment = nr_zalloc(sizeof(newrelic_external_segment_t));
  nr_txn_set_time(transaction, &segment->start);
  segment->txn       = transaction;
  segment->library   = nr_strdup(params->library ? params->library : "unknown");
  segment->procedure = nr_strdup(params->procedure);
  segment->uri       = nr_strdup(params->uri);

  return segment;
}

bool newrelic_end_external_segment(newrelic_txn_t* transaction,
                                   newrelic_external_segment_t** segment_ptr) {
  newrelic_external_segment_t* segment = NULL;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "cannot end an external segment on a NULL transaction");
    return false;
  }

  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    nrl_error(NRL_INSTRUMENT, "cannot end a NULL external segment");
    return false;
  }

  segment = *segment_ptr;
  if (transaction != segment->txn) {
    nrl_error(NRL_INSTRUMENT, "cannot end an external segment on a different transaction to the one it was created on");
    return false;
  }

  // TODO: something useful with the procedure and library when axiom can.
  nr_txn_end_node_external(transaction, &segment->start, segment->uri, nr_strlen(segment->uri), 0, NULL);

  nr_free(segment->library);
  nr_free(segment->procedure);
  nr_free(segment->uri);
  nr_realfree((void **) segment_ptr);

  return true;
}
