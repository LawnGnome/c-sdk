/*
 * libnewrelic.h -- Generic library to communicate with New Relic. See
 * accompanying LICENSE.txt for more information.
 */
#ifndef LIBNEWRELIC_H
#define LIBNEWRELIC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _nr_app_and_info_t newrelic_app_t;
typedef struct _nrtxn_t newrelic_txn_t;
typedef enum _newrelic_loglevel_t {
  LOG_INFO,
  LOG_DEBUG,
  LOG_ERROR,
  LOG_VERBOSE
} newrelic_loglevel_t;

/*
 * Configuration
 *
 * Required fields:
 * app_name:      Sets the name of the application that metrics will be reported
 *                into.
 *
 * license_key:   Sets the New Relic license key to use.
 *
 * Optional fields:
 * redirect_collector: This refers to the New Relic provided host. There is very
 *                     little reason to ever change this from the default.
 *
 * daemon_socket: The default is to use a UNIX-domain socket located at
 *                /tmp/.newrelic.sock. If you want to use UNIX domain sockets
 *                then this value must begin with a "/". Setting this to an
 *                integer value in the range 1-65534 will instruct the daemon to
 *                listen on a normal TCP socket on the port specified. On Linux,
 *                an abstract socket can be created by prefixing the socket name
 *                with '@'.
 *
 * log_filename:  Unless you provide a log file name, no logging will occur.
 *
 * log_level:     Sets the level of detail to include in the log file. You
 *                should rarely need to change this from the default LOG_INFO,
 *                and usually only under the guidance of technical support.
 *                Must be one of the following values: LOG_ERROR, LOG_INFO,
 *                LOG_DEBUG, LOG_VERBOSE
 */
typedef struct _newrelic_config_t {
  char app_name[255];
  char license_key[255];
  char redirect_collector[100];
  char daemon_socket[512];
  char log_filename[512];
  newrelic_loglevel_t log_level;
} newrelic_config_t;

/*
 * Given an application name and license key, this method returns a config
 * struct with default values for the remaining fields. The caller should
 * destroy the config after the application has been created.
 */
newrelic_config_t* newrelic_new_config(const char* app_name,
                                       const char* license_key);

/*
 * Application
 *
 * Given a config, newrelic_create_app() returns a pointer to the newly
 * allocated application, or NULL if there was an error. The caller should
 * destroy the application with the destroy method when finished.
 *
 * Specify a timeout in milliseconds for the method to retry and establish
 * a connection to the daemon. Setting timeout_ms to zero will cause the
 * method to perform one attempt at connecting to the daemon and return NULL
 * on failure.
 */
newrelic_app_t* newrelic_create_app(const newrelic_config_t* config,
                                    unsigned short timeout_ms);
bool newrelic_destroy_app(newrelic_app_t** app);

/*
 * Transaction
 *
 * Given an application and transaction name, the start methods begin timing a
 * transaction. The end method stops timing, sends the data to the New Relic
 * daemon, and destroys the transaction.
 */
newrelic_txn_t* newrelic_start_web_transaction(newrelic_app_t* app,
                                               const char* name);
newrelic_txn_t* newrelic_start_non_web_transaction(newrelic_app_t* app,
                                                   const char* name);
bool newrelic_end_transaction(newrelic_txn_t** transaction);

/*
 * Attributes
 *
 * Given a transaction, the add_attribute methods add custom key-value pairs
 * to the transaction.
 */
bool newrelic_add_attribute_int(newrelic_txn_t* transaction,
                                const char* key,
                                const int value);
bool newrelic_add_attribute_long(newrelic_txn_t* transaction,
                                 const char* key,
                                 const long value);
bool newrelic_add_attribute_double(newrelic_txn_t* transaction,
                                   const char* key,
                                   const double value);
bool newrelic_add_attribute_string(newrelic_txn_t* transaction,
                                   const char* key,
                                   const char* value);

/*
 * Error
 *
 * Given a transaction, notice_error() will record an error inside of the
 * provided transaction.
 *
 */
void newrelic_notice_error(newrelic_txn_t* transaction,
                           int priority,
                           const char* errmsg,
                           const char* errclass);

#ifdef __cplusplus
}
#endif

#endif /* LIBNEWRELIC_H */
