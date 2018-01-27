#include "libnewrelic.h"
#include "segment.h"

#include "util_logging.h"
#include "util_memory.h"
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

// TODO: category?
newrelic_segment_t* newrelic_start_segment(newrelic_txn_t* transaction,
                                           const char* name) {
  newrelic_segment_t* segment;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "unable to start segment with NULL transaction");
    return NULL;
  }

  segment = nr_zalloc(sizeof(newrelic_segment_t));
  segment->transaction = transaction;
  // TODO: validate
  segment->name = nr_strdup(name ? name : "NULL");

  start = (nrtxntime_t *) nr_zalloc (sizeof (nrtxntime_t));
  nr_txn_set_time (transaction, start);

  nr_txn_set_time(transaction, &segment->start);

  return segment;
}

void newrelic_end_segment (newrelic_segment_t **segment) {
  char *scoped_metric = NULL;
  nrtxntime_t *stop;
  nrtime_t duration;
  nrtime_t exclusive;
  char* metric_name = NULL;
  newrelic_segment_t* segment;
  bool status = false;
  nrtxntime_t stop;

  if ((NULL == segment_ptr) || (NULL == *segment_ptr)) {
    nrl_error(NRL_INSTRUMENT, "unable to end a NULL segment");
    return false;
  }
  segment = *segment_ptr;

  if (NULL == segment->transaction) {
    nrl_error(NRL_INSTRUMENT, "unable to end a segment of a NULL transaction");
    goto end;
  }

  /* Sanity check that the segment is being ended on the same transaction it
   * was started on. Transitioning a segment between transactions would be
   * problematic, since times are transaction-specific.
   * */
  if (transaction != segment->transaction) {
    nrl_error(NRL_INSTRUMENT, "cannot end a segment on a different transaction to the one it was created on");
    goto end;
  }

  nr_txn_set_time(transaction, &stop);
  duration = nr_time_duration(segment->start.when, stop.when);


  duration = nr_time_duration ((*segment)->start->when, stop->when);

  status = true;

end:
  nr_free(metric_name);
  nr_free(segment->name);
  nr_realfree((void**) segment_ptr);

  return;
}

