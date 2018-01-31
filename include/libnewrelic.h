/*!
 * @file libnewrelic.h
 *
 * @brief Generic library to communicate with New Relic. See
 * accompanying GUIDE.md and LICENSE.txt for more information.
 */
#ifndef LIBNEWRELIC_H
#define LIBNEWRELIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Application.
 *
 * @see newrelic_create_app().
 */
typedef struct _nr_app_and_info_t newrelic_app_t;

/*!
 * @brief Transaction. A transaction is started using
 * newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(). A started, or active, transaction is
 * stopped using newrelic_end_transaction(). One may modify an active
 * transaction by adding custom attributes or recording errors to a
 * transaction only after it has been started.
 */
typedef struct _nrtxn_t newrelic_txn_t;

/*!
 * @brief Log levels.  An enumeration of the possible log levels for an agent
 * configuration, or newrelic_config_t.
 *
 * @see struct _newrelic_config_t
 */
typedef enum _newrelic_loglevel_t {
  LOG_INFO,
  LOG_DEBUG,
  LOG_ERROR,
  LOG_VERBOSE
} newrelic_loglevel_t;

/*!
 * @brief Record SQL settings.
 *
 * @see struct _newrelic_transaction_tracer_config_t
 */
typedef enum _newrelic_tt_recordsql_t {
  NEWRELIC_SQL_OFF,
  NEWRELIC_SQL_RAW,
  NEWRELIC_SQL_OBFUSCATED
} newrelic_tt_recordsql_t;

/*! @brief Agent configuration used to configure the behaviour of the
 * transaction tracer.
 */
typedef enum _newrelic_transaction_tracer_threshold_t {
  /*! Use 4*apdex(T) as the minimum time a transaction must take before it is
   *  eligible for a transaction trace.
   */
  NEWRELIC_THRESHOLD_IS_APDEX_FAILING,

  /*! Use the value given in the duration_us field as the minimum time a
   *  transaction must take before it is eligible for a transaction trace.
   */
  NEWRELIC_THRESHOLD_IS_OVER_DURATION,
} newrelic_transaction_tracer_threshold_t;

/*! @brief Agent configuration used to configure transaction tracing. */
typedef struct _newrelic_transaction_tracer_config_t {
  /*! Whether to enable transaction traces. */
  bool enabled;

  /*! Whether to consider transactions for trace generation based on the apdex
   *  configuration or a specific duration.
   */
  newrelic_transaction_tracer_threshold_t threshold;

  /*! If the agent configuration threshold is set to
   *  NEWRELIC_THRESHOLD_IS_OVER_DURATION, this field specifies the minimum
   *  transaction time before a trace may be generated, in microseconds.
   */
  uint64_t duration_us;

  /*! Controls the format of the sql put into transaction traces.
   *
   *  - If set to NEWRELIC_SQL_OFF, transaction traces have no sql in them.
   *  - If set to NEWRELIC_SQL_RAW, the sql is added to the transaction
   *    trace as-is.
   *  - If set to NEWRELIC_SQL_OBFUSCATED, alphanumeric characters are set
   *    to '?'. For example 'SELECT * FROM table WHERE foo = 42' is reported
   *    as 'SELECT * FROM table WHERE foo = ?'.  These obfuscated queries are
   *    added to the transaction trace for supported datastore products.
   *
   *  New Relic highly discourages the use of the NEWRELIC_SQL_RAW setting
   *  in production environments.
   */
  newrelic_tt_recordsql_t record_sql;
} newrelic_transaction_tracer_config_t;

/*!
 * @brief Agent configuration used to configure how datastore segments
 * are recorded in a transaction.
 */
typedef struct _newrelic_datastore_segment_config_t {
  /* If set to true for a transaction, instance names are reported to New Relic.
   * More specifically, the host and port_path_or_id fields in a
   * newrelic_datastore_segment_params_t passed to
   * newrelic_datastore_start_segment() is reported when the
   * corresponding transaction is reported. */
  bool instance_reporting;

  /* If set to true for a transaction, database names are reported to New Relic.
   * More specifically, the database_name field in a
   * newrelic_datastore_segment_params_t passed to
   * newrelic_datastore_start_segment() is reported when the
   * corresponding transaction is reported. */
  bool database_name_reporting;

} newrelic_datastore_segment_config_t;

