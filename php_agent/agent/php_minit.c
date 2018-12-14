/*
 * This file handles the initialization that happens once per module load.
 */
#include "php_agent.h"

#include <dlfcn.h>
#include <signal.h>

#include "php_environment.h"
#include "php_error.h"
#include "php_extension.h"
#include "php_header.h"
#include "php_hooks.h"
#include "php_internal_instrument.h"
#include "php_samplers.h"
#include "php_user_instrument.h"
#include "php_vm.h"
#include "php_wrapper.h"
#include "fw_laravel.h"
#include "lib_guzzle4.h"
#include "lib_guzzle6.h"
#include "nr_agent.h"
#include "nr_app.h"
#include "nr_banner.h"
#include "nr_daemon_spawn.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_signals.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_threads.h"

#include "fw_laravel.h"
#include "lib_guzzle4.h"

static void php_newrelic_init_globals(zend_newrelic_globals* nrg) {
  if (nrunlikely(0 == nrg)) {
    return;
  }

  nr_memset(nrg, 0, sizeof(*nrg));
  nrg->enabled.value = 1;
  nrg->enabled.where = PHP_INI_STAGE_STARTUP;
  nrg->current_framework = NR_FW_UNSET;
}

/*
 * Zend uses newrelic_globals as the auto-generated name for the per-request
 * globals and then uses the same name to pass the per-request globals
 * as a parameter to the GINIT and GSHUTDOWN functions.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

/*
 * Initialize per-request globals.
 */
#ifdef TAGS
void zm_globals_ctor_newrelic(
    zend_newrelic_globals* newrelic_globals); /* ctags landing pad only */
#endif
PHP_GINIT_FUNCTION(newrelic) {
  NR_UNUSED_TSRMLS;

  php_newrelic_init_globals(newrelic_globals);
}

/*
 * Clean-up per-request globals.
 */
#ifdef TAGS
void zm_globals_dtor_newrelic(
    zend_newrelic_globals* newrelic_globals); /* ctags landing pad only */
