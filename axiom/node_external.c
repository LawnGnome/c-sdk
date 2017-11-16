#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_header.h"
#include "nr_txn.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_url.h"

/*
 * Purpose : This function is designed to avoid repetition in the transaction
 *           trace by collapsing adjacent calls to the same function.
 *
 * Params  : 1. The current transaction.
 *           2. The start time of the just completed call.
 *           3. The stop time of the just completed call.
 *           4. The metric/node name of the just completed call.
 *
 * Returns : NR_FAILURE if the call could not be collapsed into the last
 *           added transaction trace node, or NR_SUCCESS if the collapse was
 *           successful and a new node should not be created.
 *
 * Notes   : This function has a problem:  If rollup occurs, the priority queue
 *           is not updated with the node's new duration.
 */
nr_status_t
nr_txn_node_rollup (nrtxn_t *txn, const nrtxntime_t *start, const nrtxntime_t *stop, const char *name)
{
  nrtxnnode_t *rollup;
  const char *rollup_name;

  if (nrunlikely ((0 == txn) || (0 == start) || (0 == stop) || (0 == name))) {
    return NR_FAILURE;
  }

  rollup = txn->last_added;

  if (0 == rollup) {
    return NR_FAILURE;
  }

  rollup_name = nr_string_get (txn->trace_strings, rollup->name);

  /* 
   * The would-be-node must have the same name as the rollup.
   */
  if (0 != nr_strcmp (name, rollup_name)) {
    return NR_FAILURE;
  }

  /*
   * The rollup node cannot have any kids. This is possible iff the rollup node
   * has sequential stamps.
   */
  if (1 != (rollup->stop_time.stamp - rollup->start_time.stamp)) {
    return NR_FAILURE;
  }

  /*
   * Previously, here we tested if the would-be-node ended directly
   * after the rollup (no calls between them that didn't get saved).  This
   * condition has been eliminated since we want to aggressively roll
   * together nodes.
   */

  /*
   * At this point we are successful, and the would-be node is being rolled in.
   */
  rollup->count++;

  /*
   * Update the stamps on the rollup so that future siblings can also be rolled
   * in.
   */
  rollup->start_time.stamp = start->stamp;
  rollup->stop_time.stamp = stop->stamp;

  /*
   * We incorporate the time of the would-be node into the rollup. Note that
   * time between the rollup and the start of the would-be-node is being
   * attributed to this rollup. This may be inelegant, but it is clean and
   * consistent with the UI rollups.
   */
  rollup->stop_time.when = stop->when;

  return NR_SUCCESS;
}

/*
 * Purpose : Create metrics for a completed external call.
 *
 * Returns : A string pool index of the name that should be used for
 *           a transaction trace node.
 *
 *
 * Metrics Created
 * Name                                                         Scope      Condition
 * ----                                                         -----      ---------
 * External/all                                                 Unscoped   Always
 * External/allWeb                                              Unscoped   Web Transactions
 * External/allOther                                            Unscoped   Background Tasks
 * External/{host}/all                                          Unscoped   Always
 * External/{host}/all                                          Scoped     Cross-Process Absent (Used for Node Name)
 * ExternalApp/{host}/{external_id}/all                         Unscoped   Cross-Process Present
 * ExternalTransaction/{host}/{external_id}/{external_txnname}  Unscoped   Cross-Process Present
 * ExternalTransaction/{host}/{external_id}/{external_txnname}  Scoped     Cross-Process Present (Used for Node Name)
 *
 * These metrics are dictated by the spec located here:
 * https://source.datanerd.us/agents/agent-specs/blob/master/Cross-Application-Tracing-PORTED.md
 */
