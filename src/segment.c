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

newrelic_segment_t* newrelic_start_segment(newrelic_txn_t* transaction,
                                           const char* name,
                                           const char* category) {
  char* metric_name = NULL;
  newrelic_segment_t* segment = NULL;
  nr_segment_t* txn_segment = NULL;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "unable to start segment with NULL transaction");
    return NULL;
  }

  /* Create the axiom segment. */
  txn_segment = nr_segment_start(transaction, NULL, NULL);
  if (NULL == txn_segment) {
    return NULL;
  }

  if (!name || !newrelic_validate_segment_param(name, "segment name")) {
    name = "Unnamed Segment";
  }

  if (!category
      || !newrelic_validate_segment_param(category, "segment category")) {
    category = "Custom";
  }

  metric_name = nr_formatf("%s/%s", category, name);
  nr_segment_set_name(txn_segment, metric_name);
  nr_free(metric_name);

  /* Now create the wrapper type. */
  segment = nr_malloc(sizeof(newrelic_segment_t));
  segment->segment = txn_segment;
  segment->transaction = transaction;

  /* Set up the fields so that we can correctly track child segment duration. */
  segment->kids_duration_save = transaction->cur_kids_duration;
  transaction->cur_kids_duration = &segment->kids_duration;

  return segment;
}

bool newrelic_end_segment(newrelic_txn_t* transaction,
                          newrelic_segment_t** segment_ptr) {
  nrtime_t duration;
  nrtime_t exclusive;
  newrelic_segment_t* segment;
  bool status = false;

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
    nrl_error(NRL_INSTRUMENT,
              "cannot end a segment on a different transaction to the one it "
              "was created on");
    goto end;
  }

  /* Stop the segment. */
  nr_segment_end(segment->segment);

  /* Calculate exclusive time and restore the previous child duration field. */
  duration = nr_time_duration(segment->segment->start_time,
                              segment->segment->stop_time);
  exclusive = duration - segment->kids_duration;
  transaction->cur_kids_duration = segment->kids_duration_save;
  nr_txn_adjust_exclusive_time(transaction, duration);

  /* Add a custom metric. */
  nrm_add_ex(transaction->scoped_metrics,
             nr_string_get(transaction->trace_strings, segment->segment->name),
             duration, exclusive);

  status = true;

end:
  nr_realfree((void**)segment_ptr);

  return status;
}
