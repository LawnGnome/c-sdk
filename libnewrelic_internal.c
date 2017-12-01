#include "libnewrelic.h"
#include "libnewrelic_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "nr_agent.h"
#include "nr_app.h"
#include "nr_attributes.h"
#include "nr_axiom.h"
#include "nr_commands.h"
#include "nr_txn.h"
#include "util_object.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_sleep.h"
#include "version.h"

bool newrelic_add_attribute(newrelic_txn_t* transaction,
                            const char* key,
                            nrobj_t* obj) {
  if (NULL == transaction) {
    nrl_error(NRL_INSTRUMENT, "unable to add attribute for a NULL transaction");
    return false;
  }

  if (NULL == key) {
    nrl_error(NRL_INSTRUMENT, "unable to add attribute with a NULL key");
    return false;
  }

  if (NR_FAILURE == nr_txn_add_user_custom_parameter(transaction, key, obj)) {
    nrl_error(NRL_INSTRUMENT, "unable to add attribute for key=\"%s\"", key);
    return false;
  }

  return true;
}

nrtxnopt_t* newrelic_get_default_options(void) {
  nrtxnopt_t* opt = nr_zalloc(sizeof(nrtxnopt_t));

  opt->analytics_events_enabled = true;
  opt->custom_events_enabled = false;
  opt->synthetics_enabled = false;
  opt->instance_reporting_enabled = false;
  opt->database_name_reporting_enabled = false;
  opt->err_enabled = false;
  opt->request_params_enabled = false;
  opt->autorum_enabled = false;
  opt->error_events_enabled = false;
  opt->tt_enabled = false;
  opt->ep_enabled = false;
  opt->tt_recordsql = false;
  opt->tt_slowsql = false;
  opt->apdex_t = 0;
  opt->tt_threshold = 0;
  opt->ep_threshold = 0;
  opt->ss_threshold = 0;
  opt->cross_process_enabled = false;
  opt->tt_is_apdex_f = 0;

  return opt;
}

nrapplist_t* newrelic_init(const char* daemon_socket) {
  nrapplist_t* context = nr_applist_create();

  if (NULL == context) {
    nrl_error(NRL_INSTRUMENT, "failed to initialize newrelic");
    return NULL;
  }

  if (NR_FAILURE ==
      nr_agent_initialize_daemon_connection_parameters(daemon_socket, 0)) {
    nrl_error(NRL_INSTRUMENT, "failed to initialize daemon connection");
    return NULL;
  }

  if (!nr_agent_try_daemon_connect(10)) {
    nrl_error(NRL_INSTRUMENT, "failed to connect to the daemon");
    return NULL;
  }

  nrl_info(NRL_INSTRUMENT, "newrelic initialized");

  return context;
}

bool newrelic_cmp_options(nrtxnopt_t* o1, nrtxnopt_t* o2) {
  if (o1->analytics_events_enabled != o2->analytics_events_enabled)
    return false;
  if (o1->custom_events_enabled != o2->custom_events_enabled)
    return false;
  if (o1->synthetics_enabled != o2->synthetics_enabled)
    return false;
  if (o1->instance_reporting_enabled != o2->instance_reporting_enabled)
    return false;
  if (o1->database_name_reporting_enabled !=
      o2->database_name_reporting_enabled)
    return false;
  if (o1->err_enabled != o2->err_enabled)
    return false;
  if (o1->request_params_enabled != o2->request_params_enabled)
    return false;
  if (o1->autorum_enabled != o2->autorum_enabled)
    return false;
  if (o1->error_events_enabled != o2->error_events_enabled)
    return false;
  if (o1->tt_enabled != o2->tt_enabled)
    return false;
  if (o1->ep_enabled != o2->ep_enabled)
    return false;
  if (o1->tt_recordsql != o2->tt_recordsql)
    return false;
  if (o1->tt_slowsql != o2->tt_slowsql)
    return false;
  if (o1->apdex_t != o2->apdex_t)
    return false;
  if (o1->tt_threshold != o2->tt_threshold)
    return false;
  if (o1->ep_threshold != o2->ep_threshold)
    return false;
  if (o1->ss_threshold != o2->ss_threshold)
    return false;
  if (o1->cross_process_enabled != o2->cross_process_enabled)
    return false;
  if (o1->tt_is_apdex_f != o2->tt_is_apdex_f)
    return false;

  return true;
}

nr_status_t newrelic_connect_app(newrelic_app_t* app,
                                 nrapplist_t* context,
                                 unsigned short timeout_ms) {
  nrapp_t* nrapp;
  const int retry_sleep_ms = 50;
  nrtime_t start_time;
  nrtime_t delta_time;
  nrtime_t timeout_time = timeout_ms * NR_TIME_DIVISOR_MS;

  if ((NULL == app) || (NULL == context)) {
    nrl_error(NRL_INSTRUMENT, "invalid application or context");
    return NR_FAILURE;
  }

  /* Setting this global is necessary for transaction naming to work. */
  nr_agent_applist = context;

  /* Query the daemon until a successful connection is made or timeout occurs.*/
  start_time = nr_get_time();
  while (true) {
    nrapp = nr_agent_find_or_add_app(context, app->app_info, NULL);

    delta_time = nr_time_duration(start_time, nr_get_time());
    if (NULL != nrapp || (delta_time >= timeout_time)) {
      break;
    }

    nr_msleep(retry_sleep_ms);
  };

  if (NULL == nrapp) {
    nrl_error(NRL_INSTRUMENT, "application was unable to connect");
    return NR_FAILURE;
  }

  app->app = nrapp;
  nrt_mutex_unlock(&app->app->app_lock);
  nrl_info(NRL_INSTRUMENT, "application %s connected",
           NRSAFESTR(app->app_info->appname));

  return NR_SUCCESS;
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