/*!
 * @brief Agent configuration used to describe application name, license key, as
 * well as daemon, logging, transaction tracer and datastore configuration.
 *
 * @see newrelic_new_config().
 */
typedef struct _newrelic_config_t {
  /*! Specifies the name of the application to which data shall be reported.
   */
  char app_name[255];

  /*! Specifies the New Relic license key to use.
   */
  char license_key[255];

  /*! Optional.  Specifies the New Relic provided host. There is little reason
   *  to ever change this from the default.
   */
  char redirect_collector[100];

  /*! Optional.  Specifies the underlying communication mechanism for the
   *  agent daemon.   The default is to use a UNIX-domain socket located at
   *  /tmp/.newrelic.sock. If you want to use UNIX domain sockets then this
   *  value must begin with a "/". Setting this to an integer value in the
   *  range 1-65534 will instruct the daemon to listen on a normal TCP socket
   *  on the port specified. On Linux, an abstract socket can be created by
   *  prefixing the socket name with '@'.
   */
  char daemon_socket[512];

  /*! Optional. Specifies the file to be used for agent logs.  If no filename
   *  is provided, no logging shall occur.
   */
  char log_filename[512];

  /*! Optional. Specifies the logfile's level of detail. There is little reason
   * to change this from the default value except under the guidance of
   * technical support.
   *
   * Must be one of the following values: LOG_ERROR, LOG_INFO (default),
   * LOG_DEBUG, LOG_VERBOSE.
   */
  newrelic_loglevel_t log_level;

  /*! Optional. The transaction tracer configuration. By default, the
   *  configuration returned by newrelic_new_config() will enable transaction
   *  traces, with the threshold set to NEWRELIC_THRESHOLD_IS_APDEX_FAILING.
   */
  newrelic_transaction_tracer_config_t transaction_tracer;

  /*! Optional. The datastore tracer configuration.  By default, the
   *  configuration returned by newrelic_new_config() will enable datastore
   *  segments with instance_reporting and database_name_reporting set
   *  to true.
   */
  newrelic_datastore_segment_config_t datastore_tracer;

} newrelic_config_t;

/*!
 * @brief Create a populated agent configuration.
 *
 * Given an application name and license key, this method returns an agent
 * configuration. Specifically, it returns a pointer to a newrelic_config_t
 * with initialized app_name and license_key fields along with default values
 * for the remaining fields. The caller should free the agent configuration
 * after the application has been created.
 *
 * @param [in] app_name The name of the application.
 * @param [in] license_key A valid license key supplied by New Relic.
 *
 * @return An agent configuration populated with app_name and license_key; all
 * other fields are initialized to their defaults.
 */
newrelic_config_t* newrelic_new_config(const char* app_name,
                                       const char* license_key);

/*!
 * @brief Create an application.
 *
 * Given an agent configuration, newrelic_create_app() returns a pointer to the
 * newly allocated application, or NULL if there was an error. If successful,
 * the caller should destroy the application with the supplied
 * newrelic_destroy_app() when finished.
 *
 * @param [in] config An agent configuration created by newrelic_create_app().
 * @param [in] timeout_ms Specifies the maximum time to wait for a connection to
 * be established; a value of 0 causes the method to make only one attempt at
 * connecting to the daemon.
 *
 * @return A pointer to an allocated application, or NULL on error; any errors
 * resulting from a badly-formed agent configuration are logged.
 */
newrelic_app_t* newrelic_create_app(const newrelic_config_t* config,
                                    unsigned short timeout_ms);

/*!
 * @brief Destroy the application.
 *
 * Given an allocated application, newrelic_destroy_app() closes the logfile
 * handle and frees any memory used by app to describe the application.
 *
 * @param [in] app The address of the pointer to the allocated application.
 *
 * @return false if app is NULL or points to NULL; true otherwise.
 */
