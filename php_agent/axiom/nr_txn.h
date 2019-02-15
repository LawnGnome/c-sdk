/*
 * This file contains data types and functions for dealing with a transaction.
 *
 * This file is agent-agnostic. It defines the data types and functions used
 * to record a single transaction. A transaction is defined as a single web
 * request or a single invocation on the command line. A transaction can also
 * be started and stopped programatically, by means of API calls.
 */
#ifndef NR_TXN_HDR
#define NR_TXN_HDR

#include <stdint.h>
#include <stdbool.h>

#include "nr_analytics_events.h"
#include "nr_app.h"
#include "nr_attributes.h"
#include "nr_errors.h"
#include "nr_file_naming.h"
#include "nr_segment.h"
#include "nr_slowsqls.h"
#include "nr_synthetics.h"
#include "nr_distributed_trace.h"
#include "util_apdex.h"
#include "util_buffer.h"
#include "util_json.h"
#include "util_metrics.h"
#include "util_sampling.h"
#include "util_stack.h"
#include "util_string_pool.h"

#define NR_TXN_REQUEST_PARAMETER_ATTRIBUTE_PREFIX "request.parameters."

typedef enum _nr_tt_recordsql_t {
  NR_SQL_NONE = 0,
  NR_SQL_RAW = 1,
  NR_SQL_OBFUSCATED = 2
} nr_tt_recordsql_t;

/*
 * This structure contains transaction options.
 * Originally, this structure was populated at the transaction's start and
 * never modified:  If options needed to be changed, then a duplicate setting
 * would be put into the status structure.  This has been abandoned and
 * "autorum_enabled" and "request_params_enabled" may be changed during the
 * transaction.
 */
typedef struct _nrtxnopt_t {
  int custom_events_enabled; /* Whether or not to capture custom events */
  int synthetics_enabled;    /* Whether or not to enable Synthetics support */
  int instance_reporting_enabled; /* Whether to capture datastore instance host
                                     and port */
  int database_name_reporting_enabled; /* Whether to include database name in
                                          datastore instance */
  int err_enabled;                     /* Whether error reporting is enabled */
  int request_params_enabled; /* Whether recording request parameters is enabled
                               */
  int autorum_enabled;        /* Whether auto-RUM is enabled or not */
  int analytics_events_enabled; /* Whether to record analytics events */
  int error_events_enabled;     /* Whether to record error events */
  int tt_enabled;               /* Whether to record TT's or not */
  int ep_enabled;               /* Whether to request explain plans or not */
  nr_tt_recordsql_t
      tt_recordsql; /* How to record SQL statements in TT's (if at all) */
  int tt_slowsql;   /* Whether to support the slow SQL feature */
  nrtime_t apdex_t; /* From app default unless key txn */
  nrtime_t
      tt_threshold;  /* TT threshold in usec - faster than this isn't a TT */
  int tt_is_apdex_f; /* tt_threshold is 4 * apdex_t */
  nrtime_t ep_threshold;     /* Explain Plan threshold in usec */
  nrtime_t ss_threshold;     /* Slow SQL stack threshold in usec */
  int cross_process_enabled; /* Whether or not to read and modify headers */
  int allow_raw_exception_messages; /* Whether to replace the error/exception
                                   messages with generic text */
  int custom_parameters_enabled;    /* Whether to allow recording of custom
                                      parameters/attributes */

  int distributed_tracing_enabled; /* Whether distributed tracing functionality
                                      is enabled */
  int span_events_enabled;         /* Whether span events are enabled */
} nrtxnopt_t;

typedef enum _nrtxnstatus_cross_process_t {
  NR_STATUS_CROSS_PROCESS_DISABLED = 0, /* Cross process has been disabled */
  NR_STATUS_CROSS_PROCESS_START
  = 1, /* The response header has not been created */
  NR_STATUS_CROSS_PROCESS_RESPONSE_CREATED
  = 2 /* The response header has been created */
} nrtxnstatus_cross_process_t;

/*
 * There is precedence scheme to web transaction names. Larger numbers
 * indicate higher priority. Frozen paths are indicated with a
 * separate field in the txn structure; you should always consult the
 * path_is_frozen before doing other comparisons or assignments.
 *
 * See
 * https://newrelic.atlassian.net/wiki/display/eng/PHP+Agent+Troubleshooting#PHPAgentTroubleshooting-WebTransactionNaming
 *
 * It is tempting to make this be a strong-typed C++ enumeration using
 * ENUM_START ... ENUM_END macros, but that will not work, since
 * members of this enumeration are used as case labels.
 */
typedef enum _nr_path_type_t {
  NR_PATH_TYPE_UNKNOWN = 0,
  NR_PATH_TYPE_URI = 1,
  NR_PATH_TYPE_ACTION = 2,
  NR_PATH_TYPE_FUNCTION = 3,
  NR_PATH_TYPE_CUSTOM = 4
} nr_path_type_t;

