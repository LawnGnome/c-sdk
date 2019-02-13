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

/*!
 * @brief Transaction. A transaction is started using
 * newrelic_start_web_transaction() or
 * newrelic_start_non_web_transaction(). A started, or active, transaction is
 * stopped using newrelic_end_transaction(). One may modify a transaction
 * by adding custom attributes or recording errors only after it has been
 * started.
 */
typedef struct _newrelic_txn_t newrelic_txn_t;

/*!
 * @brief A time, measured in microseconds.
 */
typedef uint64_t newrelic_time_us_t;

/*!
 * @brief Log levels.  An enumeration of the possible log levels for an agent
 * configuration, or newrelic_config_t.
 *
 * @see struct _newrelic_config_t
 */
typedef enum _newrelic_loglevel_t {
  NEWRELIC_LOG_INFO,
  NEWRELIC_LOG_DEBUG,
  NEWRELIC_LOG_ERROR,
  NEWRELIC_LOG_VERBOSE
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
  newrelic_time_us_t duration_us;

  /*! Sets the threshold above which the New Relic agent will record a
   *  stack trace for a transaction trace, in microseconds.
   *  Default: 500000, or 0.5 seconds.
   */
  newrelic_time_us_t stack_trace_threshold_us;

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
    newrelic_time_us_t threshold_us;
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

typedef struct _newrelic_process_config_t {
  /*! Optional.  Specifies the underlying communication mechanism for the
   *  agent daemon.   The default is to use a UNIX-domain socket located at
   *  /tmp/.newrelic.sock. If you want to use UNIX domain sockets then this
   *  value must begin with a "/". Setting this to an integer value in the
   *  range 1-65534 will instruct the daemon to listen on a normal TCP socket
   *  on the port specified. On Linux, an abstract socket can be created by
   *  prefixing the socket name with '@'.
   */
  char daemon_socket[512];
} newrelic_process_config_t;

/*!
 * @brief Agent configuration used to describe application name, license key, as
 * well as optional transaction tracer and datastore configuration.
 *
 * @see newrelic_new_app_config().
 */
typedef struct _newrelic_app_config_t {
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

  /*! Optional. Specifies the file to be used for agent logs.  If no filename
   *  is provided, no logging shall occur.
   */
  char log_filename[512];

  /*! Optional. Specifies the logfile's level of detail. There is little reason
   * to change this from the default value except under the guidance of
   * technical support.
   *
   * Must be one of the following values: NEWRELIC_LOG_ERROR, NEWRELIC_LOG_INFO
   * (default), NEWRELIC_LOG_DEBUG, NEWRELIC_LOG_VERBOSE.
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

} newrelic_app_config_t;

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
 * @brief Configure the C agent's logging system.
 *
 * If the logging system was previously initialised (either by a prior call to
 * newrelic_configure_log() or implicitly by a call to newrelic_init() or
 * newrelic_create_app()), then invoking this function will close the previous
 * log file.
 *
 * @param [in] filename The path to the file to write logs to. If this is the
 * literal string "stdout" or "stderr", then logs will be written to standard
 * output or standard error, respectively.
 * @param [in] level The lowest level of log message that will be output.
 * @return true on success; false otherwise.
 */
bool newrelic_configure_log(const char* filename, newrelic_loglevel_t level);

/*!
 * @brief Initialise the C agent with non-default settings.
 *
 * Generally, this function only needs to be called explicitly if the daemon
 * socket location needs to be customised. By default, the C agent will use the
 * value in the NEW_RELIC_DAEMON_SOCKET environment variable, or
 * "/tmp/.newrelic.sock" if that environment variable is unset.
 *
 * If an explicit call to this function is required, it must occur before the
 * first call to newrelic_create_app().
 *
 * Subsequent calls to this function after a successful call to newrelic_init()
 * or newrelic_create_app() will fail.
 *
 * @param [in] daemon_socket The path to the daemon socket. On Linux, if this
 * starts with a literal '@', then this is treated as the name of an abstract
 * domain socket instead of a filesystem path. If this is NULL, then the
 * default behaviour described above will be used.
 * @param [in] time_limit_ms The amount of time, in milliseconds, that the C
 * agent will wait for a response from the daemon before considering
 * initialisation to have failed. If this is 0, then a default value will be
 * used.
 * @return true on success; false otherwise.
 */
bool newrelic_init(const char* daemon_socket, int time_limit_ms);

/*!
 * @brief Create a populated application configuration.
 *
 * Given an application name and license key, this method returns an agent
 * configuration. Specifically, it returns a pointer to a newrelic_app_config_t
 * with initialized app_name and license_key fields along with default values
 * for the remaining fields. The caller should free the agent configuration
 * after the application has been created.
 *
 * @param [in] app_name The name of the application.
 * @param [in] license_key A valid license key supplied by New Relic.
 *
 * @return An application configuration populated with app_name and
 * license_key; all other fields are initialized to their defaults.
 */
newrelic_app_config_t* newrelic_new_app_config(const char* app_name,
                                               const char* license_key);

/*!
 * @brief Create an application.
 *
 * Given an agent configuration, newrelic_create_app() returns a pointer to the
 * newly allocated application, or NULL if there was an error. If successful,
 * the caller should destroy the application with the supplied
 * newrelic_destroy_app() when finished.
 *
 * @param [in] config An application configuration created by
 * newrelic_new_app_config().
 * @param [in] timeout_ms Specifies the maximum time to wait for a connection to
 * be established; a value of 0 causes the method to make only one attempt at
 * connecting to the daemon.
 *
 * @return A pointer to an allocated application, or NULL on error; any errors
 * resulting from a badly-formed agent configuration are logged.
 */
newrelic_app_t* newrelic_create_app(const newrelic_app_config_t* config,
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
 *
 * @warning This function must only be called once for a given application.
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
 *
 * @warning This function must only be called once for a given transaction.
 */
bool newrelic_end_transaction(newrelic_txn_t** transaction_ptr);

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

/*!
 * @brief A segment within a transaction.
 */
typedef struct _newrelic_segment_t newrelic_segment_t;

/*!
 * @brief A Custom Event
 *
 * @see newrelic_create_custom_event().
 */
typedef struct _newrelic_custom_event_t newrelic_custom_event_t;

/*!
 * @brief Record the start of a custom segment in a transaction.
 *
 * Given an active transaction this function creates a custom segment to be
 * recorded as part of the transaction. A subsequent call to
 * newrelic_end_segment() records the end of the segment.
 *
 * @param [in] transaction An active transaction.
 * @param [in] name The segment name. If NULL or an invalid name is passed,
 * this defaults to "Unnamed segment".
 * @param [in] category The segment category. If NULL or an invalid category is
 * passed, this defaults to "Custom".
 *
 * @return A pointer to a valid custom segment; NULL otherwise.
 *
 */
newrelic_segment_t* newrelic_start_segment(newrelic_txn_t* transaction,
                                           const char* name,
                                           const char* category);

/*!
 * @brief Record the start of a datastore segment in a transaction.
 *
 * Given an active transaction and valid parameters, this function creates a
 * datastore segment to be recorded as part of the transaction. A subsequent
 * call to newrelic_end_segment() records the end of the segment.
 *
 * @param [in] transaction An active transaction.
 * @param [in] params Valid parameters describing a datastore segment.
 *
 * @return A pointer to a valid datastore segment; NULL otherwise.
 *
 */
newrelic_segment_t* newrelic_start_datastore_segment(
    newrelic_txn_t* transaction,
    const newrelic_datastore_segment_params_t* params);

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
 *         newrelic_end_segment() when the external request is complete. If an
 *         error occurs when creating the external segment, NULL is returned,
 *         and a log message will be written to the agent log at LOG_ERROR
 *         level.
 */
newrelic_segment_t* newrelic_start_external_segment(
    newrelic_txn_t* transaction,
    const newrelic_external_segment_params_t* params);

/*!
 * @brief Set the parent for the given segment.
 *
 * This function changes the parent for the given segment to another segment.
 * Both segments must exist on the same transaction, and must not have ended.
 *
 * @param [in] segment The segment to reparent.
 * @param [in] parent  The new parent segment.
 * @return True if the segment was successfully reparented; false otherwise.
 *
 * @warning Do not attempt to use a segment that has had newrelic_end_segment()
 *          called on it as a segment or parent: this will result in a
 *          use-after-free scenario, and likely a crash.
 */
bool newrelic_set_segment_parent(newrelic_segment_t* segment,
                                 newrelic_segment_t* parent);

/*!
 * @brief Override the timing for the given segment.
 *
 * Segments are normally timed automatically based on when they were started
 * and ended. Calling this function disables the automatic timing, and uses the
 * times given instead.
 *
 * Note that this may cause unusual looking transaction traces, as this
 * function does not change the parent segment. It is likely that users of this
 * function will also want to use newrelic_set_segment_parent() to manually
 * parent their segments.
 *
 * @param [in] segment    The segment to manually time.
 * @param [in] start_time The start time for the segment, in microseconds since
 *                        the start of the transaction.
 * @param [in] duration   The duration of the segment in microseconds.
 * @return True if the segment timing was changed; false otherwise.
 */
bool newrelic_set_segment_timing(newrelic_segment_t* segment,
                                 newrelic_time_us_t start_time,
                                 newrelic_time_us_t duration);

/*!
 * @brief Record the completion of a segment in a transaction.
 *
 * Given an active transaction, this function records the segment's metrics
 * on the transaction.
 *
 * @param [in] transaction An active transaction.
 * @param [in,out] segment The address of a valid segment.
 * Before the function returns, any segment_ptr memory is freed;
 * segment_ptr is set to NULL to avoid any potential double free errors.
 *
 * @return true if the parameters represented an active transaction
 * and custom segment to record as complete; false otherwise.
 * If an error occurred, a log message will be written to the
 * agent log at LOG_ERROR level.
 */
bool newrelic_end_segment(newrelic_txn_t* transaction,
                          newrelic_segment_t** segment_ptr);

/*!
 * @brief Creates a custom event
 *
 * Attributes can be added to the custom event using the
 * newrelic_custom_event_add_* family of functions. When the required attributes
 * have been added, the custom event can be recorded using
 * newrelic_record_custom_event().
 *
 * When passed to newrelic_record_custom_event, the custom event will be freed.
 * If you can't pass an allocated event to newrelic_record_custom_event, use the
 * newrelic_discard_custom_event function to free the event.
 *
 * @param [in] event_type The type/name of the event
 *
 * @return A pointer to a custom event; NULL otherwise.
 */
newrelic_custom_event_t* newrelic_create_custom_event(const char* event_type);

/*!
 * @brief Frees the memory for custom events created via the
 * newrelic_create_custom_event function
 *
 * This function is here in case there's an allocated newrelic_custom_event_t
 * that ends up not being recorded as a custom event, but still needs to be
 * freed
 *
 * @param [in] event The address of a valid custom event, @see
 *             newrelic_create_custom_event.
 */
void newrelic_discard_custom_event(newrelic_custom_event_t** event);

/*!
 * @brief Records the custom event.
 *
 * Given an active transaction, this function adds the custom event to the
 * transaction and timestamps it, ensuring the event will be sent to New Relic.
 *
 * @param [in] transaction pointer to a started transaction
 * @param [in] event The address of a valid custom event, @see
 *                   newrelic_create_custom_event.
 *
 * newrelic_create_custom_event
 */
void newrelic_record_custom_event(newrelic_txn_t* transaction,
                                  newrelic_custom_event_t** event);

/*!
 * @brief Adds an int key/value pair to the custom event's attributes
 *
 * Given a custom event, this function adds an integer attributes to the event.
 *
 * @param [in] event A valid custom event, @see newrelic_create_custom_event.
 * @param [in] key the string key for the key/value pair
 * @param [in] value the integer value of the key/value pair
 *
 * @return false indicates the attribute could not be added
 */
bool newrelic_custom_event_add_attribute_int(newrelic_custom_event_t* event,
                                             const char* key,
                                             int value);

/*!
 * @brief Adds a long key/value pair to the custom event's attributes
 *
 * Given a custom event, this function adds a long attribute to the event.
 *
 * @param [in] event A valid custom event, @see newrelic_create_custom_event.
 * @param [in] key the string key for the key/value pair
 * @param [in] value the long value of the key/value pair
 *
 * @return false indicates the attribute could not be added
 */
bool newrelic_custom_event_add_attribute_long(newrelic_custom_event_t* event,
                                              const char* key,
                                              long value);

/*!
 * @brief Adds a double key/value pair to the custom event's attributes
 *
 * Given a custom event, this function adds a double attribute to the event.
 *
 * @param [in] event A valid custom event, @see newrelic_create_custom_event.
 * @param [in] key the string key for the key/value pair
 * @param [in] value the double value of the key/value pair
 *
 * @return false indicates the attribute could not be added
 */
bool newrelic_custom_event_add_attribute_double(newrelic_custom_event_t* event,
                                                const char* key,
                                                double value);

/*!
 * @brief Adds a string key/value pair to the custom event's attributes
 *
 * Given a custom event, this function adds a char* (string) attribute to the
 * event.
 *
 * @param [in] event A valid custom event, @see newrelic_create_custom_event.
 * @param [in] key the string key for the key/value pair
 * @param [in] value the string value of the key/value pair
 *
 * @return false indicates the attribute could not be added
 */
bool newrelic_custom_event_add_attribute_string(newrelic_custom_event_t* event,
                                                const char* key,
                                                const char* value);

/*!
 * @brief Get the agent version.
 *
 * @return A NULL terminated string containing the C agent version number. If
 * the version number is unavailable, the string "NEWRELIC_VERSION" will be
 * returned.
 *
 * This string is owned by the agent, and must not be freed or modified.
 */
const char* newrelic_version(void);

/*
 * @brief Generate a custom metric.
 *
 * Given an active transaction and valid parameters, this function creates a
 * custom metric to be recorded as part of the transaction.
 *
 * @param [in] transaction An active transaction.
 * @param [in] metric_name The name/identifier for the metric.
 * @param [in] milliseconds The amount of time the metric will
 *             record, in milliseconds.
 *
 * @return true on success.
 */
bool newrelic_record_custom_metric(newrelic_txn_t* transaction,
                                   const char* metric_name,
                                   double milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* LIBNEWRELIC_H */
