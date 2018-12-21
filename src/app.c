#include "libnewrelic.h"
#include "app.h"
#include "version.h"

#include "nr_agent.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

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

  config
      = newrelic_new_config(given_config->app_name, given_config->license_key);

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

  config->transaction_tracer = given_config->transaction_tracer;

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

  nrt_mutex_lock(&(*app)->lock);
  {
    nr_agent_close_daemon_connection();
    nrl_close_log_file();

    nr_applist_destroy(&(*app)->context);
    nr_free((*app)->context);

    nr_app_info_destroy_fields((*app)->app_info);
    nr_free((*app)->app_info);

    if ((*app)->config) {
      nr_free((*app)->config);
    }
  }
  nrt_mutex_unlock(&(*app)->lock);

  nrt_mutex_destroy(&(*app)->lock);

  nr_realfree((void**)app);

  return true;
}