typedef struct _nrtxncat_t {
  char* inbound_guid;
  char* trip_id;
  char* referring_path_hash;
  nrobj_t* alternate_path_hashes;
  char* client_cross_process_id; /* Inbound X-NewRelic-ID (decoded and valid) */
} nrtxncat_t;

typedef struct _nrtxnstatus_t {
  int has_inbound_record_tt;  /* 1 if the inbound request header has a true
                                 record_tt, 0 otherwise */
  int has_outbound_record_tt; /* 1 if an outbound response header has a true
                                 record_tt, 0 otherwise */
  int path_is_frozen;         /* 1 is path is frozen, 0 otherwise */
  nr_path_type_t path_type;   /* Path type */
  int ignore;                 /* Set if this transaction should be ignored */
  int ignore_apdex; /* Set if no apdex metrics should be generated for this txn
                     */
  int background;   /* Set if this is a background job */
  int recording;    /* Set to 1 if we are recording, 0 if not */
  int rum_header;
  /* 0 = header not sent, 1 = sent manually, 2 = auto */ /* TODO(rrh): use
                                                            an enumeration
                                                          */
  int rum_footer;
  /* 0 = footer not sent, 1 = sent manually, 2 = auto */ /* TODO(rrh): use
                                                            an enumeration
                                                          */
  nrtime_t http_x_start; /* X-Request-Start time, or 0 if none */
  nrtxnstatus_cross_process_t cross_process;
} nrtxnstatus_t;

typedef struct _nrtxntime_t {
  int stamp;     /* Ever-increasing sequence stamp */
  nrtime_t when; /* When did this event occur */
} nrtxntime_t;

typedef enum _nr_txnnode_type_t {
  NR_CUSTOM,
  NR_DATASTORE,
  NR_EXTERNAL
} nr_txnnode_type_t;

/*
 * The union type can only hold one struct at a time. This insures that we will
 * not reserve memory for variables that are not applicable for this type of
 * node. Example: A datastore node will not need to store a method and an
 * external node will not need to store a component.
 *
 * You must check the nr_txnnode_type to determine which struct is being used.
 */

typedef struct {
  nr_txnnode_type_t type;
  struct {
    char* component; /* The name of the database vendor or driver */
  } datastore;
} nr_txnnode_attributes_t;

typedef struct _nrtxnnode_t {
  nrtxntime_t start_time; /* Start time for node */
  nrtxntime_t stop_time;  /* Stop time for node */
  int count;              /* N+1 rollup count */
  int name;               /* Node name (pooled string index) */
  int async_context;      /* Execution context (pooled string index) */
  char* id;               /* Node id.
              
                             If this is NULL, a new id will be created when a
                             span event is created from this trace node.
              
                             If this is not NULL, this id will be used for
                             creating a span event from this trace node. This
                             id set indicates that the node represents an
                             external segment and the id of the segment was use
                             as current span id in an outgoing DT payload. */
  nrobj_t* data_hash;     /* Other node specific data */
  nr_txnnode_attributes_t*
      attributes; /* Category-dependent attributes required for some
                          categories of span events, e.g. datastore. */
} nrtxnnode_t;

/*
 * Members of this enumeration are used as an index into an array.
 */
typedef enum _nr_cpu_usage_t {
  NR_CPU_USAGE_START = 0,
  NR_CPU_USAGE_END = 1,
  NR_CPU_USAGE_COUNT = 2
} nr_cpu_usage_t;

/*
 * Possible transaction types, which go into the type bitfield in the nrtxn_t
 * struct.
 *
 * NR_TXN_TYPE_CAT_INBOUND indicates both X-NewRelic-ID header and a valid
 * X-NewRelic-Transaction header were received.
 *
 * NR_TXN_TYPE_CAT_OUTBOUND indicates that we sent one or more external
 * requests with CAT headers.
 */
#define NR_TXN_TYPE_SYNTHETICS (1 << 0)
#define NR_TXN_TYPE_CAT_INBOUND (1 << 2)
#define NR_TXN_TYPE_CAT_OUTBOUND (1 << 3)
#define NR_TXN_TYPE_DT_INBOUND (1 << 4)
#define NR_TXN_TYPE_DT_OUTBOUND (1 << 5)
typedef uint32_t nrtxntype_t;

/*
 * The main transaction structure
 */
