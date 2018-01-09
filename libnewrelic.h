/*
 * libnewrelic.h -- Generic library to communicate with New Relic. See
 * accompanying GUIDE.md and LICENSE.txt for more information.
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
  * @brief Returns a populated agent configuration.
  *
  * Given an application name and license key, this method returns an agent
  * configuration.  Specifically, it returns a pointer to a newrelic_config_t
  * with initialized app_name and license_key fields along with default values
  * for the remaining fields. The caller should free the configuration after
  * the application has been created.
  *
  * @param [in] app_name The name of the application
  * @param [in] license_key A valid license key supplied by New Relic
  *
  * @return An agent configuration populated with app_name and license_key; all
  * other fields are initialized to their defaults
  */
newrelic_config_t* newrelic_new_config(const char* app_name,
                                       const char* license_key);

/*
 * @brief Create the application
 *
 * Given an agent configuration, newrelic_create_app() returns a pointer to the newly
 * allocated application, or NULL if there was an error. If successful, the
 * caller should destroy the application with the supplied newrelic_destroy_app()
 * when finished.
 *
 * @param [in] config An agent configuration describing the app_name, license_key,
 * as well as the behavior of the application and transactions
 * @param [in] timeout_ms Specifies the maximum time to wait for a connection to
 * be established; A value of 0 causes the method to make only one attempt at
 * connecting to the daemon
 *
 * @return A pointer to an allocated application, or NULL on error
 */
newrelic_app_t* newrelic_create_app(const newrelic_config_t* config,
                                    unsigned short timeout_ms);

/*
 * @brief Destroy the application
 *
 * Given an allocated application, newrelic_destroy_app() closes the logfile
 * handle and frees any memory used by app to describe the application.
 *
 * @param [in] app The address of the pointer to the allocated application
 *
 * @return false if app is NULL or points to NULL; true otherwise
 */
bool newrelic_destroy_app(newrelic_app_t** app);


/*
 * @brief Start a web based transaction
 *
 * Given a valid application and transaction name, this function begins timing
 * a new transaction and returns a valid pointer to a New Relic transaction
 * struct (newrelic_txn_t).  This struct may be used as an input parameter to
 * functions that modify an active transaction.
 *
 * @param [in] app A pointer to an allocation application
 * @param [in] name The name for the transaction
 *
 * @return A pointer to the transaction
 */
newrelic_txn_t* newrelic_start_web_transaction(newrelic_app_t* app,
                                               const char* name);

/*
 * @brief Start a non-web based transaction
 *
 * Given a valid application and transaction name, this function begins timing
 * a new transaction and returns a valid pointer to a New Relic transaction
 * struct (newrelic_txn_t). This struct may be used as an input parameter to
 * functions that modify an active transaction.
 *
 * @param [in] app A pointer to an allocation application
 * @param [in] name The name for the transaction
 * @return A pointer to the transaction
 */
newrelic_txn_t* newrelic_start_non_web_transaction(newrelic_app_t* app,
                                                   const char* name);

/*
 * @brief End a transaction
 *
 * Given a transaction started by newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(), this function stops the transaction's
 * timing, sends any data to the New Relic daemon, and destroys the transaction.
 *
 * @param [in] transaction The address of a pointer to a previously-started transaction
 *
 * @return false if transaction is NULL or points to NULL; false if data cannot
 * be sent to newrelic; true otherwise
 */
bool newrelic_end_transaction(newrelic_txn_t** transaction);

/*
 * @brief Add a custom integer attribute to a transaction
 *
 * Given a transaction started by newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(), this function appends an
 * integer attribute to the transaction
 *
 * @param [in] transaction A previously-started transaction
 * @param [in] name the name of the attribute
 * @param [in] value the integer value of the attribute
 *
 * @return true if successful; false otherwise
 */
bool newrelic_add_attribute_int(newrelic_txn_t* transaction,
                                const char* key,
                                const int value);

/*
 * @brief Add a custom long attribute to a transaction
 *
 * Given a transaction started by newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(), this function appends a
 * long attribute to the transaction
 *
 * @param [in] transaction A previously-started transaction
 * @param [in] name the name of the attribute
 * @param [in] value the long value of the attribute
 *
 * @return true if successful; false otherwise
 */
bool newrelic_add_attribute_long(newrelic_txn_t* transaction,
                                 const char* key,
                                 const long value);


/*
 * @brief Add a custom double attribute to a transaction
 *
 * Given a transaction started by newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(), this function appends a
 * double attribute to the transaction
 *
 * @param [in] transaction A previously-started transaction
 * @param [in] name the name of the attribute
 * @param [in] value the double value of the attribute
 *
 * @return true if successful; false otherwise
 */
bool newrelic_add_attribute_double(newrelic_txn_t* transaction,
                                   const char* key,
                                   const double value);

/*
 * @brief Add a custom string attribute to a transaction.
 *
 * Given a transaction started by newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(), this function appends a
 * string attribute to the transaction.
 *
 * @param [in] transaction A previously-started transaction
 * @param [in] name the name of the attribute
 * @param [in] value the string value of the attribute
 *
 * @return true if successful; false otherwise
 */
bool newrelic_add_attribute_string(newrelic_txn_t* transaction,
                                   const char* key,
                                   const char* value);

/*
 * @brief Record an error in a transaction
 *
 * Given a transaction started by newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(), this function records an
 * error inside of the transaction.
 *
 * @param [in] transaction A previously-started transaction
 * @param [in] priority The error's priority
 * @param [in] errmsg A string comprising the error message
 * @param [in] errclass A string comprising the error class
 */

void newrelic_notice_error(newrelic_txn_t* transaction,
                           int priority,
                           const char* errmsg,
                           const char* errclass);

#ifdef __cplusplus
}
#endif

#endif /* LIBNEWRELIC_H */
