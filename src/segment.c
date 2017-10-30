#include "segment.h"

#include "util_logging.h"
#include "util_strings.h"

bool newrelic_validate_segment_param(const char* in, const char* name) {
  if (nr_strchr(in, '/')) {
    if (NULL == name) {
      nrl_error(NRL_INSTRUMENT, "parameter cannot include a slash");
    } else {
      nrl_error(NRL_INSTRUMENT, "%s cannot include a slash", name);
    }
    return false;
  }

  return true;
}

newrelic_segment_t *
newrelic_start_segment (newrelic_txn_t *transaction, const char *name) {
  newrelic_segment_t *segment;
  nrtxntime_t *start;

  if (NULL == transaction) {
    nrl_error (NRL_INSTRUMENT, "unable to start segment with NULL transaction");
    return NULL;
  }

  if (NULL == name) {
    name = "NULL";
  }

  start = (nrtxntime_t *) nr_zalloc (sizeof (nrtxntime_t));
  nr_txn_set_time (transaction, start);

  segment = nr_zalloc (sizeof (newrelic_segment_t));
  segment->transaction = transaction;
  segment->name = name;
  segment->start = start;

  return segment;
}

void newrelic_end_segment (newrelic_segment_t **segment) {
  char *scoped_metric = NULL;
  nrtxntime_t *stop;
  nrtime_t duration;

  if ((NULL == segment) || (NULL == *segment)) {
    nrl_error (NRL_INSTRUMENT, "unable to end a NULL segment");
    return;
  }

  if (NULL == (*segment)->transaction) {
    nrl_error (NRL_INSTRUMENT, "unable to end a segment of a NULL transaction");
    return;
  }

  stop = (nrtxntime_t *) nr_zalloc (sizeof (nrtxntime_t));
  nr_txn_set_time ((*segment)->transaction, stop);

  duration = nr_time_duration ((*segment)->start->when, stop->when);

  scoped_metric = nr_formatf ("Custom/%s", (*segment)->name);
  nrm_add ((*segment)->transaction->scoped_metrics, scoped_metric, duration);
  nrl_verbose (NRL_INSTRUMENT,
    "added segment %s with duration " NR_TIME_FMT "!",
    scoped_metric,
    duration);

  nr_free (scoped_metric);
  nr_free (stop);
  nr_free ((*segment)->start);
  nr_free ((*segment));

  return;
}