typedef struct _nrtxn_t {
  char* agent_run_id;   /* The agent run ID */
  int high_security;    /* From application: Whether the txn is in special high
                           security mode */
  int lasp;             /* From application: Whether the txn is in special lasp
                           enabled mode */
  nrtxnopt_t options;   /* Options for this transaction */
  nrtxnstatus_t status; /* Status for the transaction */
  nrtxncat_t cat;       /* Incoming CAT fields */
  nr_random_t* rnd;     /* Random number generator, owned by the application */
  int nodes_used;       /* Number of nodes used */
  nrtxnnode_t root;     /* Root node */
  nrtxnnode_t nodes[NR_TXN_MAX_NODES];
  nrtxnnode_t* pq[NR_TXN_MAX_NODES];
  nrtxnnode_t* last_added; /* Pointer to last node added (rollup metrics) */

  nr_stack_t parent_stack; /* A stack to track the current parent in the tree of
                              segments */
  size_t segment_count; /* A count of segments for this transaction, maintained
                           throughout the life of this transaction */
  nr_segment_t* segment_root; /* The root pointer to the tree of segments */
  nrtime_t abs_start_time; /* The absolute start timestamp for this transaction;
                            * all segment start and end times are relative to
                            * this field */

  int stamp;                    /* Node stamp counter */
  nr_error_t* error;            /* Captured error */
  nr_slowsqls_t* slowsqls;      /* Slow SQL statements */
  nrpool_t* datastore_products; /* Datastore products seen */
  nrpool_t* trace_strings;      /* String pool for transaction trace */
  nrmtable_t*
      scoped_metrics; /* Contains metrics that are both scoped and unscoped. */
  nrmtable_t* unscoped_metrics; /* Unscoped metric table for the txn */
  nrobj_t* intrinsics; /* Attribute-like builtin fields sent along with traces
                          and errors */
  nr_attributes_t* attributes; /* Key+value pair tags put in txn event, txn
                                  trace, error, and browser */
  nrtime_t* cur_kids_duration; /* Points to variable to increment for a child's
                                  duration */
  nrtime_t root_kids_duration; /* Duration of children of root */
  nr_file_naming_t* match_filenames; /* Filenames to match on for txn naming */

  /*
   * This is the amount of time spent within Guzzle external calls: This time
   * is not truly asynchronous, but the async UI is used for web transactions.
   */
  nrtime_t async_duration;

  nr_analytics_events_t*
      custom_events; /* Custom events created through the API. */
  nrtime_t user_cpu[NR_CPU_USAGE_COUNT]; /* User CPU usage */
  nrtime_t sys_cpu[NR_CPU_USAGE_COUNT];  /* System CPU usage */

  char* license;     /* License copied from application for RUM encoding use. */
  char* request_uri; /* Request URI */
  char*
      path; /* Request URI or action (txn name before rules applied & prefix) */
  char* name; /* Full transaction metric name */

  nrtxntype_t type; /* The transaction type(s), as a bitfield */

  nrobj_t* app_connect_reply; /* Contents of application collector connect
                                 command reply */
  char* primary_app_name; /* The primary app name in use (ie the first rollup
                             entry) */
  nr_synthetics_t* synthetics; /* Synthetics metadata for the transaction */

  nr_distributed_trace_t*
      distributed_trace; /* distributed tracing metadata for the transaction */

  /*
   * Special control variables derived from named bits in
   * nrphpglobals_t.special_flags These are used to debug the agent, possibly in
   * the field.
   */
  struct {
    uint8_t no_sql_parsing;   /* don't do SQL parsing */
    uint8_t show_sql_parsing; /* show various steps in SQL feature parsing */
    uint8_t debug_cat;        /* extra logging for CAT */
  } special_flags;
} nrtxn_t;

static inline int nr_txn_recording(const nrtxn_t* txn) {
  if (nrlikely((0 != txn) && (0 != txn->status.recording))) {
    return 1;
  } else {
    return 0;
  }
}

/*
 * Purpose : Compare two structs of type nrtxnopt_t.
 *
 * Params  : 1. o1, a ptr to the first struct for comparison.
 *           2. o2, a ptr to the second struct for comparison.
 *
 * Returns : true if
 *           - o1 and o2 are equal.
 *           - All fields of o1 and o2 are equal.
 *           and false otherwise.
 *
 * Notes   : Defined for testing purposes, to test whether a generated
 *           set of options are initialized as expected.
 */
bool nr_txn_cmp_options(nrtxnopt_t* o1, nrtxnopt_t* o2);

/*
 * Purpose : Compare connect_reply and security_policies to settings found
 *           in opts. If SSC or LASP policies are more secure, update local
 *           settings to match and log a verbose debug message.
 *
 * Params  : 1. options set in the transaction
 *           2. connect_reply originally obtained from the daemon
 *           3. security_policies originally obtained from the daemon
 *
 */
void nr_txn_enforce_security_settings(nrtxnopt_t* opts,
                                      const nrobj_t* connect_reply,
                                      const nrobj_t* security_policies);

