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

bool newrelic_end_transaction(newrelic_txn_t** transaction_ptr) {
  newrelic_txn_t* transaction;

  if ((NULL == transaction_ptr) || (NULL == *transaction_ptr)) {
    nrl_error(NRL_INSTRUMENT, "unable to end a NULL transaction");
    return false;
  }

  transaction = *transaction_ptr;

  nrt_mutex_lock(&transaction->lock);
  {
    nrtxn_t* txn = transaction->txn;

    nr_txn_end(txn);

    nrl_verbose(NRL_INSTRUMENT,
                "sending txnname='%.64s'"
                " agent_run_id=" NR_AGENT_RUN_ID_FMT
                " nodes_used=%d"
                " duration=" NR_TIME_FMT " threshold=" NR_TIME_FMT,
                txn->name ? txn->name : "unknown", txn->agent_run_id,
                txn->nodes_used, nr_txn_duration(txn),
                txn->options.tt_threshold);

    if (0 == txn->status.ignore) {
      if (NR_FAILURE == nr_cmd_txndata_tx(nr_get_daemon_fd(), txn)) {
        nrl_error(NRL_INSTRUMENT, "failed to send transaction");
        return false;
      }
    }

    nr_txn_destroy(&txn);
  }
  nrt_mutex_unlock(&transaction->lock);

  nrt_mutex_destroy(&transaction->lock);
  nr_realfree((void**)transaction_ptr);

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

  transaction = nr_malloc(sizeof(newrelic_txn_t));
  if (NR_FAILURE == nrt_mutex_init(&transaction->lock, 0)) {
    nrl_error(NRL_INSTRUMENT, "unable to initialise transaction lock");
    nr_free(transaction);
    return NULL;
  }

  nrt_mutex_lock(&app->lock);
  {
    options = newrelic_get_transaction_options(app->config);
    transaction->txn = nr_txn_begin(app->app, options, attribute_config);
  }
  nrt_mutex_unlock(&app->lock);
  if (NULL == transaction->txn) {
    nrl_error(NRL_INSTRUMENT, "unable to start transaction");
    nr_free(transaction);
    return NULL;
  }

  if (NULL == name) {
    name = "NULL";
  }

  nr_txn_set_path(NULL, transaction->txn, name, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

  nr_attribute_config_destroy(&attribute_config);
  nr_free(options);

  if (is_web_transaction) {
    nr_txn_set_as_web_transaction(transaction->txn, 0);
    nrl_verbose(NRL_INSTRUMENT, "starting web transaction \"%s\"", name);
  } else {
    nr_txn_set_as_background_job(transaction->txn, 0);
    nrl_verbose(NRL_INSTRUMENT, "starting non-web transaction \"%s\"", name);
  }

  return transaction;
}
