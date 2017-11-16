#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_agent.h"
#include "nr_app_private.h"
#include "nr_commands.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * These mutex-unprotected global variables are
 * used to prevent spamming the logs with log messages.
 */

/*
 * Log message for when an application can not be added because the application
 * limit has been reached.
 */
time_t nr_last_log_max_apps = 0;

/*
 * Purpose : Determine if an application matches the given information.
 *
 * Params  : 1. The app in question.
 *           2. The license to match.
 *           3. The appname to match.
 *
 * Returns : NR_SUCCESS if the app is a match, and NR_FAILURE otherwise.
 * 
 * Locking : Assumes the application is locked.
 */
nr_status_t
nr_app_match (const nrapp_t *app, const nr_app_info_t *info)
{
  if ((0 == app) || (NULL == info) || (0 == info->license) || (0 == info->appname)) {
    return NR_FAILURE;
  }

  if (0 != nr_strcmp (info->license, app->info.license)) {
    return NR_FAILURE;
  }

  if (0 != nr_strcmp (info->appname, app->info.appname)) {
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

void
nr_app_info_destroy_fields (nr_app_info_t *info)
{
  if (NULL == info) {
    return;
  }

  nr_free (info->license);
  nro_delete (info->settings);
  nro_delete (info->environment);
  nro_delete (info->labels);
  nr_free (info->host_display_name);
  nr_free (info->lang);
  nr_free (info->version);
  nr_free (info->appname);
  nr_free (info->redirect_collector);
}

/*
 * Purpose : Destroy an application freeing all of its associated memory.
 *
 * Params  : 1. Pointer to nrapp_t * containing the application in question.
 *
 * Locking : Assumes the application is locked. If app_ptr points to a location
 *           within the global applist, then the applist should be locked as
 *           well.
 */
void
nr_app_destroy (nrapp_t **app_ptr)
{
  nrapp_t *app;

  if ((0 == app_ptr) || (0 == *app_ptr)) {
    return;
  }

  app = *app_ptr;

  nr_app_info_destroy_fields (&app->info);

  nr_free (app->agent_run_id);
  nr_free (app->plicense);
  nr_rules_destroy (&app->url_rules);
  nr_rules_destroy (&app->txn_rules);
  nr_segment_terms_destroy (&app->segment_terms);
  nro_delete (app->connect_reply);
  nr_random_destroy (&app->rnd);

  nrt_mutex_unlock (&app->app_lock);
  nrt_mutex_destroy (&app->app_lock);
  nr_memset (app, 0, sizeof (*app));

  nr_realfree ((void **)app_ptr);
}

nrapplist_t *
nr_applist_create (void)
{
  nr_status_t rv;
  nrapplist_t *applist = (nrapplist_t *)nr_zalloc (sizeof (nrapplist_t));

  rv = nrt_mutex_init (&applist->applist_lock, 0);
  if (NR_SUCCESS != rv) {
    return 0;
  }

  applist->num_apps = 0;
  applist->apps = (nrapp_t **)nr_calloc (NR_APP_LIMIT, sizeof (nrapp_t *));

  return applist;
}

void
nr_applist_destroy (nrapplist_t **applist_ptr)
{
  int i;
  nrapplist_t *applist;

  if (0 == applist_ptr) {
    return;
  }
  applist = *applist_ptr;
  if (0 == applist) {
    return;
  }

  nrt_mutex_lock (&applist->applist_lock); {
    if (applist->apps) {
      for (i = 0; i < NR_APP_LIMIT; i++) {
        if (applist->apps[i]) {
          nrt_mutex_lock (&applist->apps[i]->app_lock);
          nr_app_destroy (&applist->apps[i]);
          applist->apps[i] = 0;
        }
      }
      nr_free (applist->apps);
    }
  } nrt_mutex_unlock (&applist->applist_lock);

  nrt_mutex_destroy (&applist->applist_lock);
  nr_memset (applist, 0, sizeof (nrapplist_t));
  nr_realfree ((void **)applist_ptr);
}

nrapp_t *
nr_app_verify_id (nrapplist_t *applist, const char *agent_run_id)
{
  int i;
  nrapp_t *app = 0;

  if (0 == applist) {
    return 0;
  }

  if (NULL == agent_run_id) {
    return NULL;
  }

  nrt_mutex_lock (&applist->applist_lock); {
    for (i = 0; i < applist->num_apps; i++) {
      app = applist->apps[i];

      if (NULL == app) {
        continue;
      }

      nrt_mutex_lock (&app->app_lock); {
        if ((NR_APP_OK == app->state) &&
            (0 == nr_strcmp (agent_run_id, app->agent_run_id))) {
          nrt_mutex_unlock (&applist->applist_lock);
          return app;
        }
      } nrt_mutex_unlock (&app->app_lock);
    }
  } nrt_mutex_unlock (&applist->applist_lock);

  return NULL;
}

static void
log_app_limit_hard (const char *appname)
{
  time_t now = time (0);

  if ((now - nr_last_log_max_apps) > NR_LOG_BACKOFF_MAX_APPS_SECONDS) {
    nr_last_log_max_apps = now;
    nrl_error (NRL_ACCT, "Maximum number of applications (%d) reached. Unable to add app=" NRP_FMT,
      NR_APP_LIMIT, NRP_APPNAME (appname));
  }
}

char *
nr_app_create_printable_license (const char *license)
{
  int len;
  char buf[NR_LICENSE_SIZE + 1];

  if (0 == license) {
    return 0;
  }

  len = nr_strlen (license);
  if (NR_LICENSE_SIZE != len) {
    return 0;
  }

  snprintf (buf, sizeof (buf), NR_PRINTABLE_LICENSE_FMT,
            license + NR_PRINTABLE_LICENSE_PREFIX_START,
            license + NR_PRINTABLE_LICENSE_SUFFIX_START);

  return nr_strdup (buf);
}

static nrapp_t *
create_new_app (const nr_app_info_t *info)
{
  nrapp_t *app = (nrapp_t *)nr_zalloc (sizeof (nrapp_t));

  app->info.license       = nr_strdup (info->license);
  app->plicense           = nr_app_create_printable_license (info->license);
  app->state              = NR_APP_UNKNOWN;
  app->info.appname       = nr_strdup (info->appname);
  app->info.lang          = nr_strdup (info->lang);
  app->info.version       = nr_strdup (info->version);
  app->info.settings      = nro_copy (info->settings);
  app->info.environment   = nro_copy (info->environment);
  app->info.high_security = info->high_security;
  app->info.labels        = nro_copy (info->labels);
  app->info.host_display_name = nr_strdup (info->host_display_name);
  app->info.redirect_collector = nr_strdup (info->redirect_collector);

  app->rnd = nr_random_create ();
  nr_random_seed_from_time (app->rnd);

  nrt_mutex_init (&app->app_lock, 0);
  nrt_mutex_lock (&app->app_lock);

  nrl_debug (NRL_ACCT, "added app=" NRP_FMT " license=" NRP_FMT, NRP_APPNAME (app->info.appname),
    NRP_LICNAME (app->plicense));

  return app;
}

#define NR_APP_LOG_HIGH_SECURITY_MISMATCH_BACKOFF_SECONDS 20

static void
nr_app_log_high_security_mismatch (const char *appname)
{
  static int last_warn = 0;
  time_t now = time (0);

  if ((now - last_warn) > NR_APP_LOG_HIGH_SECURITY_MISMATCH_BACKOFF_SECONDS) {
    last_warn = now;
    nrl_error (NRL_DAEMON, "unable to add app=" NRP_FMT " as there already "
      "exists an app with the same name but a different high security setting.  "
      "Please ensure that all of your PHP ini files have the same "
      "newrelic.high_security value then restart your web servers and the "
      "newrelic-daemon.", 
    NRP_APPNAME (appname ? appname : "<unknown>"));
  }
}

static int
nr_app_info_valid (const nr_app_info_t *info)
{
  if (NULL == info) {
    return 0;
  }
  if (NULL == info->appname) {
    return 0;
  }
  if (NULL == info->license) {
    return 0;
  }
  if (NULL == info->environment) {
    return 0;
  }
  if (NULL == info->lang) {
    return 0;
  }
  if (NULL == info->version) {
    return 0;
  }
  if (NULL == info->redirect_collector) {
    return 0;
  }
  return 1;
}

nrapp_t *
nr_app_find_or_add_app (nrapplist_t *applist, const nr_app_info_t *info)
{
  nrapp_t *app = 0;

  if (0 == nr_app_info_valid (info)) {
    return 0;
  }
  if (0 == applist) {
    return 0;
  }

  nrt_mutex_lock (&applist->applist_lock); {
    int i;
    int num_apps = applist->num_apps;

    /*
     * Search for the application.
     */
    app = 0;
    for (i = 0; i < num_apps; i++) {
      nrapp_t *test_app = applist->apps[i];

      if (0 != test_app) {
        nrt_mutex_lock (&test_app->app_lock); {
          if (NR_SUCCESS == nr_app_match (test_app, info)) {
            /*
             * The app is returned locked.
             */
            app = test_app;
            break;
          }
        } nrt_mutex_unlock (&test_app->app_lock);
      }
    }

    if (app) {
      /*
       * A matching application was found in the loop above.
       * Check that high security is set correctly.
       * Note that it is impossible to have two applications with the same
       * name and license but different high_security values:  That would
       * be silly because the collector would reject one of the connections,
       * since the account is either set to high security or it isn't.
       */
      if (info->high_security != app->info.high_security) {
        nr_app_log_high_security_mismatch (info->appname);
        nrt_mutex_unlock (&app->app_lock);
        app = 0;
      }
    } else {
      /*
       * The app was not found and must be added if the app list is not full.
       */
      if (num_apps >= NR_APP_LIMIT) {
        log_app_limit_hard (info->appname);
      } else {
        app = create_new_app (info);
        applist->apps[num_apps] = app;
        applist->num_apps += 1;
      }
    }
  } nrt_mutex_unlock (&applist->applist_lock);

  return app;
}

/*
 * Purpose : Determine whether the agent should query the daemon about
 *           the given app.
 *
 * Params  : 1. The app in question.
 *
 * Returns : 1 if the daemon should be queried, and 0 otherwise. 
 * 
 * Locking : Assumes the application is locked.
 *
 * Notes   : These queries are minimized since excessive queries were causing
 *           performance degradation for at least one customer.
 */
int
nr_agent_should_do_app_daemon_query (const nrapp_t *app, time_t now)
{
  time_t period;

  if (0 == app) {
    return 0;
  }

  if (NR_APP_INVALID == app->state) {
    return 0;
  }

  if (NR_APP_UNKNOWN == app->state) {
    period = (1 + app->failed_daemon_query_count) * NR_APP_UNKNOWN_QUERY_BACKOFF_SECONDS;

    if (period > NR_APP_UNKNOWN_QUERY_BACKOFF_LIMIT_SECONDS) {
      period = NR_APP_UNKNOWN_QUERY_BACKOFF_LIMIT_SECONDS;
    }
  } else {
    /*
     * The daemon may be queried even if the app is known and valid: This is
     * to ensure that the agent will get the latest settings from RPM if a
     * restart occurs.
     */
    period = NR_APP_REFRESH_QUERY_PERIOD_SECONDS;
  }

  if ((now - app->last_daemon_query) > period) {
    return 1;
  }
  return 0;
}

nrapp_t *
nr_agent_find_or_add_app (
  nrapplist_t *applist,
  const nr_app_info_t *info,
  nrobj_t *(*settings_callback_fn)(void))
{
  time_t now = time (0);
  nrapp_t *app;

  if (0 == nr_app_info_valid (info)) {
    return 0;
  }

  app = nr_app_find_or_add_app (applist, info);
  if (0 == app) {
    return 0;
  }

  if ((0 == app->info.settings) && settings_callback_fn) {
    app->info.settings = settings_callback_fn ();
  }

  /*
   * Query the daemon about the state of the application, if appropriate.
   */
  if (nr_agent_should_do_app_daemon_query (app, now)) {
    app->last_daemon_query = now;
    nr_cmd_appinfo_tx (nr_get_daemon_fd (), app);
    if (NR_APP_OK == app->state) {
      app->failed_daemon_query_count = 0;
    } else {
      app->failed_daemon_query_count += 1;
    }
  }

  if (NR_APP_OK == app->state) {
    return app;
  }

  nrt_mutex_unlock (&app->app_lock);
  return 0;
}