/*
 * Purpose : Start a new transaction belonging to the given application.
 *
 * Params  : 1. The relevant application.  This application is assumed
 *              to be locked and is not unlocked by this function.
 *           2. Pointer to the starting options for the transaction.
 *
 * Returns : A newly created transaction pointer or NULL if the request could
 *           not be completed.
 *
 * Notes   : It is up to the caller to ensure that only one transaction is
 *           active in a given context (thread, request etc).
 */
extern nrtxn_t* nr_txn_begin(nrapp_t* app,
                             const nrtxnopt_t* opts,
                             const nr_attribute_config_t* attribute_config);

/*
 * Purpose : End a transaction by finalizing all metrics and timers.
 *
 * Params  : 1. Pointer to the transaction being ended.
 */
extern void nr_txn_end(nrtxn_t* txn);

/*
 * Purpose : Set the timing of a transaction.
 *
 *           1. The pointer to the transaction to be retimed.
 *           2. The new start time for the transaction, in microseconds since
 *              the since the UNIX epoch.
 *           3. The new duration for the transaction, in microseconds.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_txn_set_timing(nrtxn_t* txn, nrtime_t start, nrtime_t duration);

/*
 * Purpose : Set the transaction path and type.  Writes a log message.
 *
 * Params  :
 *           1. A descriptive name for the originator of the txn name, used for
 * logging.
 *           2. The transaction to set the path in.
 *           3. The path.
 *           4. The path type (nr_path_type_t above).
 *           5. describes if it is OK to overwrite the existing transaction name
 * at the same priority level.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 *
 * Notes   : If the path type has already been frozen then this function
 *           silently ignores the request to change the path type.
 */
typedef enum _txn_assignment_t {
  NR_NOT_OK_TO_OVERWRITE,
  NR_OK_TO_OVERWRITE
} nr_txn_assignment_t;

extern nr_status_t nr_txn_set_path(const char* whence,
                                   nrtxn_t* txn,
                                   const char* path,
                                   nr_path_type_t ptype,
                                   nr_txn_assignment_t ok_to_override);

/*
 * Purpose : Set the request URI ("real path") for the transaction.
 *
 * Params  : 1. The transaction to set the path in.
 *           2. The request URI.
 *
 * Notes   : The request URI is used in transaction traces, slow sqls, and
 *           errors.  This function will obey the transaction's
 *           options.request_params_enabled
 *           setting and remove trailing '?' parameters correctly.
 */
extern void nr_txn_set_request_uri(nrtxn_t* txn, const char* uri);

/*
 * Purpose : Set up a timing structure for a potential new node.
 *
 * Params  : 1. The transaction pointer.
 *           2. Pointer to the timing structure.
 *
 * Returns : Nothing.
 *
 * Notes   : This is a frequently called function that should be as optimized
 *           as possible. It is called by the agent specific code whenever a
 *           function is being instrumented, immediately before executing the
 *           function. When the function returns, the agent code must call one
 *           of the node termination functions, defined below. The agnostic
 *           code will do all the work to determine whether or not the node is
 *           kept.
 */
static inline void nr_txn_set_time(nrtxn_t* txn, nrtxntime_t* t) {
  if (nrunlikely(0 == t)) {
    return;
  }
  if (nrunlikely(0 == txn)) {
    t->when = 0;
    t->stamp = 0;
    return;
  } else {
    t->when = nr_get_time();
    t->stamp = txn->stamp;
    txn->stamp += 1;
  }
}

/*
 * Purpose : Populate the stop timing structure after a call has ended, and
 *           verify using the transaction and start structure that the
 *           transaction has not changed during the course of the call.
 *
 * Params  : 1. The current transaction.
 *           2. The start timing structure of the call (previously populated).
 *           3. The stop timing structure to be populated.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.  If NR_FAILURE is returned, then
 *           no data about the completed call should be recorded.
 */
extern nr_status_t nr_txn_set_stop_time(nrtxn_t* txn,
                                        const nrtxntime_t* start,
                                        nrtxntime_t* stop);

static inline void nr_txn_copy_time(const nrtxntime_t* src, nrtxntime_t* dest) {
  if (nrlikely((0 != src) && (0 != dest))) {
    dest->stamp = src->stamp;
    dest->when = src->when;
  }
}

/*
 * Purporse : Save a node within the transaction's trace.
 *
 * Params   : 1. The current transaction.
 *            2. The node's start time.
 *            3. The node's stop time.
 *            4. The name of the node.
 *            5. The execution context, or NULL if the node is on the main
 *               "thread".
 *            6. Hash containing extra node information.
 *            7. Category-dependent attributes required for some  categories of
 *               span events, e.g. datastore.
 */
extern void nr_txn_save_trace_node(nrtxn_t* txn,
                                   const nrtxntime_t* start,
                                   const nrtxntime_t* stop,
                                   const char* name,
                                   const char* async_context,
                                   const nrobj_t* data_hash,
                                   const nr_txnnode_attributes_t* attributes);

