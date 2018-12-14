/*
 * This file handles global variables.
 */
#include "php_agent.h"
#include "util_memory.h"

nrphpglobals_t nr_php_per_process_globals;

void nr_php_per_process_globals_dispose(void) {
  nr_free(nr_php_per_process_globals.collector);
  nr_free(nr_php_per_process_globals.proxy);
  nr_free(nr_php_per_process_globals.daemon);
  nr_free(nr_php_per_process_globals.pidfile);
  nr_free(nr_php_per_process_globals.daemon_logfile);
  nr_free(nr_php_per_process_globals.daemon_loglevel);
  nr_free(nr_php_per_process_globals.daemon_auditlog);
  nr_free(nr_php_per_process_globals.daemon_app_timeout);
  nr_free(nr_php_per_process_globals.udspath);
  nr_free(nr_php_per_process_globals.php_version);
  nr_free(nr_php_per_process_globals.upgrade_license_key);
  nro_delete(nr_php_per_process_globals.appenv);
  nr_free(nr_php_per_process_globals.apache_add);

  nr_memset(&nr_php_per_process_globals, 0, sizeof(nr_php_per_process_globals));
}
