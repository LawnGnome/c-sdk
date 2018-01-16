#include "libnewrelic.h"
#include "libnewrelic_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "nr_agent.h"
#include "nr_app.h"
#include "nr_axiom.h"
#include "nr_commands.h"
#include "nr_txn.h"
#include "util_object.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_sleep.h"
#include "version.h"

void newrelic_notice_error(newrelic_txn_t* transaction,
                           int priority,
                           const char* errmsg,
                           const char* errclass) {
  char* stacktrace_json;

  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "unable to add error to NULL transaction");
    return;
  }

  if (NULL == errmsg || 0 == errmsg[0]) {
    nrl_error(NRL_INSTRUMENT,
              "unable to add NULL/empty error message to transaction");
    return;
  }

  if (NULL == errclass || 0 == errclass[0]) {
    nrl_error(NRL_INSTRUMENT,
              "unable to add NULL/empty error class to transaction");
    return;
  }

  if (0 == transaction->options.err_enabled) {
    nrl_error(NRL_INSTRUMENT,
              "unable to add error to transaction when errors are disabled");
    return;
  }

  if (0 == transaction->status.recording) {
    nrl_error(NRL_INSTRUMENT,
              "unable to add error to transaction that is not recording");
    return;
  }

  if (0 != transaction->error &&
      priority < nr_error_priority(transaction->error)) {
    nrl_error(NRL_INSTRUMENT,
              "an error with a higher priority already exists on transaction");
    return;
  }

  if (NR_FAILURE == nr_txn_record_error_worthy(transaction, priority)) {
    nrl_error(NRL_INSTRUMENT, "unable to add error to transaction");
    return;
  }

  stacktrace_json = newrelic_get_stack_trace_as_json();
  nr_txn_record_error(transaction, priority, errmsg, errclass, stacktrace_json);
  nr_free(stacktrace_json);
}
