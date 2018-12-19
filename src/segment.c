#include "libnewrelic.h"
#include "segment.h"
#include "transaction.h"

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
  newrelic_segment_t* segment = NULL;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "unable to start segment with NULL transaction");
    return NULL;
  }

  if (!name || !newrelic_validate_segment_param(name, "segment name")) {
    name = "Unnamed Segment";
  }

  if (!category
      || !newrelic_validate_segment_param(category, "segment category")) {
    category = "Custom";
  }

  /* Now create the wrapper type. */
  segment = nr_malloc(sizeof(newrelic_segment_t));
  segment->kids_duration = 0;
  segment->transaction = transaction->txn;

  /* Set up the fields so that we can correctly track child segment duration. */
  nrt_mutex_lock(&transaction->lock);
  {
    char* metric_name;

    /* Create the axiom segment. */
    segment->segment = nr_segment_start(transaction->txn, NULL, NULL);
    if (NULL == segment->segment) {
      nrt_mutex_unlock(&transaction->lock);
      nr_free(segment);

      return NULL;
    }

    segment->kids_duration_save = transaction->txn->cur_kids_duration;
    transaction->txn->cur_kids_duration = &segment->kids_duration;

    /* Set the segment name. */
    metric_name = nr_formatf("%s/%s", category, name);
    nr_segment_set_name(segment->segment, metric_name);
    nr_free(metric_name);
  }
  nrt_mutex_unlock(&transaction->lock);

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

  if (NULL == segment->transaction || NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "unable to end a segment of a NULL transaction");
    goto end;
  }

  /* Sanity check that the segment is being ended on the same transaction it
   * was started on. Transitioning a segment between transactions would be
   * problematic, since times are transaction-specific.
   * */
  if (transaction->txn != segment->transaction) {
    nrl_error(NRL_INSTRUMENT,
              "cannot end a segment on a different transaction to the one it "
              "was created on");
    goto end;
  }

  nrt_mutex_lock(&transaction->lock);
  {
    /* Stop the segment. */
    nr_segment_end(segment->segment);

    /* Calculate exclusive time and restore the previous child duration field.
     */
    duration = nr_time_duration(segment->segment->start_time,
                                segment->segment->stop_time);
    exclusive = duration - segment->kids_duration;
    transaction->txn->cur_kids_duration = segment->kids_duration_save;
    nr_txn_adjust_exclusive_time(transaction->txn, duration);

    /* Add a custom metric. */
    nrm_add_ex(
        transaction->txn->scoped_metrics,
        nr_string_get(transaction->txn->trace_strings, segment->segment->name),
        duration, exclusive);
  }
  nrt_mutex_unlock(&transaction->lock);

  status = true;

end:
  nr_realfree((void**)segment_ptr);

  return status;
}