bool newrelic_destroy_app(newrelic_app_t** app);

/*!
 * @brief Start a web based transaction.
 *
 * Given an application pointer and transaction name, this function begins
 * timing a new transaction. It returns a valid pointer to an active New Relic
 * transaction, newrelic_txn_t.  The return value of this function may be
 * used as an input parameter to functions that modify an active transaction.
 *
 * @param [in] app A pointer to an allocation application.
 * @param [in] name The name for the transaction.
 *
 * @return A pointer to the transaction.
 */
newrelic_txn_t* newrelic_start_web_transaction(newrelic_app_t* app,
                                               const char* name);

/*!
 * @brief Start a non-web based transaction.
 *
 * Given a valid application and transaction name, this function begins timing
 * a new transaction and returns a valid pointer to a New Relic transaction,
 * newrelic_txn_t. The return value of this function may be used as an input
 * parameter to functions that modify an active transaction.
 *
 * @param [in] app A pointer to an allocation application.
 * @param [in] name The name for the transaction.
 *
 * @return A pointer to the transaction.
 */
newrelic_txn_t* newrelic_start_non_web_transaction(newrelic_app_t* app,
                                                   const char* name);

/*!
 * @brief End a transaction.
 *
 * Given an active transaction, this function stops the transaction's
 * timing, sends any data to the New Relic daemon, and destroys the transaction.
 *
 * @param [in] transaction The address of a pointer to an active transaction.
 *
 * @return false if transaction is NULL or points to NULL; false if data cannot
 * be sent to newrelic; true otherwise.
 */
bool newrelic_end_transaction(newrelic_txn_t** transaction);

/*!
 * @brief Add a custom integer attribute to a transaction.
 *
 * Given an active transaction, this function appends an
 * integer attribute to the transaction.
 *
 * @param [in] transaction An active transaction.
 * @param [in] key The name of the attribute.
 * @param [in] value The integer value of the attribute.
 *
 * @return true if successful; false otherwise.
 */
bool newrelic_add_attribute_int(newrelic_txn_t* transaction,
                                const char* key,
                                const int value);

/*!
 * @brief Add a custom long attribute to a transaction.
 *
 * Given an active transaction, this function appends a
 * long attribute to the transaction.
 *
 * @param [in] transaction An active transaction.
 * @param [in] key The name of the attribute.
 * @param [in] value The long value of the attribute.
 *
 * @return true if successful; false otherwise.
 */
bool newrelic_add_attribute_long(newrelic_txn_t* transaction,
                                 const char* key,
                                 const long value);

/*!
 * @brief Add a custom double attribute to a transaction.
 *
 * Given an active transaction, this function appends a
 * double attribute to the transaction.
 *
 * @param [in] transaction An active transaction.
 * @param [in] key The name of the attribute.
 * @param [in] value The double value of the attribute.
 *
 * @return true if successful; false otherwise.
 */
bool newrelic_add_attribute_double(newrelic_txn_t* transaction,
                                   const char* key,
                                   const double value);

/*!
 * @brief Add a custom string attribute to a transaction.
 *
 * Given an active transaction, this function appends a
 * string attribute to the transaction.
 *
 * @param [in] transaction An active transaction.
 * @param [in] key The name of the attribute.
 * @param [in] value The string value of the attribute.
 *
 * @return true if successful; false otherwise.
 */
bool newrelic_add_attribute_string(newrelic_txn_t* transaction,
                                   const char* key,
                                   const char* value);

/*!
 * @brief Record an error in a transaction.
 *
 * Given an active transaction, this function records an error
 * inside of the transaction.
 *
 * @param [in] transaction An active transaction.
 * @param [in] priority The error's priority.
 * @param [in] errmsg A string comprising the error message.
 * @param [in] errclass A string comprising the error class.
 */

void newrelic_notice_error(newrelic_txn_t* transaction,
                           int priority,
                           const char* errmsg,
                           const char* errclass);

#ifdef __cplusplus
}
#endif

#endif /* LIBNEWRELIC_H */