#include "node_datastore.h"
#include "node_external.h"

/*
 * Purpose : Indicate whether or not an error with the given priority level
 *           would be saved in the transaction. Used to prevent gathering of
 *           information about errors that would not be saved.
 *
 * Params  : 1. The transaction pointer.
 *           2. The priority of the error. A higher number indicates a more
 *              serious error.
 *
 * Returns : NR_SUCCESS if the error would be saved, NR_FAILURE otherwise.
 */
extern nr_status_t nr_txn_record_error_worthy(const nrtxn_t* txn, int priority);

/*
 * Purpose : Record the given error in the transaction.
 *
 * Params  : 1. The transaction pointer.
 *           2. The priority of the error. A higher number indicates a more
 *              serious error.
 *           3. The error message.
 *           4. The error class.
 *           5. Stack trace in JSON format.
 *
 * Returns : Nothing.
 *
 * Notes   : This function will still record an error when high security is
 *           enabled but the message will be replaced with a placeholder.
 */
#define NR_TXN_HIGH_SECURITY_ERROR_MESSAGE \
  "Message removed by New Relic high_security setting"

#define NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE \
  "Message removed by New Relic security settings"

extern void nr_txn_record_error(nrtxn_t* txn,
                                int priority,
                                const char* errmsg,
                                const char* errclass,
                                const char* stacktrace_json);

/*
 * Purpose : Create the transaction name, apply all rules to it, and store it
 *           in the transaction's string pool. It can later be used in the
 *           RUM buffer and for metrics. The transaction name is used to check
 *           if the transaction is a key transaction, and if so, the apdex value
 *           is updated. In the course of applying url_rules and txn_rules, if
 *           an 'ignore' rule is matched then the entire transaction should be
 *           ignored.
 *
 * Params  : 1. The transaction pointer.
 *
 * Returns : NR_FAILURE if the transaction should be ignored, NR_SUCCESS
 *           otherwise.
 *
 */
extern nr_status_t nr_txn_freeze_name_update_apdex(nrtxn_t* txn);

/*
 * Purpose : Create a supportability metric name to be created when the
 *           instrumented function is called.
 */
extern char* nr_txn_create_fn_supportability_metric(const char* function_name,
                                                    const char* class_name);

/*
 * Purpose : Force an unscoped metric with a single count of the given name.
 */
extern void nr_txn_force_single_count(nrtxn_t* txn, const char* metric_name);

/*
 * Purpose : Determine whether the given transaction trace should be
 *           force persisted when sent to the collector.  Force persisted
 *           traces should have some noteworthy property and should be
 *           sent to the collector with the "force_persist" JSON boolean
 *           set to true.
 *
 * Returns : 1 if the transaction should be force persisted, and 0 otherwise.
 */
extern int nr_txn_should_force_persist(const nrtxn_t* txn);

/*
 * Purpose : Destroy a transaction, freeing all of its associated memory.
 */
extern void nr_txn_destroy(nrtxn_t** txnptr);

/*
 * Purpose : Mark the transaction as being a background job or web transaction.
 *
 * Params  : 1. The current transaction.
 *           2. An optional string used in a debug log message to indicate
 *              why the background status of the transaction has been changed.
 */
extern void nr_txn_set_as_background_job(nrtxn_t* txn, const char* reason);
extern void nr_txn_set_as_web_transaction(nrtxn_t* txn, const char* reason);

/*
 * Purpose : Set the http response code of the transaction.
 */
extern void nr_txn_set_http_status(nrtxn_t* txn, int http_code);

/*
 * Purpose : Add a key:value attribute pair to the current transaction's
 *           custom parameters.
 *
 * Returns : NR_SUCCESS if the parameter was added successfully, or NR_FAILURE
 *           if there was any sort of error.
 */
extern nr_status_t nr_txn_add_user_custom_parameter(nrtxn_t* txn,
                                                    const char* key,
                                                    const nrobj_t* value);

/*
 * Purpose : Add a request parameter to the transaction's attributes.
 *
 * Params  : 1. The current transaction.
 *           2. The request parameter name.
 *           3. The request parameter value.
 *           4. Whether or not request parameters have been enabled by a
 *              deprecated (non-attribute) configuration setting.
 */
extern void nr_txn_add_request_parameter(nrtxn_t* txn,
                                         const char* key,
                                         const char* value,
                                         int legacy_enable);

/*
 * Purpose : These attributes have special functions since the request referrer
 *           must be cleaned and the content length is converted to a string.
 */
extern void nr_txn_set_request_referer(nrtxn_t* txn,
                                       const char* request_referer);
