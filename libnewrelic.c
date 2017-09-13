#include "libnewrelic.h"

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

typedef struct _nr_app_and_info_t {
  nrapp_t *app;
  nr_app_info_t *app_info;
  newrelic_config_t *config;
  nrapplist_t *context;
} nr_app_and_info_t;

newrelic_config_t *
newrelic_new_config (const char *app_name, const char *license_key)
{
  newrelic_config_t *config;

  if (NULL == app_name) {
    nrl_error (NRL_INSTRUMENT, "app name is required");
    return NULL;
  }

  if (NR_LICENSE_SIZE != nr_strlen (license_key)) {
    nrl_error (NRL_INSTRUMENT, "invalid license key format");
    return NULL;
  }

  config = (newrelic_config_t *) nr_zalloc (sizeof (newrelic_config_t));

  nr_strxcpy (config->app_name, app_name, nr_strlen (app_name));
  nr_strxcpy (config->license_key, license_key, nr_strlen (license_key));

  // TODO Check env vars

  return config;
}

/*
 * This helper function returns a set of default options. Having this as
 * a separate function will help in the future when the option variables are
 * moved into the newrelic_config_t struct and they need to be extracted/copied
 * into the options struct. The helper function returns a populated options
 * struct ready to be passed to Axiom.
 */
static nrtxnopt_t *
newrelic_get_options (void)
{
  nrtxnopt_t *opt = nr_zalloc (sizeof (nrtxnopt_t));

  /* Set default options */
  opt->analytics_events_enabled         = true; // Enable transaction events
  opt->custom_events_enabled            = true;
  opt->synthetics_enabled               = false;
  opt->instance_reporting_enabled       = false;
  opt->database_name_reporting_enabled  = false;
  opt->err_enabled                      = false;
  opt->request_params_enabled           = false;
  opt->autorum_enabled                  = false;
  opt->error_events_enabled             = false;
  opt->tt_enabled                       = false;
  opt->ep_enabled                       = false;
  opt->tt_recordsql                     = false;
  opt->tt_slowsql                       = false;
  opt->apdex_t                          = 0;    // Set by application
  opt->tt_threshold                     = 0;
  opt->ep_threshold                     = 0;
  opt->ss_threshold                     = 0;
  opt->cross_process_enabled            = false;
  opt->tt_is_apdex_f                    = 0;

  return opt;
}

static nrapplist_t *
newrelic_init (const char *daemon_socket)
{
  nrapplist_t *context = nr_applist_create ();

  if (NULL == context) {
    nrl_error (NRL_INSTRUMENT, "failed to initialize newrelic");
    return NULL;
  }

  if (NR_FAILURE == nr_agent_initialize_daemon_connection_parameters (daemon_socket, 0)) {
    nrl_error (NRL_INSTRUMENT, "failed to initialize daemon connection");
    return NULL;
  }

  if (!nr_agent_try_daemon_connect (10)) {
    nrl_error (NRL_INSTRUMENT, "failed to connect to the daemon");
    return NULL;
  }

  nrl_info (NRL_INSTRUMENT, "newrelic initialized");

  return context;
}

static nr_status_t
newrelic_connect_app (newrelic_app_t *app, nrapplist_t *context, unsigned short timeout_ms)
{
  nrapp_t *nrapp;
  const int retry_sleep_ms = 50;
  nrtime_t start_time;
  nrtime_t delta_time;
  nrtime_t timeout_time = timeout_ms * NR_TIME_DIVISOR_MS;

  if ((NULL == app) || (NULL == context)) {
    nrl_error (NRL_INSTRUMENT, "invalid application or context");
    return NR_FAILURE;
  }

  /* Setting this global is necessary for transaction naming to work. */
  nr_agent_applist = context;

  /* Query the daemon until a successful connection is made or timeout occurs.*/
  start_time = nr_get_time_monotonic ();
  while (true) {
    nrapp = nr_agent_find_or_add_app (context, app->app_info, NULL);

    delta_time = nr_time_duration (start_time, nr_get_time_monotonic ());
    if (NULL != nrapp || (delta_time >= timeout_time)) {
      break;
    }

    nr_msleep (retry_sleep_ms);
  };

  if (NULL == nrapp) {
    nrl_error (NRL_INSTRUMENT, "application was unable to connect");
    return NR_FAILURE;
  }

  app->app = nrapp;
  nrt_mutex_unlock (&app->app->app_lock);
  nrl_info (NRL_INSTRUMENT, "application %s connected", NRSAFESTR (app->app_info->appname));

  return NR_SUCCESS;
}

