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

/*! @brief Agent configuration used to indicate the datastore product being
 * instrumented.  Used to populate the product field of a
 * struct _newrelic_datastore_segment_params_t.  Datastore product names are
 * used across New Relic agents and the following constants assist users in
 * maintaining consistent product names. */

/* The following product constants represent the SQL-like datastores that
 * this agent supports.  When the struct _newrelic_tt_recordsql_t setting of the
 * agent is set to NEWRELIC_SQL_RAW or NEWRELIC_SQL_OBFUSCATED, the query
 * param of the struct _newrelic_datastore_segment_config_t is reported to
 * New Relic. */
#define NEWRELIC_DATASTORE_FIREBIRD "Firebird"
#define NEWRELIC_DATASTORE_INFORMIX "Informix"
#define NEWRELIC_DATASTORE_MSSQL "MSSQL"
#define NEWRELIC_DATASTORE_MYSQL "MySQL"
#define NEWRELIC_DATASTORE_ORACLE "Oracle"
#define NEWRELIC_DATASTORE_POSTGRES "Postgres"
#define NEWRELIC_DATASTORE_SQLITE "SQLite"
#define NEWRELIC_DATASTORE_SYBASE "Sybase"

/* The following product constants represent the datastores that are not
 * SQL-like. As such, the query param of the
 * struct _newrelic_datastore_segment_config_t is not reported to
 * New Relic. */
#define NEWRELIC_DATASTORE_MEMCACHE "Memcached"
#define NEWRELIC_DATASTORE_MONGODB "MongoDB"
#define NEWRELIC_DATASTORE_ODBC "ODBC"
#define NEWRELIC_DATASTORE_REDIS "Redis"
#define NEWRELIC_DATASTORE_OTHER "Other"

/*!
 * @brief Application.
 *
 * @see newrelic_create_app().
 */
typedef struct _nr_app_and_info_t newrelic_app_t;

/*! @brief The internal type used to represent a datastore segment. */
typedef struct _newrelic_datastore_segment_t newrelic_datastore_segment_t;

/*!
 * @brief Transaction. A transaction is started using
 * newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(). A started, or active, transaction is
 * stopped using newrelic_end_transaction(). One may modify a transaction
 * by adding custom attributes or recording errors only after it has been
 * started.
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
  /*! Whether to enable transaction traces.
   *  Default: true.
   */
  bool enabled;

  /*! Whether to consider transactions for trace generation based on the apdex
   *  configuration or a specific duration.
   *  Default: NEWRELIC_THRESHOLD_IS_APDEX_FAILING.
   */
  newrelic_transaction_tracer_threshold_t threshold;

  /*! If the agent configuration threshold is set to
   *  NEWRELIC_THRESHOLD_IS_OVER_DURATION, this field specifies the minimum
   *  transaction time before a trace may be generated, in microseconds.
   *  Default: 0.
   */
  uint64_t duration_us;

  /*! Sets the threshold above which the New Relic agent will record a
   *  stack trace for a transaction trace, in microseconds.
   *  Default: 500000, or 0.5 seconds.
   */
  uint64_t stack_trace_threshold_us;

  struct {
    /*! Controls whether slow datastore queries are recorded.  If set to true
     *  for a transaction, the transaction tracer records the top-10 slowest
     *  queries along with a stack trace of where the call occurred.
     *  Default: true.
     */
    bool enabled;

    /*! Controls the format of the sql put into transaction traces for supported
     *  sql-like products. Only relevant if the above
     *  datastore_reporting.enabled field is set to true.
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
     *
     *  Default: NEWRELIC_SQL_OBFUSCATED.
     */
    newrelic_tt_recordsql_t record_sql;

    /*! Specify the threshold above which a datastore query is considered
     *  "slow", in microseconds.  Only relevant if the above
     *  datastore_reporting.enabled field is set to true.
     *  Default: 500000, or 0.5 seconds.
     */
    uint64_t threshold_us;
  } datastore_reporting;

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
 * @brief Segment configuration used to instrument calls to databases and object
 * stores.
 */
typedef struct _newrelic_datastore_segment_params_t {
  /*! Specifies the datastore type, e.g., "MySQL", to indicate that the segment
   *  represents a query against a MySQL database. New Relic recommends using
   *  the predefined NEWRELIC_DATASTORE_FIREBIRD through
   *  NEWRELIC_DATASTORE_SYBASE constants for this field. If this field points
   *  to a string that is not one of NEWRELIC_DATASTORE_FIREBIRD through
   *  NEWRELIC_DATASTORE_SYBASE, the resulting datastore segment shall be
   *  instrumented as an unsupported datastore.
   *
   *  This field is required to be a non-empty, null-terminated string that does
   *  not include any slash characters.  Empty strings are replaced with the
   *  string NEWRELIC_DATASTORE_OTHER.
   */
  char* product;

  /*! Optional. Specifies the table or collection being used or queried against.
   *
   *  If provided, this field is required to be a null-terminated string that
   *  does not include any slash characters. It is also valid to use the default
   *  NULL value, in which case the default string of "other" will be attached
   *  to the datastore segment.
   */
  char* collection;

  /*! Optional. Specifies the operation being performed: for example, "select"
   *  for an SQL SELECT query, or "set" for a Memcached set operation.
   *  While operations may be specified with any case, New Relic suggests
   *  using lowercase.
   *
   *  If provided, this field is required to be a null-terminated string that
   *  does not include any slash characters. It is also valid to use the default
   *  NULL value, in which case the default string of "other" will be attached
   *  to the datastore segment.
   */
  char* operation;

  /*! Optional. Specifies the datahost host name.
   *
   *  If provided, this field is required to be a null-terminated string that
   *  does not include any slash characters. It is also valid to use the default
   *  NULL value, in which case the default string of "other" will be attached
   *  to the datastore segment.
   */
  char* host;

  /*! Optional. Specifies the port or socket used to connect to the datastore.
   *
   *  If provided, this field is required to be a null-terminated string.
   */
  char* port_path_or_id;

  /*! Optional. Specifies the database name or number in use.
   *
   *  If provided, this field is required to be a null-terminated string.
   */
  char* database_name;

  /*! Optional. Specifies the database query that was sent to the server.
   *  For security reasons, this value is only used if you set product to
   *  a supported sql-like datastore, NEWRELIC_DATASTORE_FIREBIRD,
   *  NEWRELIC_DATASTORE_INFORMIX, NEWRELIC_DATASTORE_MSSQL, etc. This
   *  allows the agent to correctly obfuscate the query. When the product
   *  is set otherwise, no query information is reported to New Relic.
   *
   *  If provided, this field is required to be a null-terminated string.
   */
  char* query;

} newrelic_datastore_segment_params_t;

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