static char *
node_external_create_metrics (
  nrtxn_t *txn,
  nrtime_t duration,
  const char *url,
  int urllen,
  const char *external_id,
  const char *external_txnname)
{
  char buf[1024];
  const int buflen = sizeof (buf);
  const char *domain;
  int domainlen = 0;

  if (0 == txn) {
    return 0;
  }

  /* Rollup metric */
  nrm_force_add (txn->unscoped_metrics, "External/all", duration);

  domain = nr_url_extract_domain (url, urllen, &domainlen);
  if ((0 == domain) || (domainlen <= 0) || (domainlen >= (buflen - 256))) {
    domain = "<unknown>";
    domainlen = nr_strlen (domain);
  }

  buf[0] = 0;
  snprintf (buf, buflen, "External/%.*s/all", domainlen, domain);

  if (external_id && external_txnname) {
    nrm_add (txn->unscoped_metrics, buf, duration);
    /*
     * Cross process present.
     */
    snprintf (buf, buflen, "ExternalApp/%.*s/%s/all", domainlen, domain, external_id);
    nrm_add (txn->unscoped_metrics, buf, duration);

    snprintf (buf, buflen, "ExternalTransaction/%.*s/%s/%s", domainlen, domain, external_id, external_txnname);
  }

  /* Scoped metric */
  nrm_add (txn->scoped_metrics, buf, duration);

  return nr_strdup (buf);
}

void
nr_txn_do_end_node_external (
  nrtxn_t *txn,
  const char *async_context,
  const nrtxntime_t *start,
  const nrtxntime_t *stop,
  const char *url,
  int urllen,
  int do_rollup,
  const char *encoded_response_header)
{
  nrtime_t duration;
  char *cleaned_url;
  char *node_name = 0;
  char *external_id = 0;
  char *external_txnname = 0;
  char *external_guid = 0;
  nrobj_t *data_hash = 0;

  if (0 == nr_txn_valid_node_end (txn, start, stop)) {
    return;
  }

  duration = nr_time_duration (start->when, stop->when);

  if (0 == async_context) {
    nr_txn_adjust_exclusive_time (txn, duration);
  }

  if (encoded_response_header) {
    nr_header_outbound_response (txn, encoded_response_header, &external_id, &external_txnname, &external_guid);
  }

  node_name = node_external_create_metrics (txn, duration, url, urllen, external_id, external_txnname);

  if (1 == do_rollup) {
    if (NR_SUCCESS == nr_txn_node_rollup (txn, start, stop, node_name)) {
      goto leave;
    }
  }

  data_hash = nro_new_hash ();
  if (external_guid) {
    nro_set_hash_string (data_hash, "transaction_guid", external_guid);
  }
  cleaned_url = nr_url_clean (url, urllen);
  if (cleaned_url) {
    nro_set_hash_string (data_hash, "uri", cleaned_url);
    nr_free (cleaned_url);
  }

  nr_txn_save_trace_node (txn, start, stop, node_name, async_context, data_hash);

  /* Fall through to */
leave:
  nr_free (node_name);
  nro_delete (data_hash);
  nr_free (external_id);
  nr_free (external_txnname);
  nr_free (external_guid);
}

void
nr_txn_end_node_external (
  nrtxn_t *txn,
  const nrtxntime_t *start,
  const char *url,
  int urllen,
  int do_rollup,
  const char *encoded_response_header)
{
  nrtxntime_t stop;

  stop.stamp = 0;
  stop.when = 0;
  nr_txn_set_time (txn, &stop);

  nr_txn_do_end_node_external (txn, NULL, start, &stop, url, urllen, do_rollup, encoded_response_header);
}

void
nr_txn_end_node_external_async (
  nrtxn_t *txn,
  const char *async_context,
  const nrtxntime_t *start,
  nrtime_t duration,
  const char *url,
  int urllen,
  int do_rollup,
  const char *encoded_response_header)
{
  nrtxntime_t stop;

  stop.stamp = 0;
  stop.when = 0;
  nr_txn_set_time (txn, &stop);
  stop.when = start->when + duration;

  nr_txn_do_end_node_external (txn, async_context, start, &stop, url, urllen, do_rollup, encoded_response_header);
}