extern void nr_txn_set_request_content_length(nrtxn_t* txn,
                                              const char* content_length);

struct _nr_txn_attribute_t;
typedef struct _nr_txn_attribute_t nr_txn_attribute_t;

/*
 * Purpose : Create attributes with the correct names and destinations.
 *           For relevant links see the comment above the definitions.
 */
extern const nr_txn_attribute_t* nr_txn_request_accept_header;
extern const nr_txn_attribute_t* nr_txn_request_content_type;
extern const nr_txn_attribute_t* nr_txn_request_content_length;
extern const nr_txn_attribute_t* nr_txn_request_host;
extern const nr_txn_attribute_t* nr_txn_request_method;
extern const nr_txn_attribute_t* nr_txn_request_user_agent;
extern const nr_txn_attribute_t* nr_txn_server_name;
extern const nr_txn_attribute_t* nr_txn_response_content_type;
extern const nr_txn_attribute_t* nr_txn_response_content_length;
extern void nr_txn_set_string_attribute(nrtxn_t* txn,
                                        const nr_txn_attribute_t* attribute,
                                        const char* value);
extern void nr_txn_set_long_attribute(nrtxn_t* txn,
                                      const nr_txn_attribute_t* attribute,
                                      long value);

/*
 * Purpose : Return the length of the transaction.  This function will return
 *           0 if the transaction has not yet finished or if the transaction
 *           is NULL.
 */
extern nrtime_t nr_txn_duration(const nrtxn_t* txn);

/*
 * Purpose : Return the length of the transaction up to now.
 *           This function returns now - txn's start time.
 */
extern nrtime_t nr_txn_unfinished_duration(const nrtxn_t* txn);

/*
 * Purpose : Return the queue time associated with this transaction.
 *           If no queue start time has been recorded then this function
 *           will return 0.
 */
extern nrtime_t nr_txn_queue_time(const nrtxn_t* txn);

/*
 * Purpose : Set the time at which this transaction entered a web server queue
 *           prior to being started.
 */
extern void nr_txn_set_queue_start(nrtxn_t* txn, const char* x_request_start);

/*
 * Purpose : Add a custom event.
 */
extern void nr_txn_record_custom_event(nrtxn_t* txn,
                                       const char* type,
                                       const nrobj_t* params);

/*
 * Purpose : Return the CAT trip ID for the current transaction.
 *
 * Params  : 1. The current transaction.
 *
 * Returns : A pointer to the current trip ID within the transaction struct.
 */
extern const char* nr_txn_get_cat_trip_id(const nrtxn_t* txn);

/*
 * Purpose : Return the GUID for the given transaction.
 *
 * Params  : 1. The transaction.
 *
 * Returns : A pointer to the current GUID within the transaction struct.
 */
extern const char* nr_txn_get_guid(const nrtxn_t* txn);

/*
 * Purpose : Set the GUID for the given transaction.
 *
 * Params  : 1. The transaction.
 *           2. The GUID. This may be NULL to remove the GUID.
 *
 * Notes   : 1. This function is intended for internal testing use only.
 *           2. If a distributed trace is not already present within the
 *              transaction struct, one will be created. If the transaction is
 *              not destroyed with nr_txn_destroy(), it is _your_ responsibility
 *              to ensure that nr_distributed_trace_destroy() is invoked to
 *              avoid a memory leak.
 */
extern void nr_txn_set_guid(nrtxn_t* txn, const char* guid);

/*
 * Purpose : Generate and return the current CAT path hash for the transaction.
 *
 * Params  : 1. The current transaction.
 *
 * Returns : A newly allocated string containing the hash, or NULL if an error
 *           occurred.
 */
extern char* nr_txn_get_path_hash(nrtxn_t* txn);

/*
 * Purpose : Checks if the given account ID is a trusted account for CAT.
 *
 * Params  : 1. The current transaction.
 *           2. The account ID to check.
 *
 * Returns : Non-zero if the account is trusted; zero otherwise.
 */
extern int nr_txn_is_account_trusted(const nrtxn_t* txn, int account_id);

/*
 * Purpose : Checks if the given account ID is a trusted account for DT.
 *
 * Params  : 1. The current transaction.
 *           2. The account ID to check.
 *
 * Returns : true if the account is trusted; false otherwise.
 */
extern bool nr_txn_is_account_trusted_dt(const nrtxn_t* txn,
                                         const char* trusted_key);

/*
 * Purpose : Check if the transaction should create apdex metrics.
 *
 * Params  : 1. The transaction.
 *
 * Returns : Non-zero if the transaction should create apdex metrics; zero
 *           otherwise.
 */
extern int nr_txn_should_create_apdex_metrics(const nrtxn_t* txn);

