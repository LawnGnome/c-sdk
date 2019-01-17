/*
 * This file contains functions to manage applications.
 *
 * Every transaction reports data to one application structure. If this
 * application has multiple names (eg "app1;app2;app3") this data may be split
 * across multiple applications within RPM, but the agent and daemon are
 * oblivious to this. Applications are used in the agent and daemon. This
 * header defines both, as the same structure is used.
 */
#ifndef NR_APP_HDR
#define NR_APP_HDR

#include <stdint.h>
#include <sys/types.h>

#include "nr_app_harvest.h"
#include "nr_rules.h"
#include "nr_segment_terms.h"
#include "util_random.h"
#include "util_threads.h"

/*
 * License size and formatters to print externally visible licenses.
 */
#define NR_LICENSE_SIZE 40
#define NR_PRINTABLE_LICENSE_WINDOW_SIZE 2
#define NR_PRINTABLE_LICENSE_WINDOW \
  "%." NR_STR2(NR_PRINTABLE_LICENSE_WINDOW_SIZE) "s"
#define NR_PRINTABLE_LICENSE_FMT \
  NR_PRINTABLE_LICENSE_WINDOW "..." NR_PRINTABLE_LICENSE_WINDOW
#define NR_PRINTABLE_LICENSE_PREFIX_START 0
#define NR_PRINTABLE_LICENSE_SUFFIX_START \
  (NR_LICENSE_SIZE - NR_PRINTABLE_LICENSE_WINDOW_SIZE)

/*
 * Application Locking
 *
 * At no time should a thread hold a pointer
 * to an unlocked application. Therefore, all app pointer function parameters
 * and return values must be locked. When a thread wants to acquire a locked
 * application, it must use one of the functions below. This is to ensure
 * that no thread tries to lock an app which has been reclaimed. Threads that
 * wish to hold a reference to an unlocked application should instead hold
 * an agent_run_id.
 *
 * NOTE: This app limit should match the daemon's app limit set in limits.go.
 */
#define NR_APP_LIMIT 250

/*
 * The fields in nr_app_info_t come from local configuration.  This is the
 * information which is sent up to the collector during the connect command.
 */
typedef struct _nr_app_info_t {
  int high_security;    /* Indicates if high security been set locally for this
                           application */
  char* license;        /* License key provided */
  nrobj_t* settings;    /* New Relic settings */
  nrobj_t* environment; /* Application environment */
  nrobj_t*
      labels; /* Labels
                 (https://newrelic.atlassian.net/wiki/display/eng/Labels+for+Language+Agents)
               */
  char*
      host_display_name;         /* Optional user-provided host name for UI
                                    (https://source.datanerd.us/agents/agent-specs/blob/master/2015-04-0001-CustomHostNames.md)
                                  */
  char* lang;                    /* Language */
  char* version;                 /* Version */
  char* appname;                 /* Application name */
  char* redirect_collector;      /* Collector proxy used for redirect command */
  char* security_policies_token; /* LASP token */
  nrobj_t*
      supported_security_policies; /* list of supported security policies */
} nr_app_info_t;

typedef struct _nrapp_t {
  nr_app_info_t info;
  nr_random_t* rnd;   /* Random number generator */
  int state;          /* Connection state */
  char* plicense;     /* Printable license (abbreviated for security) */
  char* agent_run_id; /* The collector's agent run ID; assigned from RPM */
  time_t last_daemon_query; /* Used by agent: Last time we queried daemon about
                               this app */
  int failed_daemon_query_count; /* Used by agent: Number of times daemon query
                                    has not returned valid */
  nrrules_t* url_rules; /* From RPM - rules for txn path. Only used by agent. */
  nrrules_t* txn_rules; /* From RPM - rules for full txn metric name. Only used
                           by agent. */
  nr_segment_terms_t* segment_terms; /* From RPM - rules for transaction segment
                                        terms. Only used by agent. */
  nrobj_t* connect_reply;            /* From RPM - Full connect command reply */
  nrobj_t* security_policies;        /* from Daemon - full security policies map
                                        obtained from Preconnect */
  nrthread_mutex_t app_lock;         /* Serialization lock */
  nr_app_harvest_t harvest;          /* Harvest timing and sampling data */
} nrapp_t;

typedef enum _nrapptype_t {
  NR_APP_INVALID = -1, /* The app has an invalid license key */
  NR_APP_UNKNOWN = 0,  /* The app has not yet been connected to RPM */
  NR_APP_OK = 1        /* The app is connected and valid */
} nrapptype_t;

typedef struct _nrapplist_t {
  int num_apps;
  nrapp_t** apps;
  nrthread_mutex_t applist_lock;
} nrapplist_t;

/*
 * Purpose : Create a new application list.
 *
 * Returns : A pointer to an unlocked newly allocated app list on success, and
 *           NULL on failure.
 */
extern nrapplist_t* nr_applist_create(void);

/*
 * Purpose : Destroy of the global application list, destroying all of
 *           the applications stored within.
 */
extern void nr_applist_destroy(nrapplist_t** applist_ptr);

/*
 * Purpose : Determine if the given agent run ID refers to a valid
 *           application.
 *
 * Params  : 1. The list of applications unlocked.
 *           2. The agent run ID.
 *
 * Returns : A locked application on success and 0 otherwise.
 *
 * Locking : Returns the application locked, if one is returned.
 *
 * Note    : For this function to return an application, two conditions must be
 *           met:  The agent_run_id must be valid and refer to an application
 *           AND that application must be valid (connected).
 */
extern nrapp_t* nr_app_verify_id(nrapplist_t* applist,
                                 const char* agent_run_id);

/*
 * Purpose : Search for an application within the agent. If the application
 *           does not yet exist within the account, add it, and query the
 *           daemon, which in turn will either return the known application
 *           information (if the daemon previously knew about the application)
 *           or return unknown, and connect the application with RPM.
 *
 * Params  : 1. The application list unlocked.
 *           2. The application information.
 *           3. An agent-specific callback function whose purpose it is to
 *              provide the settings hash upon app creation.
 *
 * Returns : A pointer to the locked valid application if it, or NULL if the
 *           application is unknown or invalid, or if there was any form of
 *           error.
 *
 * Notes   : This function is called by the agent when a transaction starts and
 *           the desired application is known. It may ultimately result in an
 *           APPINFO command being sent to the daemon, which in turn will
 *           either return the known application info from its cache or begin
 *           the process of querying the application from RPM.
 */
extern nrapp_t* nr_agent_find_or_add_app(
    nrapplist_t* applist,
    const nr_app_info_t* info,
    nrobj_t* (*settings_callback_fn)(void));

/*
 * Purpose : Create and return a sanitized/obfuscated version of the license
 *           for use in the phpinfo and log files.
 *
 * Params  : 1. The raw license.
 *
 * Returns : A newly allocated string containing the printable license.
 */
extern char* nr_app_create_printable_license(const char* license);

/*
 * Purpose : Free all app info structure fields.
 */
extern void nr_app_info_destroy_fields(nr_app_info_t* info);

#endif /* NR_APP_HDR */