newrelic_app_t *
newrelic_create_app (const newrelic_config_t *given_config, unsigned short timeout_ms)
{
  newrelic_app_t *app;
  nr_app_info_t *app_info;
  newrelic_config_t *config;
  nrapplist_t *context;

  /* Map newrelic_loglevel_t ENUM to char* */
  static const char* log_array[] = {"info", "debug", "error", "verbose"};

  if (0 < nr_strlen (given_config->log_filename)) {
    if (NR_FAILURE == nrl_set_log_file (given_config->log_filename)) {
      nrl_set_log_file ("stderr");
      nrl_error (NRL_INSTRUMENT, "unable to open log file '%s'", NRSAFESTR (given_config->log_filename));
    }
  }

  /* Sanity checking */
  if (0 >= nr_strlen (given_config->app_name)) {
    nrl_error (NRL_INSTRUMENT, "app name is required");
    return NULL;
  }

  if (NR_LICENSE_SIZE != nr_strlen (given_config->license_key)) {
    nrl_error (NRL_INSTRUMENT, "invalid license key format");
    return NULL;
  }

  if (NR_FAILURE == nrl_set_log_level (log_array[given_config->log_level])) {
    return NULL;
  }

  /* Assemble config */
  config = newrelic_new_config (given_config->app_name, given_config->license_key);

  if (0 < nr_strlen (given_config->daemon_socket)) {
    nr_strxcpy (config->daemon_socket, given_config->daemon_socket, nr_strlen (given_config->daemon_socket));
  } else {
    nr_strcpy (config->daemon_socket, "/tmp/.newrelic.sock");
  }

  if (0 < nr_strlen (given_config->log_filename)) {
    nr_strxcpy (config->log_filename, given_config->log_filename, nr_strlen (given_config->log_filename));
  }

  /* Assemble app info */
  app_info = (nr_app_info_t *) nr_zalloc (sizeof (nr_app_info_t));

  if (0 < nr_strlen (given_config->redirect_collector)) {
    app_info->redirect_collector = nr_strdup (given_config->redirect_collector);
  } else {
    app_info->redirect_collector = nr_strdup ("collector.newrelic.com");
  }

  app_info->appname     = nr_strdup (given_config->app_name);
  app_info->license     = nr_strdup (given_config->license_key);
  app_info->lang        = nr_strdup ("sdk");
  app_info->environment = nro_new_hash ();
  app_info->version     = nr_strdup ("0.1");

  /* Assemble app */
  app = (newrelic_app_t *) nr_zalloc (sizeof (newrelic_app_t));
  app->app_info = app_info;
  app->config = config;


  context = newrelic_init (app->config->daemon_socket);
  if (NULL == context) {
    /* There should already be an error msg printed */
    return NULL;
  }

  app->context = context;

  if (NR_FAILURE == newrelic_connect_app (app, context, timeout_ms)) {
    /* There should already be an error msg printed */
    nrl_close_log_file ();
    return NULL;
  }

  return app;
}

void
newrelic_destroy_app (newrelic_app_t **app)
{
  if ((NULL == app) || (NULL == *app)) {
    return;
  }

  nrl_info (NRL_INSTRUMENT, "newrelic shutting down");

  nr_agent_close_daemon_connection ();
  nrl_close_log_file ();

  nr_applist_destroy (&(*app)->context);
  nr_free ((*app)->context);

  nr_app_info_destroy_fields ((*app)->app_info);
  nr_free ((*app)->app_info);

  if ((*app)->config) {
    nr_free ((*app)->config);
  }

  nr_realfree ((void **) app);

  return;
}

static newrelic_txn_t *
newrelic_start_transaction (newrelic_app_t *app, const char *name, bool is_web_transaction)
{
  newrelic_txn_t *transaction = NULL;
  nrtxnopt_t *options = NULL;
  nr_attribute_config_t *attribute_config = NULL;

  if (NULL == app) {
    nrl_error (NRL_INSTRUMENT, "application is NULL");
    return NULL;
  }

  options = newrelic_get_options ();
  transaction = nr_txn_begin (app->app, options, attribute_config);

  if (NULL == name) {
    name = "NULL Transaction Name";
  }

  nr_txn_set_path (NULL, transaction, name, NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);

  nr_attribute_config_destroy (&attribute_config);
  nr_free (options);

  if (is_web_transaction) {
    nr_txn_set_as_web_transaction (transaction, 0);
    nrl_verbose (NRL_INSTRUMENT, "starting web transaction \"%s\"", name);
  } else {
    nr_txn_set_as_background_job (transaction, 0);
    nrl_verbose (NRL_INSTRUMENT, "starting non-web transaction \"%s\"", name);
  }

  return transaction;
}