/*
 * Purpose : Checks if a transaction trace should be saved for this
 *           transaction.
 *
 * Params  : 1. The transaction.
 *           2. The duration of the transaction.
 *
 * Returns : Non-zero if a trace should be saved; zero otherwise.
 */
extern int nr_txn_should_save_trace(const nrtxn_t* txn, nrtime_t duration);

/*
 * Purpose : The exclusive times of all scoped metrics should sum to the
 *           transaction's duration.  Therefore, when a scoped metric is
 *           made this function must be called with the duration of the metric
 *           so that the parent's exclusive time can be calculated.
 */
extern void nr_txn_adjust_exclusive_time(nrtxn_t* txn, nrtime_t duration);

/*
 * Purpose : Increment the async duration of the transaction.
 *
 * Params  : 1. The transaction.
 *           2. The amount of time to add to the async duration.
 */
extern void nr_txn_add_async_duration(nrtxn_t* txn, nrtime_t duration);

/*
 * Purpose : Validate that the transaction is recording and that the start
 *           and stop times comprise a valid call in the current transaction.
 */
extern int nr_txn_valid_node_end(const nrtxn_t* txn,
                                 const nrtxntime_t* start,
                                 const nrtxntime_t* stop);

/*
 * Purpose : Return 1 if the txn's nr.guid should be added as an
 *           intrinsic to the txn's analytics event, and 0 otherwise.
 */
extern int nr_txn_event_should_add_guid(const nrtxn_t* txn);

/*
 * Purpose : Get the effective SQL recording setting for the transaction, taking
 *           into account high security mode.
 *
 * Params  : 1. The transaction.
 *
 * Returns : The recording level.
 */
extern nr_tt_recordsql_t nr_txn_sql_recording_level(const nrtxn_t* txn);

/*
 * Purpose : Adds CAT intrinsics to the analytics event parameters.
 */
extern void nr_txn_add_cat_analytics_intrinsics(const nrtxn_t* txn,
                                                nrobj_t* intrinsics);

/*
 * Purpose : Generate the apdex zone for the given transaction.
 *
 * Params  : 1. The transaction.
 *           2. The duration of the transaction.
 *
 * Returns : The apdex.
 */
extern nr_apdex_zone_t nr_txn_apdex_zone(const nrtxn_t* txn, nrtime_t duration);

extern int nr_txn_is_synthetics(const nrtxn_t* txn);

/*
 * Purpose : Returns the time at which the txn started as a double. Returns 0
 *           if the txn is NULL.
 */
extern double nr_txn_start_time_secs(const nrtxn_t* txn);

/*
 * Purpose : Returns the time at which the txn started as an nrtime_t. Returns 0
 *           if the txn is NULL.
 */
extern nrtime_t nr_txn_start_time(const nrtxn_t* txn);

/*
 * Purpose : Given a transaction and a time relative to the start of the
 * transaction, return the absolute time. Returns relative_time if the txn
 * is NULL.
 */
extern nrtime_t nr_txn_time_rel_to_abs(const nrtxn_t* txn,
                                       const nrtime_t relative_time);

/*
 * Purpose : Given a transaction and an absolute time, return the time 
 * relative to the start of the transaction. Returns absolute_time if the txn
 * is NULL.
 */
extern nrtime_t nr_txn_time_abs_to_rel(const nrtxn_t* txn,
                                       const nrtime_t absolute_time);

/*
 * Purpose : Add a pattern to the list of files that will be matched on for
 *           transaction file naming.
 *
 * Note    : As of Apr 2015 (almost 4.21), this is only called by
 *           nr_txn_add_match_files.  However, it was decided to leave this
 *           exposed as it could be useful in the future.
 */
extern void nr_txn_add_file_naming_pattern(nrtxn_t* txn,
                                           const char* user_pattern);

/*
 * Purpose : Add a comma-separated list of regex patterns to be matched against
 *           for file naming to a transaction.
 */
extern void nr_txn_add_match_files(nrtxn_t* txn,
                                   const char* comma_separated_list);

/*
 * Purpose : Check a filename against the list of match patterns registered for
 *           a given transaction. If a match is found, name the transaction
 *           according to the txn config.
 */
extern void nr_txn_match_file(nrtxn_t* txn, const char* filename);

/*
 * Purpose : Generate an error event.
 *
 * Params  : 1. The transaction.
 *
 * Returns : An error event.
 */
extern nr_analytics_event_t* nr_error_to_event(const nrtxn_t* txn);

/*
 * Purpose : Generate a transaction event.
 *
 * Params  : 1. The transaction.
 *
 * Returns : A transaction event.
 */
extern nr_analytics_event_t* nr_txn_to_event(const nrtxn_t* txn);

/*
 * Purpose : Name the transaction from a function which has been specified by
 *           the user to be the name of the transaction if called.
 */
extern void nr_txn_name_from_function(nrtxn_t* txn,
                                      const char* funcname,
                                      const char* classname);