typedef struct _newrelic_segment_t newrelic_segment_t;

newrelic_segment_t *newrelic_start_segment(newrelic_txn_t *transaction,
                                           const char *name);

bool newrelic_end_segment(newrelic_txn_t* transaction,
                          newrelic_segment_t **segment_ptr);

/*!
 * @brief Record the start of a datastore segment in a transaction.
 *
 * Given an active transaction and valid parameters, this function
 * creates a datastore segment to be recorded as part of the transaction.
 * A subsequent call to newrelic_end_datastore_segment() records the
 * ends of the segment.
 *
 * @param [in] transaction An active transaction.
 * @param [in] params Valid parameters describing a datastore segment.
 *
 * @return A pointer to a valid datastore segment; NULL otherwise.
 *
 */
newrelic_datastore_segment_t* newrelic_start_datastore_segment(
    newrelic_txn_t* transaction,
    const newrelic_datastore_segment_params_t* params);

/*!
 * @brief Record the completion of a datastore segment in a transaction.
 *
 * Given an active transaction, this function records the segment's data
 * on the transaction, including the original information supplied
 * in the newrelic_datastore_segment_params, the segment metrics, and
 * the segment's stacktrace.
 *
 * @param [in] transaction An active transaction.
 * @param [in,out] segment The address of a valid datastore segment.
 * Before the function returns, any segment_ptr memory is freed;
 * segment_ptr is set to NULL to avoid any potential double free errors.
 *
 * @return true if the parameters represented an active transaction
 * and datastore segment to record as complete; false otherwise.
 * If an error occurred, a log message will be written to the
 * agent log at LOG_ERROR level.
 */
bool newrelic_end_datastore_segment(newrelic_txn_t* transaction,
                                    newrelic_datastore_segment_t** segment_ptr);

/*! @brief The internal type used to represent an external segment. */
typedef struct _newrelic_external_segment_t newrelic_external_segment_t;

/*! @brief Parameters used when creating an external segment. */
typedef struct _newrelic_external_segment_params_t {
  /*!
   * The URI that was loaded. This field is required to be a null-terminated
   * string containing a valid URI, and cannot be NULL.
   */
  char* uri;

  /*!
   * The procedure used to load the external resource.
   *
   * In HTTP contexts, this will usually be the request method (eg `GET`,
   * `POST`, et al). For non-HTTP requests, or protocols that encode more
   * specific semantics on top of HTTP like SOAP, you may wish to use a
   * different value that more precisely encodes how the resource was
   * requested.
   *
   * If provided, this field is required to be a null-terminated string that
   * does not include any slash characters. It is also valid to provide NULL,
   * in which case no procedure will be attached to the external segment.
   */
  char* procedure;

  /*!
   * The library used to load the external resource.
   *
   * If provided, this field is required to be a null-terminated string that
   * does not include any slash characters. It is also valid to provide NULL,
   * in which case no library will be attached to the external segment.
   */
  char* library;
} newrelic_external_segment_params_t;

/*!
 * @brief Start recording an external segment within a transaction.
 *
 * Given an active transaction, this function creates an external segment
 * inside of the transaction and marks it as having been started. An external
 * segment is generally used to represent a HTTP or RPC request.
 *
 * @param [in] transaction An active transaction.
 * @param [in] params      The parameters describing the external request. All
 *                         parameters are copied, and no references to the
 *                         pointers provided are kept after this function
 *                         returns.
 * @return A pointer to an external segment, which may then be provided to
 *         newrelic_end_external_segment() when the external request is
 *         complete. If an error occurs when creating the external segment,
 *         NULL is returned, and a log message will be written to the agent log
 *         at LOG_ERROR level.
 */
newrelic_external_segment_t* newrelic_start_external_segment(
    newrelic_txn_t* transaction,
    const newrelic_external_segment_params_t* params);

/*!
 * @brief Stop recording an external segment within a transaction.
 *
 * Given an active transaction and an external segment created by
 * newrelic_start_external_segment(), this function stops the external segment
 * and saves it.
 *
 * @param [in]     transaction An active transaction.
 * @param [in,out] segment_ptr A pointer to the segment pointer returned by
 *                             newrelic_start_external_segment(). The segment
 *                             pointer will be set to NULL before this function
 *                             returns to avoid any potential double free
 *                             errors.
 * @return True if the segment was saved successfully, or false if an error
 *         occurred. If an error occurred, a log message will be written to the
 *         agent log at LOG_ERROR level.
 */
bool newrelic_end_external_segment(newrelic_txn_t* transaction,
                                   newrelic_external_segment_t** segment_ptr);

#ifdef __cplusplus
}
#endif

#endif /* LIBNEWRELIC_H */