newrelic_txn_t *
newrelic_start_web_transaction (newrelic_app_t *app, const char *name)
{
  return newrelic_start_transaction (app, name, true);
}

newrelic_txn_t *
newrelic_start_non_web_transaction (newrelic_app_t *app, const char *name)
{
  return newrelic_start_transaction (app, name, false);
}

void
newrelic_end_transaction (newrelic_txn_t **transaction)
{
  if ((NULL == transaction) || (NULL == *transaction)) {
    nrl_error (NRL_INSTRUMENT, "unable to end a NULL transaction");
    return;
  }

  nr_txn_end (*transaction);

  nrl_verbose (NRL_INSTRUMENT,
    "sending txnname='%.64s'"
    " agent_run_id=" NR_AGENT_RUN_ID_FMT
    " nodes_used=%d"
    " duration=" NR_TIME_FMT
    " threshold=" NR_TIME_FMT,
    (*transaction)->name ? (*transaction)->name : "unknown",
    (*transaction)->agent_run_id,
    (*transaction)->nodes_used,
    nr_txn_duration ((*transaction)),
    (*transaction)->options.tt_threshold);

  if (0 == (*transaction)->status.ignore) {
    if (NR_FAILURE == nr_cmd_txndata_tx (nr_get_daemon_fd (), *transaction)) {
      nrl_error (NRL_INSTRUMENT, "failed to send transaction");
    }
  }

  nr_txn_destroy (transaction);

  return;
}

bool
newrelic_transaction_add_attribute_int (newrelic_txn_t *transaction, const char *name, const int value)
{
  nr_status_t rv;
  nrobj_t *obj;

  if (NULL == name) {
    nrl_error (NRL_INSTRUMENT, "failed to add integer attribute with NULL name");
    return false;
  }

  obj = nro_new_int (value);
  rv = nr_txn_add_user_custom_parameter (transaction, name, obj);
  nro_delete (obj);

  if (NR_FAILURE == rv) {
    nrl_error (NRL_INSTRUMENT, "failed to add integer attribute name=\"%s\" value=\"%d\"", name, value);
    return false;
  }

  return true;
}

bool
newrelic_transaction_add_attribute_long (newrelic_txn_t *transaction, const char *name, const long value)
{
  nr_status_t rv;
  nrobj_t *obj;

  if (NULL == name) {
    nrl_error (NRL_INSTRUMENT, "failed to add long attribute with NULL name");
    return false;
  }

  obj = nro_new_long (value);
  rv = nr_txn_add_user_custom_parameter (transaction, name, obj);
  nro_delete (obj);

  if (NR_FAILURE == rv) {
    nrl_error (NRL_INSTRUMENT, "failed to add long attribute name=\"%s\" value=\"%ld\"", name, value);
    return false;
  }

  return true;
}

bool
newrelic_transaction_add_attribute_double (newrelic_txn_t *transaction, const char *name, const double value)
{
  nr_status_t rv;
  nrobj_t *obj;

  if (NULL == name) {
    nrl_error (NRL_INSTRUMENT, "failed to add double attribute with NULL name");
    return false;
  }

  obj = nro_new_double (value);
  rv = nr_txn_add_user_custom_parameter (transaction, name, obj);
  nro_delete (obj);

  if (NR_FAILURE == rv) {
    nrl_error (NRL_INSTRUMENT, "failed to add double attribute name=\"%s\" value=\"%f\"", name, value);
    return false;
  }

  return true;
}

bool
newrelic_transaction_add_attribute_string (newrelic_txn_t *transaction, const char *name, const char *value)
{
  nr_status_t rv;
  nrobj_t *obj;

  if (NULL == name || NULL == value) {
    nrl_error (NRL_INSTRUMENT, "failed to add string attribute with NULL name or value");
    return false;
  }

  obj = nro_new_string (value);
  rv = nr_txn_add_user_custom_parameter (transaction, name, obj);
  nro_delete (obj);

  if (NR_FAILURE == rv) {
    nrl_error (NRL_INSTRUMENT, "failed to add string attribute name=\"%s\" value=\"%s\"", name, value);
    return false;
  }

  return true;
}