#endif
PHP_GSHUTDOWN_FUNCTION(newrelic) {
  NR_UNUSED_TSRMLS;

  /*
   * Note that this is allocated the first time RINIT is called, rather than in
   * the more obvious GINIT function. nr_php_extension_instrument_dtor can cope
   * with an uninitialised extensions structure.
   */
  nr_php_extension_instrument_destroy(&newrelic_globals->extensions);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/*
 * Returns : a nr_daemon_startup_mode_t describing the daemon startup mode.
 *
 * Consults configuration settings and file system markers to decide what to do.
 * See the documentation in:
 *   https://docs.newrelic.com/docs/agents/php-agent/installation/starting-php-daemon
 *   https://docs.newrelic.com/docs/agents/php-agent/configuration/php-agent-configuration
 */
static nr_daemon_startup_mode_t nr_php_get_daemon_startup_mode(void) {
  /*
   * Never launch a daemon if there exists a manual configuration file.
   * If the file /etc/newrelic/newrelic.cfg exists, the agent will never
   * start the daemon as it assumes the daemon is meant to be started by
   * its startup script, not via the agent. This setting has no meaning
   * to, and does not appear in newrelic.cfg.
   */
  if (0 == nr_access("/etc/newrelic/newrelic.cfg", F_OK)) {
    return NR_DAEMON_STARTUP_INIT;
  }

  if (3 == NR_PHP_PROCESS_GLOBALS(no_daemon_launch)) {
    /*
     * The agent will never start the daemon.
     * Use this if you are configuring the daemon via newrelic.cfg and starting
     * it outside of the agent.
     */
    return NR_DAEMON_STARTUP_INIT;
  }

  if (NR_PHP_PROCESS_GLOBALS(cli)) {
    /*
     * If command line version of PHP was used, the agent will not start the
     * daemon.
     */
    if (1 == NR_PHP_PROCESS_GLOBALS(no_daemon_launch)) {
      return NR_DAEMON_STARTUP_INIT;
    }
  } else {
    /*
     * If non-command line version of PHP was used (for example Apache or
     * php-fpm) then the agent will not start the daemon (only the command line
     * usage will start the daemon).
     */
    if (2 == NR_PHP_PROCESS_GLOBALS(no_daemon_launch)) {
      return NR_DAEMON_STARTUP_INIT;
    }
  }

  return NR_DAEMON_STARTUP_AGENT;
}

/*
 * Returns : NR_FAILURE if it is a threaded MPM, and NR_SUCCESS otherwise.
 */
static nr_status_t nr_php_check_for_threaded_mpm(TSRMLS_D) {
  if ((0 != NR_PHP_PROCESS_GLOBALS(is_apache))
      && (0 != NR_PHP_PROCESS_GLOBALS(apache_threaded))) {
    NR_PHP_PROCESS_GLOBALS(mpm_bad) = 1;
    php_error_docref(
        NULL TSRMLS_CC, E_WARNING,
        "You attempted to load the New Relic module, but you appear to be "
        "using a "
        "threaded Apache MPM (--with-mpm=worker/event). This MPM is not "
        "supported by PHP or New Relic, as it has known stability issues.");
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

#define NR_PHP_UPGRADE_LICENSE_KEY_FILE "/etc/newrelic/upgrade_please.key"

static char* nr_php_check_for_upgrade_license_key(void) {
  FILE* keyfile = fopen(NR_PHP_UPGRADE_LICENSE_KEY_FILE, "r");

  if (keyfile) {
    char tmpstr[NR_LICENSE_SIZE + 16];
    size_t br = fread(tmpstr, 1, NR_LICENSE_SIZE, keyfile);

    fclose(keyfile);
    tmpstr[NR_LICENSE_SIZE] = 0;
    if (NR_LICENSE_SIZE == br) {
      return nr_strdup(tmpstr);
    }
  }

  return 0;
}

static char* nr_php_get_agent_specific_info(void) {
  const char* php_version;
  const char* zend_type;
  char web_server_info[512];
  char buf[512];

  web_server_info[0] = '\0';
  if (NR_PHP_PROCESS_GLOBALS(is_apache)) {
    snprintf(web_server_info, sizeof(web_server_info),
             "apache='%d.%d.%d%s' mpm=%s", NR_PHP_PROCESS_GLOBALS(apache_major),
             NR_PHP_PROCESS_GLOBALS(apache_minor),
             NR_PHP_PROCESS_GLOBALS(apache_patch),
             NR_PHP_PROCESS_GLOBALS(apache_add),
             (1 == NR_PHP_PROCESS_GLOBALS(apache_threaded)) ? "threaded"
                                                            : "prefork");
  }

  if (NR_PHP_PROCESS_GLOBALS(php_version)
      && (0 != NR_PHP_PROCESS_GLOBALS(php_version)[0])) {
    php_version = NR_PHP_PROCESS_GLOBALS(php_version);
  } else {
    php_version = "unknown";
  }

#ifdef ZTS
  zend_type = "yes";
#else
  zend_type = "no";
#endif

  buf[0] = '\0';
  snprintf(buf, sizeof(buf), " php='%s' zts=%s sapi='%s' %s", php_version,
           zend_type, sapi_module.name, web_server_info);

  return nr_strdup(buf);
}

/*
 * IMPORTANT: lifted directly out of Apache's httpd.h
 */
typedef struct nr_ap_version {
  int major;              /**< major number */
  int minor;              /**< minor number */
  int patch;              /**< patch number */
  const char* add_string; /**< additional string like "-dev" */
} nr_ap_version_t;

static void nr_php_populate_apache_process_globals(void) {
  void* handle;
  void (*mpmptr)(int, int*);
  void (*verptr)(nr_ap_version_t*);
  nr_ap_version_t av;
  int is_threaded = 0;

  handle = dlopen(0, RTLD_LAZY | RTLD_GLOBAL);

  if (0 == handle) {
    return;
  }

  mpmptr = (void (*)(int, int*))dlsym(handle, "ap_mpm_query");
  if (0 == mpmptr) {
    mpmptr = (void (*)(int, int*))dlsym(handle, "_ap_mpm_query");
  }

  verptr = (void (*)(nr_ap_version_t*))dlsym(handle, "ap_get_server_revision");
  if (0 == verptr) {
    verptr
        = (void (*)(nr_ap_version_t*))dlsym(handle, "_ap_get_server_revision");
  }

  if ((0 == mpmptr) || (0 == verptr)) {
    return;
  }

  is_threaded = 0;
  mpmptr(2 /*AP_MPMQ_IS_THREADED*/, &is_threaded);

  nr_memset(&av, 0, sizeof(av));
  verptr(&av);

  dlclose(handle);

  if (av.major) {
    NR_PHP_PROCESS_GLOBALS(is_apache) = 1;
    NR_PHP_PROCESS_GLOBALS(apache_major) = av.major;
    NR_PHP_PROCESS_GLOBALS(apache_minor) = av.minor;
    NR_PHP_PROCESS_GLOBALS(apache_patch) = av.patch;
    NR_PHP_PROCESS_GLOBALS(apache_add) = nr_strdup(av.add_string);
    NR_PHP_PROCESS_GLOBALS(apache_threaded) = is_threaded ? 1 : 0;
  }
}

static char* nr_php_get_php_version_number(TSRMLS_D) {
  char* version = 0;
  zval* php_ver = nr_php_get_constant("PHP_VERSION" TSRMLS_CC);

  if (0 == php_ver) {
    return 0;
  }

  if (nr_php_is_zval_non_empty_string(php_ver)) {
    version = nr_strndup(Z_STRVAL_P(php_ver), Z_STRLEN_P(php_ver));
  }

  nr_php_zval_free(&php_ver);

  return version;
}

#ifdef TAGS
void zm_startup_newrelic(void); /* ctags landing pad only */
#endif
PHP_MINIT_FUNCTION(newrelic) {
  nr_status_t ret;
  int port = 0;
  char* udspath = 0;
  nr_daemon_startup_mode_t daemon_startup_mode;
  int daemon_connect_succeeded;

  zend_extension dummy;

  (void)type;

  nr_memset(&nr_php_per_process_globals, 0, sizeof(nr_php_per_process_globals));
  NR_PHP_PROCESS_GLOBALS(enabled) = 1;
  NR_PHP_PROCESS_GLOBALS(our_module_number) = module_number;
  NR_PHP_PROCESS_GLOBALS(use_https) = 1;
  NR_PHP_PROCESS_GLOBALS(php_version) = nr_php_get_php_version_number(TSRMLS_C);
  NR_PHP_PROCESS_GLOBALS(upgrade_license_key)
      = nr_php_check_for_upgrade_license_key();
  NR_PHP_PROCESS_GLOBALS(high_security) = 0;
  nr_php_populate_apache_process_globals();
  /*
   * The CLI SAPI reports its name as cli. The CLI Web server reports its name
   * as cli-server.
   */
  if (0 == nr_strcmp(sapi_module.name, "cli")) {
    NR_PHP_PROCESS_GLOBALS(cli) = 1;
  }

  /*
   * As of 01Jan2014, we don't even try to support Apache threaded mpm.
   * If we detect that we're running in that environment, just disable the
   * agent. There are no overrides.
   */
  if (NR_SUCCESS != nr_php_check_for_threaded_mpm(TSRMLS_C)) {
    /*
     * Here we return SUCCESS, despite the lack of it.  The global 'enabled'
     * flag prevents future execution by this module.
     *
     * TODO(willhf) We should explore the ramifications of returning failure
     * from this function.
     *
     * See zend_startup_module_ex and zend_startup_modules within zend_API.c
     * This is tricky code:
     * Note that the return values of zend_startup_module_ex do not match those
     * expected by zend_hash_apply.
     */
    NR_PHP_PROCESS_GLOBALS(enabled) = 0;
    return SUCCESS;
  }

  /*
   * The internal function wrap records are created prior to reading the ini
   * entries so that they can be properly disabled by:
   *   newrelic.special.disable_instrumentation
   */
  nr_php_generate_internal_wrap_records();

  nr_php_register_ini_entries(module_number TSRMLS_CC);

  if (0 == NR_PHP_PROCESS_GLOBALS(enabled)) {
  disbad:
    nrl_info(NRL_INIT, "New Relic PHP Agent globally disabled");
    NR_PHP_PROCESS_GLOBALS(enabled) = 0;
    nrl_close_log_file();
    return SUCCESS;
  }

  port = NR_PHP_PROCESS_GLOBALS(port);
  udspath = NR_PHP_PROCESS_GLOBALS(udspath);

  ret = nr_agent_initialize_daemon_connection_parameters(udspath, port);
  if (NR_FAILURE == ret) {
    nrl_warning(NRL_INIT, "daemon connection initialization failed");
    goto disbad;
  }

  daemon_startup_mode = nr_php_get_daemon_startup_mode();

  {
    char* agent_specific_info = nr_php_get_agent_specific_info();

    nr_banner(port, udspath, daemon_startup_mode, agent_specific_info);
    nr_free(agent_specific_info);
  }

  if (0 == nr_php_use_license(0 TSRMLS_CC)) {
    nrl_warning(
        NRL_INIT,
        "A global default license has not been set or has invalid format. "
        "Please add a 'newrelic.license' key in the global php.ini or "
        "in the newrelic.ini file, or ensure that a valid license is provided "
        "on a "
        "per-virtual host or per-directory basis.");
  }

    /*
     * Attempt to connect to the daemon here.  Note that we do this no matter
     * the startup mode.  This delay allows CLI processes enough time to
     * connect. Since they handle a single request, they cannot wait through a
     * request for the connection to finish.
     */
#define NR_PHP_MINIT_DAEMON_CONNECTION_TIMEOUT_MS 10
  daemon_connect_succeeded
      = nr_agent_try_daemon_connect(NR_PHP_MINIT_DAEMON_CONNECTION_TIMEOUT_MS);

  if (0 == daemon_connect_succeeded) {
    if (NR_DAEMON_STARTUP_AGENT == daemon_startup_mode) {
      nr_daemon_args_t daemon_args;
      pid_t daemon_pid = -1;

      nr_memset(&daemon_args, 0, sizeof(daemon_args));

      daemon_args.proxy = NR_PHP_PROCESS_GLOBALS(proxy);
      daemon_args.sockfile = NR_PHP_PROCESS_GLOBALS(udspath);
      daemon_args.tcp_port = NR_PHP_PROCESS_GLOBALS(port);

      if (0 == NR_PHP_PROCESS_GLOBALS(use_https)) {
        nrl_warning(NRL_INIT,
                    "deprecated newrelic.daemon.ssl setting ignored.  Daemon "
                    "always connects via https/tls.");
      }

      daemon_args.tls_cafile = NR_PHP_PROCESS_GLOBALS(ssl_cafile);
      daemon_args.tls_capath = NR_PHP_PROCESS_GLOBALS(ssl_capath);

#define NR_PHP_DAEMON_PIDFILE "newrelic-daemon.pid"
      daemon_args.pidfile = NR_PHP_PROCESS_GLOBALS(pidfile);
      if (0 == nr_php_ini_setting_is_set_by_user("newrelic.daemon.pidfile")) {
        if (0 == nr_access("/var/run", W_OK)) {
          daemon_args.pidfile = "/var/run/" NR_PHP_DAEMON_PIDFILE;
        } else if (0 == nr_access("/var/pid", W_OK)) {
          daemon_args.pidfile = "/var/pid/" NR_PHP_DAEMON_PIDFILE;
        } else if (0 == nr_access("/var/log/newrelic", W_OK)) {
          daemon_args.pidfile = "/var/log/newrelic/" NR_PHP_DAEMON_PIDFILE;
        } else if (0 == nr_access("/var/log", W_OK)) {
          daemon_args.pidfile = "/var/log/" NR_PHP_DAEMON_PIDFILE;
        } else {
          nrl_warning(NRL_INIT,
                      "unable to find suitable pidfile location, using none");
          daemon_args.pidfile = NULL;
        }
      }

      daemon_args.logfile = NR_PHP_PROCESS_GLOBALS(daemon_logfile);
      daemon_args.loglevel = NR_PHP_PROCESS_GLOBALS(daemon_loglevel);
      daemon_args.auditlog = NR_PHP_PROCESS_GLOBALS(daemon_auditlog);
      daemon_args.app_timeout = NR_PHP_PROCESS_GLOBALS(daemon_app_timeout);
      daemon_args.integration_mode
          = NR_PHP_PROCESS_GLOBALS(daemon_special_integration);
      daemon_args.debug_http
          = NR_PHP_PROCESS_GLOBALS(daemon_special_curl_verbose);
      daemon_args.utilization = NR_PHP_PROCESS_GLOBALS(utilization);

      daemon_pid
          = nr_spawn_daemon(NR_PHP_PROCESS_GLOBALS(daemon), &daemon_args);

      if (-1 == daemon_pid) {
        goto disbad;
      }
    } else {
      nrl_warning(NRL_DAEMON,
                  "failed to connect to the newrelic-daemon.  The agent "
                  "expects a daemon "
                  "to be started externally. "
                  "Please refer to: "
                  "https://newrelic.com/docs/php/"
                  "newrelic-daemon-startup-modes#daemon-external");
    }
  }

  /*
   * If this is a web server master process (eg Apache mod_php), it may
   * fork worker processes.  In order to prevent sharing of the daemon
   * connection fd, we want to close the connection before the fork.
   * If the process is not going to fork (eg CLI), then closing the connection
   * would necessitate another connect (which is quite costly using TCP).
   *
   * Previously, an atfork handler was registered in order to close
   * the connection only if a fork occurred.  However, this was problematic
   * on FreeBSD and OS X for Apache graceful restarts, presumably because the
   * atfork handler function pointer referenced extension code which could be
   * removed by Apache.
   */
  if (0 == NR_PHP_PROCESS_GLOBALS(cli)) {
    nr_agent_close_daemon_connection();
  }

  /*
   * Save the original PHP hooks and then apply our own hooks. The agent is
   * almost fully operational now. The last remaining initialization that
   * takes place (see the function below) is called on the very first call
   * to RINIT. The reason this is done is that we want to do some work once
   * ALL extensions have been loaded. Here during the MINIT phase there may
   * still be many other extensions to come and some, like XDEBUG, are not very
   * well behaved citizens and we need to ensure certain initialization tasks
   * are run only once the PHP VM engine is ticking over fully.
   */

  NR_PHP_PROCESS_GLOBALS(orig_execute) = NR_ZEND_EXECUTE_HOOK;
  NR_ZEND_EXECUTE_HOOK = nr_php_execute;

  if (NR_PHP_PROCESS_GLOBALS(instrument_internal)) {
    nrl_info(
        NRL_AGENT,
        "enabling internal function instrumentation (this might be slow!)");

    /*
     * We use execute_internal as a fallback as that's what PHP does
     * internally: it's entirely normal for zend_execute_internal to be NULL,
     * in which case it's implied that execute_internal will be the internal
     * executor used.
     */
    NR_PHP_PROCESS_GLOBALS(orig_execute_internal)
        = zend_execute_internal ? zend_execute_internal : execute_internal;
    zend_execute_internal = nr_php_execute_internal;
  }

  /*
   * Save the SAPI module header handler so we can use our own wrapper.
   */
  NR_PHP_PROCESS_GLOBALS(orig_header_handler) = sapi_module.header_handler;
  sapi_module.header_handler = nr_php_header_handler;

#define NR_INFO_SPECIAL_FLAGS(field)                  \
  if (NR_PHP_PROCESS_GLOBALS(special_flags).field) {  \
    nrl_info(NRL_INIT, "special_flags." #field "=1"); \
  }
  NR_INFO_SPECIAL_FLAGS(no_sql_parsing);
  NR_INFO_SPECIAL_FLAGS(show_sql_parsing);
  NR_INFO_SPECIAL_FLAGS(enable_path_translated);
  NR_INFO_SPECIAL_FLAGS(no_background_jobs);
  NR_INFO_SPECIAL_FLAGS(show_executes);
  NR_INFO_SPECIAL_FLAGS(show_execute_params);
  NR_INFO_SPECIAL_FLAGS(show_execute_stack);
  NR_INFO_SPECIAL_FLAGS(show_execute_returns);
  NR_INFO_SPECIAL_FLAGS(show_executes_untrimmed);
  NR_INFO_SPECIAL_FLAGS(no_signal_handler);
  NR_INFO_SPECIAL_FLAGS(debug_autorum);
  NR_INFO_SPECIAL_FLAGS(show_loaded_files);
  NR_INFO_SPECIAL_FLAGS(debug_cat);
#undef NR_INFO_SPECIAL_FLAGS

  nr_guzzle4_minit(TSRMLS_C);
  nr_guzzle6_minit(TSRMLS_C);
  nr_laravel_minit(TSRMLS_C);
  nr_php_set_opcode_handlers();

  nrl_debug(NRL_INIT, "MINIT processing done");
  NR_PHP_PROCESS_GLOBALS(zend_offset) = zend_get_resource_handle(&dummy);

  return (SUCCESS);
}

static void nr_php_fatal_signal_handler(int sig) {
  int fd;

  TSRMLS_FETCH();

  fd = nrl_get_log_fd();
  if (fd >= 0) {
    nr_signal_tracer_common(sig);  // TODO: nr_backtrace_fd (fd);
    nr_write(fd, NR_PSTR("PHP execution trace follows...\n"));
    nr_php_backtrace_fd(fd, -1 /* unlimited */ TSRMLS_CC);
  }

  /*
   * Reraise the signal with the default signal handler so that the OS can dump
   * core or perform any other configured action.
   */
  nr_signal_reraise(sig);
}

nr_status_t nr_php_late_initialization(TSRMLS_D) {
  nrl_debug(NRL_INIT, "late_init called from pid=%d", nr_getpid());

  /*
   * The applist should be created here (after the web server forks),
   * so that the applist mutexes do not need to be re-initialized.
   */
  nr_agent_applist = nr_applist_create();
  if (0 == nr_agent_applist) {
    nrl_error(NRL_INIT, "unable to initialize applist structure");
  }

  /*
   * We have learned that the popular Xdebug extension does not "play well
   * with others" with regards to its replacement of the error handler.
   * Since they actually to want to replace it, not simply trap it, this is
   * reasonable behavior. However, it makes it difficult for us to trap and
   * forward the errors, so if a user has Xdebug loaded, we do not install
   * our own error callback handler. Otherwise, we do.
   */
  if (0 == zend_get_extension("Xdebug")) {
    NR_PHP_PROCESS_GLOBALS(orig_error_cb) = zend_error_cb;
    zend_error_cb = nr_php_error_cb;
  } else {
    nrl_warning(NRL_INIT,
                "the Xdebug extension prevents the New Relic agent from "
                "gathering errors. No errors will be recorded.");
  }

  /*
   * Install our signal handler, unless the user has set a special flag telling
   * us not to.
   */
  if (0 == (NR_PHP_PROCESS_GLOBALS(special_flags).no_signal_handler)) {
    nr_signal_handler_install(nr_php_fatal_signal_handler);
  }

  NR_PHP_PROCESS_GLOBALS(appenv) = nr_php_get_environment(TSRMLS_C);

  NR_PHP_PROCESS_GLOBALS(done_instrumentation) = 1;
  nr_php_add_internal_instrumentation(TSRMLS_C);

  nr_php_initialize_samplers();

  return NR_SUCCESS;
}
