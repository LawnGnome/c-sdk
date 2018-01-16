#include "libnewrelic.h"
#include "transaction.h"
#include "app.h"
#include "config.h"

#include "nr_agent.h"
#include "nr_attributes.h"
#include "nr_commands.h"
#include "nr_txn.h"
#include "util_logging.h"
#include "util_memory.h"

newrelic_txn_t* newrelic_start_web_transaction(newrelic_app_t* app,
                                               const char* name) {
  return newrelic_start_transaction(app, name, true);
}

newrelic_txn_t* newrelic_start_non_web_transaction(newrelic_app_t* app,
                                                   const char* name) {
  return newrelic_start_transaction(app, name, false);
}

bool newrelic_end_transaction(newrelic_txn_t** transaction) {
  if ((NULL == transaction) || (NULL == *transaction)) {
    nrl_error(NRL_INSTRUMENT, "unable to end a NULL transaction");
    return false;
  }

  nr_txn_end(*transaction);

  nrl_verbose(NRL_INSTRUMENT,
              "sending txnname='%.64s'"
              " agent_run_id=" NR_AGENT_RUN_ID_FMT
              " nodes_used=%d"
              " duration=" NR_TIME_FMT " threshold=" NR_TIME_FMT,
              (*transaction)->name ? (*transaction)->name : "unknown",
              (*transaction)->agent_run_id, (*transaction)->nodes_used,
              nr_txn_duration((*transaction)),
              (*transaction)->options.tt_threshold);

  if (0 == (*transaction)->status.ignore) {
    if (NR_FAILURE == nr_cmd_txndata_tx(nr_get_daemon_fd(), *transaction)) {
      nrl_error(NRL_INSTRUMENT, "failed to send transaction");
      return false;
    }
  }

  nr_txn_destroy(transaction);

  return true;
}

newrelic_txn_t* newrelic_start_transaction(newrelic_app_t* app,
                                           const char* name,
                                           bool is_web_transaction) {
  newrelic_txn_t* transaction = NULL;
  nrtxnopt_t* options = NULL;
  nr_attribute_config_t* attribute_config = NULL;

  if (NULL == app) {
    nrl_error(NRL_INSTRUMENT,
              "unable to start transaction with a NULL application");
    return NULL;
  }

  options = newrelic_get_default_options();
  transaction = nr_txn_begin(app->app, options, attribute_config);

  if (NULL == name) {
    name = "NULL";
  }

  nr_txn_set_path(NULL, transaction, name, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

  nr_attribute_config_destroy(&attribute_config);
  nr_free(options);

  if (is_web_transaction) {
    nr_txn_set_as_web_transaction(transaction, 0);
    nrl_verbose(NRL_INSTRUMENT, "starting web transaction \"%s\"", name);
  } else {
    nr_txn_set_as_background_job(transaction, 0);
    nrl_verbose(NRL_INSTRUMENT, "starting non-web transaction \"%s\"", name);
  }

  return transaction;
}
