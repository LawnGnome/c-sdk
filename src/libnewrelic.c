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

newrelic_config_t* newrelic_new_config(const char* app_name,
                                       const char* license_key) {
  newrelic_config_t* config;

  if (NULL == app_name) {
    nrl_error(NRL_INSTRUMENT, "app name is required");
    return NULL;
  }

  if (NR_LICENSE_SIZE != nr_strlen(license_key)) {
    nrl_error(NRL_INSTRUMENT, "invalid license key format");
    return NULL;
  }

  config = (newrelic_config_t*)nr_zalloc(sizeof(newrelic_config_t));

  nr_strxcpy(config->app_name, app_name, nr_strlen(app_name));
  nr_strxcpy(config->license_key, license_key, nr_strlen(license_key));

  return config;
}

newrelic_app_t* newrelic_create_app(const newrelic_config_t* given_config,
                                    unsigned short timeout_ms) {
  newrelic_app_t* app;
  nr_app_info_t* app_info;
  newrelic_config_t* config;
  nrapplist_t* context;

  /* Map newrelic_loglevel_t ENUM to char* */
  static const char* log_array[] = {"info", "debug", "error", "verbose"};

  if (NULL == given_config) {
    nrl_set_log_file("stderr");
    nrl_error(NRL_INSTRUMENT, "%s expects a non-null config", __func__);
    return NULL;
  }

  if (0 < nr_strlen(given_config->log_filename)) {
    if (NR_FAILURE == nrl_set_log_file(given_config->log_filename)) {
      nrl_set_log_file("stderr");
      nrl_error(NRL_INSTRUMENT, "unable to open log file '%s'",
                NRSAFESTR(given_config->log_filename));
    }
  }

  if (0 >= nr_strlen(given_config->app_name)) {
    nrl_error(NRL_INSTRUMENT, "app name is required");
    return NULL;
  }

  if (NR_LICENSE_SIZE != nr_strlen(given_config->license_key)) {
    nrl_error(NRL_INSTRUMENT, "invalid license key format");
    return NULL;
  }

  if (NR_FAILURE == nrl_set_log_level(log_array[given_config->log_level])) {
    return NULL;
  }

  config =
      newrelic_new_config(given_config->app_name, given_config->license_key);

  if (0 < nr_strlen(given_config->daemon_socket)) {
    nr_strxcpy(config->daemon_socket, given_config->daemon_socket,
               nr_strlen(given_config->daemon_socket));
  } else {
    nr_strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
  }

  if (0 < nr_strlen(given_config->log_filename)) {
    nr_strxcpy(config->log_filename, given_config->log_filename,
               nr_strlen(given_config->log_filename));
  }

  app_info = (nr_app_info_t*)nr_zalloc(sizeof(nr_app_info_t));

  if (0 < nr_strlen(given_config->redirect_collector)) {
    app_info->redirect_collector = nr_strdup(given_config->redirect_collector);
  } else {
    app_info->redirect_collector = nr_strdup("collector.newrelic.com");
  }

  app_info->appname = nr_strdup(given_config->app_name);
  app_info->license = nr_strdup(given_config->license_key);
  app_info->lang = nr_strdup("sdk");
  app_info->environment = nro_new_hash();
  app_info->version = nr_strdup(newrelic_version());

  app = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));
  app->app_info = app_info;
  app->config = config;

  context = newrelic_init(app->config->daemon_socket);
  if (NULL == context) {
    /* There should already be an error message printed */
    return NULL;
  }

  app->context = context;

  if (NR_FAILURE == newrelic_connect_app(app, context, timeout_ms)) {
    /* There should already be an error message printed */
    nrl_close_log_file();
    return NULL;
  }

  return app;
}

bool newrelic_destroy_app(newrelic_app_t** app) {
  if ((NULL == app) || (NULL == *app)) {
    return false;
  }

  nrl_info(NRL_INSTRUMENT, "newrelic shutting down");

  nr_agent_close_daemon_connection();
  nrl_close_log_file();

  nr_applist_destroy(&(*app)->context);
  nr_free((*app)->context);

  nr_app_info_destroy_fields((*app)->app_info);
  nr_free((*app)->app_info);

  if ((*app)->config) {
    nr_free((*app)->config);
  }

  nr_realfree((void**)app);

  return true;
}

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
