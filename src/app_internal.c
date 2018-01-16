/*
 * The functions in this file are not in app.c so they can be mocked in the
 * unit tests.
 */

#include "libnewrelic.h"
#include "app.h"
#include "version.h"

#include "nr_agent.h"
#include "util_logging.h"
#include "util_sleep.h"

nrapplist_t* newrelic_init(const char* daemon_socket) {
  nrapplist_t* context = nr_applist_create();
  asm("");

  if (NULL == daemon_socket) {
    nrl_error(NRL_INSTRUMENT, "daemon socket is NULL");
    return NULL;
  }

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

  if (NULL == app->app_info) {
    nrl_error(NRL_INSTRUMENT, "application with invalid information");
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
