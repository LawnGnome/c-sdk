/*
 * This file contains a list of commands the child can send the daemon.
 */
#ifndef NR_COMMANDS_HDR
#define NR_COMMANDS_HDR

#include "nr_app.h"
#include "nr_txn.h"

/*
 * Purpose : Given a partially populated application structure (only the back
 *           pointer to the containing license, the application names and the
 *           calculated application ID need be present) query the daemon for
 *           full details about the application. If the daemon already knows
 *           about this application it will respond with the information
 *           immediately, otherwise the daemon will wake the connector thread
 *           in order to query RPM about the app.
 *
 * Params  : 1. Daemon file descriptor to send cmd to.
 *           2. The partially populated application.
 *
 * Returns : NR_SUCCESS or NR_FAILURE. Note that even if the daemon was not
 *           able to populate the structure on account of it not knowing about
 *           it yet, this function will still return NR_SUCCESS. If the app
 *           was successfully queried, app_>state will be NR_APP_OK.
 *
 * Locking : The application must be locked prior to calling this function
 *           and will remain locked on exit.
 */
extern nr_status_t nr_cmd_appinfo_tx(int daemon_fd, nrapp_t* app);

/*
 * Purpose : Given a transaction that is complete, send it to the daemon. All
 *           metrics that are not synthesised in the daemon must be present,
 *           and all references to metric scopes must have been resolved. This
 *           data is sent by the agent as the very last thing before declaring
 *           a transaction as "done". This does not necessarily have to happen
 *           at the logical end of a transaction - it can be forced to end by
 *           an API call, for example.
 *
 * Params  : 1. Daemon file descriptor to send cmd to.
 *           2. The transaction to send.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 *
 * Locking : A transaction by definition cannot have locking contention issues
 *           as only one thread in an agent can be dealing with a transaction
 *           at a time. Therefore, the transaction structure has no locking.
 */
extern nr_status_t nr_cmd_txndata_tx(int daemon_fd, const nrtxn_t* txn);

/* Hook for stubbing APPINFO messages during testing. */
extern nr_status_t (*nr_cmd_appinfo_hook)(int daemon_fd, nrapp_t* app);

/* Hook for stubbing TXNDATA messages during testing. */
extern nr_status_t (*nr_cmd_txndata_hook)(int daemon_fd, const nrtxn_t* txn);

extern uint64_t nr_cmd_appinfo_timeout_us;

#endif /* NR_COMMANDS_HDR */