/*
 * Purpose : Ignore the current transaction and stop recording.
 */
extern void nr_txn_ignore(nrtxn_t* txn);

/*
 * Purpose : Add a custom metric from the API.
 *
 * Returns : NR_SUCCESS if the metric could be added, and NR_FAILURE otherwise.
 *
 * NOTE    : No attempt is made to vet the metric name choice of the customer:
 *           Their choice could collide with any agent metric name.
 */
extern nr_status_t nr_txn_add_custom_metric(nrtxn_t* txn,
                                            const char* name,
                                            double value_ms);

/*
 * Purpose : Checks if the transaction name matches a string
 *
 * Returns : true is the string matches, false otherwise
 */
extern bool nr_txn_is_current_path_named(const nrtxn_t* txn, const char* name);

/*
 * Purpose : Accept a base 64 encoded distributed tracing payload for the given
 *           transaction.
 *
 * Params  : 1. The transaction.
 *           2. The base64 encoded payload
 *           3. The type of transporation (e.g. "http", "https")
 *
 * Returns : true if we're able to accept the payload, false otherwise
 */
bool nr_txn_accept_distributed_trace_payload_httpsafe(
    nrtxn_t* txn,
    const char* payload,
    const char* transport_type);

/*
 * Purpose : Accept a distributed tracing payload for the given transaction.
 *
 * Params  : 1. The transaction.
 *           2. The payload
 *           3. The type of transporation (e.g. "http", "https")
 *
 * Returns : true if we're able to accept the payload, false otherwise
 */
bool nr_txn_accept_distributed_trace_payload(nrtxn_t* txn,
                                             const char* payload,
                                             const char* transport_type);

/*
 * Purpose : Create a distributed tracing payload for the given transaction.
 *
 * Params  : 1. The transaction.
 *
 * Returns : A newly allocated, null terminated payload string, which the caller
 *           must destroy with nr_free() when no longer needed, or NULL on
 *           error.
 */
extern char* nr_txn_create_distributed_trace_payload(nrtxn_t* txn);

/*
 * Purpose : Determine whether span events should be created. This is true if
 *           `distributed_tracing_enabled` and `span_events_enabled` are true
 *           and the transaction is sampled.
 *
 * Params  : 1. The transaction.
 *
 * Returns : true if span events should be created.
 */
extern bool nr_txn_should_create_span_events(const nrtxn_t* txn);

/*
 * Purpose : Get a pointer to the currently-executing segment.
 *
 * Params  : The current transaction.
 *
 * Note    : In some cases, the parent of a segment shall be supplied, as in
 *           cases of newrelic_start_segment(). In other cases, the parent of
 *           a segment shall be determined by the currently executing segment.
 *           For the second scenario, get a pointer to the active segment so
 *           that a new, parented segment may be added to the transaction.
 *
 * Returns : A pointer to the active segment.
 */
extern nr_segment_t* nr_txn_get_current_segment(nrtxn_t* txn);

/*
 * Purpose : Set the current segment for the transaction.
 *
 * Params  : 1. The current transaction.
 *           2. A pointer to the currently-executing segment.
 *
 * Note    : On the transaction is a data structure used to manage the parenting
 *           of stacks for the main context.  Currently it's implemented as a
 *           stack.  This call is equivalent to pushing a segment pointer onto
 *           the stack of parents.
 *
 */
extern void nr_txn_set_current_segment(nrtxn_t* txn, nr_segment_t* segment);

/*
 * Purpose : Retire the currently-executing segment
 *
 * Params  : The current transaction.
 *
 * Note    : On the transaction is a data structure used to manage the
 *           parenting of stacks for the main context.  Currently it's
 *           implemented as a stack.  This call is equivalent to popping
 *           a segment pointer from the stack of parents.
 */
extern void nr_txn_retire_current_segment(nrtxn_t* txn);

/*
 * Purpose : Allocate memory for the type dependant attributes of a transaction
 * trace node.
 *
 * Params : None.
 *
 * Returns : The allocated memory.
 */
extern nr_txnnode_attributes_t* nr_txnnode_attributes_create(void);

/*
 * Purpose : Assign the type dependant attributes to a trace node. This assigns
 * the node a custom type if the attributes are NULL.
 *
 * Params : 1. The node that you would like updated.
 *          2. The struct that contains the attributes you would like in the
 * node.
 */
extern void nr_txnnode_set_attributes(
    nrtxnnode_t* node,
    const nr_txnnode_attributes_t* attributes);

/*
 * Purpose : Release the memory that has been allocated.
 *
 * Params : 1. The struct that contains memory you would like released.
 */
extern void nr_txnnode_attributes_destroy(nr_txnnode_attributes_t* attributes);

#endif /* NR_TXN_HDR */
