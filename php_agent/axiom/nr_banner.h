/*
 * This file contains a function to write agent/daemon info to the log file.
 */
#ifndef NR_BANNER_HDR
#define NR_BANNER_HDR

typedef enum _nr_daemon_startup_mode_t {
  NR_DAEMON_STARTUP_UNKOWN = -1, /* unknown startup mode */
  NR_DAEMON_STARTUP_INIT = 0,    /* daemon started up elsewhere */
  NR_DAEMON_STARTUP_AGENT = 1, /* daemon started up from the agent by forking */
} nr_daemon_startup_mode_t;

/*
 * Purpose : Emit a banner containing the agent version and other pertinent
 *           information, usually to the log file.
 *
 * Params  : 1. The external port for a daemon (if any)
 *           2. The listening UDS endpoint (if any)
 *           3. The daemon startup mode.
 *           4. A string containing extra information about the language or
 *              environment that axiom is supporting.  For PHP, this is the
 *              PHP version, the SAPI version, the zts flavor, and Apache
 *              information.  This string is optional, and is NULL when called
 *              by the PHP background daemon.
 *
 * Returns : Nothing.
 *
 * Notes   : One of either workers or an external port must be specified, but
 *           not both (there is no such daemon mode that uses both workers and
 *           and external daemon).
 */
extern void nr_banner(int external_port,
                      const char* udspath,
                      nr_daemon_startup_mode_t daemon_launch_mode,
                      const char* agent_specific_info);

#endif /* NR_BANNER_HDR */