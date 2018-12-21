#include "nr_axiom.h"

#include <locale.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "nr_agent.h"
#include "nr_commands.h"
#include "nr_custom_events.h"
#include "nr_guid.h"
#include "nr_segment.h"
#include "nr_segment_private.h"
#include "nr_slowsqls.h"
#include "nr_synthetics.h"
#include "nr_distributed_trace.h"
#include "nr_txn.h"
#include "nr_txn_private.h"
#include "node_datastore.h"
#include "util_base64.h"
#include "util_cpu.h"
#include "util_hash.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_random.h"
#include "util_reply.h"
#include "util_sampling.h"
#include "util_sql.h"
#include "util_sleep.h"
#include "util_strings.h"
#include "util_string_pool.h"
#include "util_url.h"

struct _nr_txn_attribute_t {
  const char* name;
  uint32_t destinations;
};

#define NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT                             \
  (NR_ATTRIBUTE_DESTINATION_TXN_TRACE | NR_ATTRIBUTE_DESTINATION_ERROR \
   | NR_ATTRIBUTE_DESTINATION_TXN_EVENT)

#define NR_TXN_ATTRIBUTE_TRACE_ERROR \
  (NR_ATTRIBUTE_DESTINATION_TXN_TRACE | NR_ATTRIBUTE_DESTINATION_ERROR)

#define NR_TXN_ATTR(X, NAME, DESTS) \
  const nr_txn_attribute_t* X = &(nr_txn_attribute_t) { (NAME), (DESTS) }

/*
 * Creating Events and Attributes
 * https://pages.datanerd.us/engineering-management/architecture-notes/notes/122/index.html
 *
 * Attribute Catalog
 * https://pages.datanerd.us/engineering-management/architecture-notes/notes/123/
 *
 * IMPORTANT: "All changes to events and attributes MUST be approved by
 * @hshapiro and @nic"
 *
 * Attributes Spec
 * https://source.datanerd.us/agents/agent-specs/blob/master/Agent-Attributes-PORTED.md
 *
 * Note that this list does not include all attributes:  custom user attributes
 * which are added through nr_txn_add_user_custom_parameter and request
 * parameters which are created in nr_txn_add_request_parameter.
 */

/* https://source.datanerd.us/agents/agent-specs/blob/master/Custom-Host-Names.md#attributes
 */
NR_TXN_ATTR(nr_txn_host_display_name,
            "host.displayName",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_request_accept_header,
            "request.headers.accept",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_request_content_type,
            "request.headers.contentType",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_request_content_length,
            "request.headers.contentLength",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_request_host,
            "request.headers.host",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_request_method,
            "request.method",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_request_user_agent,
            "request.headers.User-Agent",
            NR_TXN_ATTRIBUTE_TRACE_ERROR);
NR_TXN_ATTR(nr_txn_request_referer,
            "request.headers.referer",
            NR_ATTRIBUTE_DESTINATION_ERROR);
NR_TXN_ATTR(nr_txn_response_content_type,
            "response.headers.contentType",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_response_content_length,
            "response.headers.contentLength",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
/* This "SERVER_NAME" attribute is PHP specific:  It was a custom parameter
 * before attributes happened. */
NR_TXN_ATTR(nr_txn_server_name, "SERVER_NAME", NR_TXN_ATTRIBUTE_TRACE_ERROR);
NR_TXN_ATTR(nr_txn_response_code,
            "httpResponseCode",
            NR_TXN_ATTRIBUTE_TRACE_ERROR_EVENT);
NR_TXN_ATTR(nr_txn_error_message,
            "errorMessage",
            NR_ATTRIBUTE_DESTINATION_TXN_EVENT);
NR_TXN_ATTR(nr_txn_error_type, "errorType", NR_ATTRIBUTE_DESTINATION_TXN_EVENT);

void nr_txn_set_string_attribute(nrtxn_t* txn,
                                 const nr_txn_attribute_t* attribute,
                                 const char* value) {
  if (NULL == txn) {
    return;
  }
  if (NULL == attribute) {
    return;
  }
  if (NULL == value) {
    return;
  }
  if ('\0' == value[0]) {
    return;
  }
  nr_attributes_agent_add_string(txn->attributes, attribute->destinations,
                                 attribute->name, value);
}

void nr_txn_set_long_attribute(nrtxn_t* txn,
                               const nr_txn_attribute_t* attribute,
                               long value) {
  if (NULL == txn) {
    return;
  }
  if (NULL == attribute) {
    return;
  }
  nr_attributes_agent_add_long(txn->attributes, attribute->destinations,
                               attribute->name, value);
}

/* These sample options are provided for tests. */
const nrtxnopt_t nr_txn_test_options
    = {.custom_events_enabled = 0,
       .synthetics_enabled = 0,
       .instance_reporting_enabled = 1,
       .database_name_reporting_enabled = 1,
       .err_enabled = 1,
       .request_params_enabled = 0,
       .autorum_enabled = 1,
       .analytics_events_enabled = 1,
       .error_events_enabled = 1,
       .tt_enabled = 1,
       .ep_enabled = 1,
       .tt_recordsql = NR_SQL_OBFUSCATED,
       .tt_slowsql = 1,
       .apdex_t = (nrtime_t)(0.5 * NR_TIME_DIVISOR_D),
       .tt_threshold = 2 * NR_TIME_DIVISOR,
       .tt_is_apdex_f = 1,
       .ep_threshold = 500 * NR_TIME_DIVISOR_MS,
       .ss_threshold = 500 * NR_TIME_DIVISOR_MS,
       .cross_process_enabled = 1};

bool nr_txn_cmp_options(nrtxnopt_t* o1, nrtxnopt_t* o2) {
  if (o1 == o2)
    return true;
  if (o1 == NULL || o2 == NULL)
    return false;
  if (o1->custom_events_enabled != o2->custom_events_enabled)
    return false;
  if (o1->synthetics_enabled != o2->synthetics_enabled)
    return false;
  if (o1->instance_reporting_enabled != o2->instance_reporting_enabled)
    return false;
  if (o1->database_name_reporting_enabled
      != o2->database_name_reporting_enabled)
    return false;
  if (o1->err_enabled != o2->err_enabled)
    return false;
  if (o1->request_params_enabled != o2->request_params_enabled)
    return false;
  if (o1->autorum_enabled != o2->autorum_enabled)
    return false;
  if (o1->analytics_events_enabled != o2->analytics_events_enabled)
    return false;
  if (o1->error_events_enabled != o2->error_events_enabled)
    return false;
  if (o1->tt_enabled != o2->tt_enabled)
    return false;
  if (o1->ep_enabled != o2->ep_enabled)
    return false;
  if (o1->tt_recordsql != o2->tt_recordsql)
    return false;
  if (o1->tt_slowsql != o2->tt_slowsql)
    return false;
  if (o1->apdex_t != o2->apdex_t)
    return false;
  if (o1->tt_threshold != o2->tt_threshold)
    return false;
  if (o1->tt_is_apdex_f != o2->tt_is_apdex_f)
    return false;
  if (o1->ep_threshold != o2->ep_threshold)
    return false;
  if (o1->ss_threshold != o2->ss_threshold)
    return false;
  if (o1->cross_process_enabled != o2->cross_process_enabled)
    return false;
  if (o1->distributed_tracing_enabled != o2->distributed_tracing_enabled)
    return false;
  if (o1->span_events_enabled != o2->span_events_enabled)
    return false;

  return true;
}

void nr_txn_enforce_security_settings(nrtxnopt_t* opts,
                                      const nrobj_t* connect_reply,
                                      const nrobj_t* sec_policies) {
  if (NULL == opts) {
    return;
  }

  /* LASP
   * It is perfectly valid for any of the below policies to not exist
   * in the security policies object that is captured from the daemon.
   * Because of this  we return a default value of 2 indicating it
   * doesn't exist, therefore take no action as a result.
   */

  if (0 == nr_reply_get_bool(sec_policies, "record_sql", 2)) {
    opts->tt_recordsql = NR_SQL_NONE;
    nrl_verbosedebug(NRL_TXN,
                     "Setting newrelic.transaction_tracer.record_sql = \"off\" "
                     "by server security policy");
  } else if (1 == nr_reply_get_bool(sec_policies, "record_sql", 2)
             && NR_SQL_RAW == opts->tt_recordsql) {
    nrl_verbosedebug(NRL_TXN,
                     "Setting newrelic.transaction_tracer.record_sql = "
                     "\"obfuscated\" by server security policy");
    opts->tt_recordsql = NR_SQL_OBFUSCATED;
  }

  if (0 == nr_reply_get_bool(sec_policies, "allow_raw_exception_messages", 2)) {
    opts->allow_raw_exception_messages = 0;
  }

  if (0 == nr_reply_get_bool(sec_policies, "custom_events", 2)) {
    opts->custom_events_enabled = 0;
    nrl_verbosedebug(NRL_TXN,
                     "Setting newrelic.custom_insights_events.enabled = false "
                     "by server security policy");
  }

  if (0 == nr_reply_get_bool(sec_policies, "custom_parameters", 2)) {
    opts->custom_parameters_enabled = 0;
  }

  /* Account level controlled fields
   * Check if these values are more secure than the local config. This
   * happens after LASPso any relevant debug messages get seen by the customer
   */

  if (0 == nr_reply_get_bool(connect_reply, "collect_analytics_events", 1)) {
    opts->analytics_events_enabled = 0;
    nrl_verbosedebug(
        NRL_TXN, "Setting newrelic.analytics_events.enabled = false by server");
  }

  // LASP also modifies this setting. Kept seperate for readability.
  if (0 == nr_reply_get_bool(connect_reply, "collect_custom_events", 1)) {
    opts->custom_events_enabled = 0;
    nrl_verbosedebug(
        NRL_TXN,
        "Setting newrelic.custom_insights_events.enabled = false by server");
  }

  if (0 == nr_reply_get_bool(connect_reply, "collect_traces", 0)) {
    opts->tt_enabled = 0;
    opts->ep_enabled = 0;
    opts->tt_slowsql = 0;
    nrl_verbosedebug(
        NRL_TXN,
        "Setting newrelic.transaction_tracer.enabled = false by server");
    nrl_verbosedebug(NRL_TXN,
                     "Setting newrelic.transaction_tracer.explain_enabled = "
                     "false by server");
    nrl_verbosedebug(
        NRL_TXN,
        "Setting newrelic.transaction_tracer.slow_sql = false by server");
  }

  if (0 == nr_reply_get_bool(connect_reply, "collect_errors", 0)) {
    opts->err_enabled = 0;
    nrl_verbosedebug(
        NRL_TXN, "Setting newrelic.error_collector.enabled = false by server");
  }

  if (0 == nr_reply_get_bool(connect_reply, "collect_error_events", 1)) {
    opts->error_events_enabled = 0;
    nrl_verbosedebug(
        NRL_TXN,
        "Setting newrelic.error_collector.capture_events = false by server");
  }
}

static inline void nr_txn_create_dt_metrics(nrtxn_t* txn,
                                            const char* metric_prefix,
                                            int value) {
  char* all_metric;
  char* all_web_other_metric;
  char* metric_name;
  const char* metric_postfix;

  if (NULL == txn) {
    return;
  }

  if (NULL == metric_prefix) {
    return;
  }

  metric_postfix = txn->status.background ? "allOther" : "allWeb";

  if (NULL != txn->distributed_trace
      && nr_distributed_trace_inbound_is_set(txn->distributed_trace)) {
    metric_name = nr_formatf(
        "%s/%s/%s/%s",
        nr_distributed_trace_inbound_get_type(txn->distributed_trace),
        nr_distributed_trace_inbound_get_account_id(txn->distributed_trace),
        nr_distributed_trace_inbound_get_app_id(txn->distributed_trace),
        nr_distributed_trace_inbound_get_transport_type(
            txn->distributed_trace));
  } else {
    metric_name = nr_strdup("Unknown/Unknown/Unknown/Unknown");
  }

  all_metric = nr_formatf("%s/%s/all", metric_prefix, metric_name);
  all_web_other_metric
      = nr_formatf("%s/%s/%s", metric_prefix, metric_name, metric_postfix);

  nrm_force_add(txn->unscoped_metrics, all_metric, value);
  nrm_force_add(txn->unscoped_metrics, all_web_other_metric, value);

  nr_free(all_metric);
  nr_free(all_web_other_metric);
  nr_free(metric_name);
}

nrtxn_t* nr_txn_begin(nrapp_t* app,
                      const nrtxnopt_t* opts,
                      const nr_attribute_config_t* attribute_config) {
  nrtxn_t* nt;
  char* guid;
  nr_status_t err = 0;
  nr_sampling_priority_t priority;

  if (0 == app) {
    return 0;
  }

  if (NR_APP_OK != app->state) {
    return 0;
  }

  if (NULL == opts) {
    return NULL;
  }

  nt = (nrtxn_t*)nr_zalloc(sizeof(nrtxn_t));
  nt->status.path_is_frozen = 0;
  nt->status.path_type = NR_PATH_TYPE_UNKNOWN;
  nt->agent_run_id = nr_strdup(app->agent_run_id);
  nt->rnd = app->rnd;

  /*
   * Allocate the transaction-global string pools.
   */
  nt->trace_strings = nr_string_pool_create();

  nr_memcpy(&nt->options, opts, sizeof(nrtxnopt_t));

  nt->options.apdex_t
      = (nrtime_t)(nr_reply_get_double(app->connect_reply, "apdex_t", 0.5)
                   * NR_TIME_DIVISOR_D);

  if (nt->options.tt_is_apdex_f) {
    nt->options.tt_threshold = 4 * nt->options.apdex_t;
  }

#define NR_TXN_MAX_SLOWSQLS 10
  nt->slowsqls = nr_slowsqls_create(NR_TXN_MAX_SLOWSQLS);
  nt->datastore_products = nr_string_pool_create();
  nt->unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  nt->scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  nt->attributes = nr_attributes_create(attribute_config);
  nt->intrinsics = nro_new_hash();
  nt->root_kids_duration = 0;
  nt->cur_kids_duration = &nt->root_kids_duration;

#define NR_TXN_MAX_CUSTOM_EVENTS (10 * 1000)
  nt->custom_events = nr_analytics_events_create(NR_TXN_MAX_CUSTOM_EVENTS);

  /*
   * Enforce SSC and LASP if enabled
   */
  nr_txn_enforce_security_settings(&nt->options, app->connect_reply,
                                   app->security_policies);

  /*
   * Allocate the stack to manage segment parenting
   */
  nr_stack_init(&nt->parent_stack, NR_STACK_DEFAULT_CAPACITY);

  /*
   * Install the root segment
   */
  nt->segment_root = nr_zalloc(sizeof(nr_segment_t));
  nr_segment_children_init(&nt->segment_root->children);
  nt->segment_root->start_time = nr_get_time();
  nr_txn_set_current_segment(nt, nt->segment_root);
  nt->segment_count = 1;

  /*
   * And finally set in the status member those values that can be changed
   * during the transaction but which start out with the original option.
   */
  nt->status.ignore_apdex = 0;
  if (nt->options.cross_process_enabled) {
    nt->status.cross_process = NR_STATUS_CROSS_PROCESS_START;
  } else {
    nt->status.cross_process = NR_STATUS_CROSS_PROCESS_DISABLED;
  }
  nt->status.recording = 1;

  nr_txn_set_time(nt, &nt->root.start_time);

  nr_get_cpu_usage(&nt->user_cpu[NR_CPU_USAGE_START],
                   &nt->sys_cpu[NR_CPU_USAGE_START]);

  nt->license = nr_strdup(app->info.license);

  nt->app_connect_reply = nro_copy(app->connect_reply);
  nt->primary_app_name = nr_txn_get_primary_app_name(app->info.appname);

  nt->cat.alternate_path_hashes = nro_new_hash();

  if (app->info.high_security) {
    nt->high_security = 1;
  }

  if (NULL != app->info.security_policies_token
      && '\0' != app->info.security_policies_token[0]) {
    nt->lasp = 1;
    nt->options.request_params_enabled = 0;  // Force disabled
  }

  nr_txn_set_string_attribute(nt, nr_txn_host_display_name,
                              app->info.host_display_name);

  nt->distributed_trace = nr_distributed_trace_create();

  /*
   * Per the spec: The trace id is constant for the entire trip. Its value is
   * equal to the guid of the first span in the trip (this is the id of root
   * span of the transaction, which equals the transaction id).
   *
   * The trace id will be overwritten by accepting an inbound DT
   * payload.
   */
  guid = nr_guid_create(app->rnd);
  nr_distributed_trace_set_txn_id(nt->distributed_trace, guid);
  nr_distributed_trace_set_trace_id(nt->distributed_trace, guid);

  nr_distributed_trace_set_trusted_key(
      nt->distributed_trace,
      nro_get_hash_string(nt->app_connect_reply, "trusted_account_key", &err));
  nr_distributed_trace_set_account_id(
      nt->distributed_trace,
      nro_get_hash_string(nt->app_connect_reply, "account_id", &err));
  nr_distributed_trace_set_app_id(
      nt->distributed_trace,
      nro_get_hash_string(nt->app_connect_reply, "primary_application_id",
                          &err));

  priority = nr_generate_initial_priority(app->rnd);
  if (nr_app_harvest_should_sample(&app->harvest, app->rnd)) {
    nr_distributed_trace_set_sampled(nt->distributed_trace, true);
    priority += 1.0;
  }
  nr_distributed_trace_set_priority(nt->distributed_trace, priority);

  nr_free(guid);

  return nt;
}

void nr_txn_node_dispose_fields(nrtxnnode_t* node) {
  if (nrunlikely(0 == node)) {
    return;
  }

  nro_delete(node->data_hash);
  nr_txnnode_attributes_destroy(node->attributes);
  nr_free(node->id);
  nr_memset(node, 0, sizeof(*node));
}

/*
 * This function returns 1 if A is faster, and 0 otherwise.
 * Faster nodes should be replaced before slower nodes.
 */
static inline int pq_data_compare(const nrtxnnode_t* a, const nrtxnnode_t* b) {
  nrtime_t duration_a = a->stop_time.when - a->start_time.when;
  nrtime_t duration_b = b->stop_time.when - b->start_time.when;

  return duration_a < duration_b;
}

int nr_txn_pq_data_compare_wrapper(const nrtxnnode_t* a, const nrtxnnode_t* b) {
  return pq_data_compare(a, b);
}

nrtxnnode_t* nr_txn_save_if_slow_enough(nrtxn_t* txn, nrtime_t duration) {
  int n;
  int parent;
  int priority_child;
  nrtxnnode_t* node;

  if (0 == txn) {
    return 0;
  }

  /*
   * Does the TT have room for a new node?
   */
  if (txn->nodes_used < NR_TXN_NODE_LIMIT) {
    node = txn->nodes + txn->nodes_used;

    txn->nodes_used++;
    node->start_time.when = 0;
    node->stop_time.when = duration;

    n = txn->nodes_used;
    parent = n / 2;
    while (parent && pq_data_compare(node, txn->pq[parent])) {
      txn->pq[n] = txn->pq[parent];
      n /= 2;
      parent /= 2;
    }
    txn->pq[n] = node;
    return node;
  }

  /*
   * If the TT is full of nodes, we replace an existing node
   * if the new node is slower.
   */
  {
    nrtxnnode_t fakenode;

    fakenode.start_time.when = 0;
    fakenode.stop_time.when = duration;

    if (0 == pq_data_compare(txn->pq[1], &fakenode)) {
      return 0;
    }
  }

  node = txn->pq[1];
  nr_txn_node_dispose_fields(node);
  node->start_time.when = 0;
  node->stop_time.when = duration;

  for (n = 1;;) {
    int childl = n * 2;
    int childr = childl + 1;

    /* no children */
    if (childl > NR_TXN_NODE_LIMIT) {
      break;
    }

    /* one child */
    if (childr > NR_TXN_NODE_LIMIT) {
      if (pq_data_compare(txn->pq[childl], node)) {
        txn->pq[n] = txn->pq[childl];
        n = childl;
        break;
      } else {
        break;
      }
    }

    /* two children */
    if (pq_data_compare(txn->pq[childr], txn->pq[childl])) {
      priority_child = childr;
    } else {
      priority_child = childl;
    }
    if (pq_data_compare(node, txn->pq[priority_child])) {
      break;
    }

    txn->pq[n] = txn->pq[priority_child];
    n = priority_child;
  }
  txn->pq[n] = node;
  return node;
}

void nr_txn_save_trace_node(nrtxn_t* txn,
                            const nrtxntime_t* start,
                            const nrtxntime_t* stop,
                            const char* name,
                            const char* async_context,
                            const nrobj_t* data_hash,
                            const nr_txnnode_attributes_t* attributes) {
  nrtime_t duration;
  nrtxnnode_t* node;

  if ((0 == txn) || (0 == start) || (0 == stop) || (0 == name)) {
    return;
  }

  if (0 == txn->status.recording) {
    return;
  }
  if (0 == txn->options.tt_enabled) {
    return;
  }
  if (stop->when < start->when) {
    return;
  }
  if (stop->stamp <= start->stamp) {
    return;
  }

  duration = stop->when - start->when;

  node = nr_txn_save_if_slow_enough(txn, duration);

  if (0 == node) {
    return;
  }
  txn->last_added = node;

  nr_txn_copy_time(start, &node->start_time);
  nr_txn_copy_time(stop, &node->stop_time);
  node->count = 0;
  node->name = nr_string_add(txn->trace_strings, name);
  if (data_hash) {
    node->data_hash = nro_copy(data_hash);
  } else {
    node->data_hash = 0;
  }
  nr_txnnode_set_attributes(node, attributes);

  if (NULL != async_context) {
    /* Async node */
    node->async_context = nr_string_add(txn->trace_strings, async_context);
  } else {
    /* Regular node */
    node->async_context = 0;
  }

  /*
   * See comments in nr_txn_create_distributed_trace_payload for an
   * explanation of this mechanism.
   *
   * txn->current_node_id and txn->current_node_time are set during outbound DT
   * payload generation. A sanity check is done to verify, that the timestamp
   * of the DT payload creation falls between the start and the end time of
   * this trace node.
   */
  if (txn->current_node_id && NULL != attributes
      && NR_EXTERNAL == attributes->type) {
    node->id = txn->current_node_id;
    txn->current_node_id = NULL;
  } else {
    node->id = NULL;
  }
}

/*
 * Purpose : Apply url_rules to the transaction's path.  This should occur
 *           before the path is used to create the full metric name.
 *
 * Returns : NR_FAILURE if the transaction should be ignored and NR_SUCCESS
 *           otherwise.
 */
static nr_status_t nr_txn_apply_url_rules(nrtxn_t* txn,
                                          const nrrules_t* rules) {
  char path_before[512];
  nr_status_t ret;
  nr_rules_result_t rv;
  char* output = 0;

  if (nrunlikely((0 == txn) || (0 == rules) || (0 == txn->path))) {
    return NR_SUCCESS;
  }

  /*
   * Copy the path onto the stack.  The path should start with "/" before
   * rules are applied.
   */
  path_before[0] = '\0';
  snprintf(path_before, sizeof(path_before), "%s%s",
           ('/' == txn->path[0]) ? "" : "/", txn->path);

  rv = nr_rules_apply(rules, path_before, &output);

  if (NR_RULES_RESULT_IGNORE == rv) {
    txn->status.ignore = 1;
    ret = NR_FAILURE;
  } else if (NR_RULES_RESULT_CHANGED == rv) {
    nr_free(txn->path);
    txn->path = nr_strdup(output);
    ret = NR_SUCCESS;
  } else {
    /* NR_RULES_RESULT_UNCHANGED */
    ret = NR_SUCCESS;
  }

  nr_free(output);
  nrl_verbosedebug(
      NRL_RULES, "url rules: ignore=%d before=" NRP_FMT " after=" NRP_FMT,
      txn->status.ignore, 150, path_before, NRP_TXNNAME(txn->path));

  return ret;
}

/*
 * Purpose : Apply transaction_name_rules to the transaction's full metric name.
 *
 * Returns : NR_FAILURE if the transaction should be ignored and NR_SUCCESS
 *           otherwise.
 */
static nr_status_t nr_txn_apply_txn_rules(nrtxn_t* txn,
                                          const nrrules_t* rules) {
  nr_rules_result_t rv;
  nr_status_t ret;
  char txnname_before[512];
  char* output = 0;

  if ((0 == txn) || (0 == rules) || (0 == txn->name)) {
    return NR_SUCCESS;
  }

  txnname_before[0] = '\0';
  snprintf(txnname_before, sizeof(txnname_before), "%s", txn->name);

  rv = nr_rules_apply(rules, txnname_before, &output);

  if (NR_RULES_RESULT_IGNORE == rv) {
    txn->status.ignore = 1;
    ret = NR_FAILURE;
  } else if (NR_RULES_RESULT_UNCHANGED == rv) {
    ret = NR_SUCCESS;
  } else {
    /* NR_RULES_RESULT_CHANGED */
    nr_free(txn->name);
    txn->name = nr_strdup(output);
    ret = NR_SUCCESS;
  }

  nr_free(output);
  nrl_verbosedebug(NRL_RULES,
                   "txn rules: ignore=%d before=" NRP_FMT " after=" NRP_FMT,
                   NR_RULES_RESULT_IGNORE == rv, 150, txnname_before,
                   NRP_TXNNAME(txn->name));

  return ret;
}

static void nr_txn_apply_segment_terms(nrtxn_t* txn,
                                       const nr_segment_terms_t* terms) {
  char* name;

  if ((NULL == txn) || (NULL == txn->name)) {
    return;
  }

  name = nr_segment_terms_apply(terms, txn->name);
  if (name) {
    nr_free(txn->name);
    txn->name = name;
  }
}

static int nr_txn_should_do_url_rules(int path_type, int is_background) {
  if (is_background) {
    return 0;
  }

  switch (path_type) {
    case NR_PATH_TYPE_CUSTOM:
    case NR_PATH_TYPE_URI:
      return 1;

    case NR_PATH_TYPE_ACTION:
    case NR_PATH_TYPE_FUNCTION:
    case NR_PATH_TYPE_UNKNOWN:
    default:
      return 0;
  }
}

static void nr_txn_update_apdex_if_key_txn(nrtxn_t* txn) {
  double db = 0.0;
  const nrobj_t* key_txns;

  if (0 == txn) {
    return;
  }

  key_txns
      = nro_get_hash_hash(txn->app_connect_reply, "web_transactions_apdex", 0);
  if (0 == key_txns) {
    return;
  }

  db = nr_reply_get_double(key_txns, txn->name, (double)-1);
  if (db < 0) {
    return;
  }

  txn->options.apdex_t = (nrtime_t)(db * NR_TIME_DIVISOR);
  nrl_verbosedebug(NRL_TXN, "key txn: " NRP_FMT ": new apdex=" NR_TIME_FMT,
                   NRP_TXNNAME(txn->name), txn->options.apdex_t);

  /*
   * If the tt_threshold is based off of apdex, then it must be updated
   * Note that if the threshold was not the default value of 'apdex', then
   * it remains unchanged.
   */
  if (txn->options.tt_is_apdex_f) {
    txn->options.tt_threshold = 4 * txn->options.apdex_t;
  }
}

nr_status_t nr_txn_freeze_name_update_apdex(nrtxn_t* txn) {
  nrapp_t* app = 0;
  const char* name = 0;   /* Txn name, used for metric and scope */
  const char* prefix = 0; /* Txn name prefix */

  if (nrunlikely((0 == txn) || (0 != txn->status.ignore))) {
    return NR_FAILURE;
  }

  if (0 != txn->status.path_is_frozen) {
    return NR_SUCCESS;
  }

  /*
   * This prevents anything from changing the Web Transaction name.
   */
  txn->status.path_is_frozen = 1;
  nrl_debug(NRL_TXN, "txn naming freeze");

  switch (txn->status.path_type) {
    case NR_PATH_TYPE_URI:
      if (nrunlikely(txn->status.background)) {
        prefix = "OtherTransaction/php/";
      } else {
        prefix = "WebTransaction/Uri/";
      }
      break;

    case NR_PATH_TYPE_ACTION:
      if (nrunlikely(txn->status.background)) {
        prefix = "OtherTransaction/Action/";
      } else {
        prefix = "WebTransaction/Action/";
      }
      break;

    case NR_PATH_TYPE_FUNCTION:
      if (nrunlikely(txn->status.background)) {
        prefix = "OtherTransaction/Function/";
      } else {
        prefix = "WebTransaction/Function/";
      }
      break;

    case NR_PATH_TYPE_CUSTOM:
      if (nrunlikely(txn->status.background)) {
        prefix = "OtherTransaction/Custom/";
      } else {
        prefix = "WebTransaction/Custom/";
      }
      break;

    case NR_PATH_TYPE_UNKNOWN:
    default:
      if (nrunlikely(txn->status.background)) {
        name = "OtherTransaction/php/<unknown>";
      } else {
        name = "WebTransaction/Uri/<unknown>";
      }
      prefix = 0;
      break;
  }

  /*
   * Lock the application to use its url_rules, txn_rules and segment_terms.
   *
   * This is the only point in time (other than txn's start) that the
   * transaction has to access the application.  It would be nice to remove
   * this entirely:  Perhaps the url_rules, txn_rules and segment_terms could
   * be copied into the txn at its start.  Unfortunately, this approach might
   * require compiling the rules for each transaction, which may be costly.
   */
  app = nr_app_verify_id(nr_agent_applist, txn->agent_run_id);
  if (0 == app) {
    return NR_FAILURE;
  }

  /*
   * If there is a path, apply the url_rules (for non-background CUSTOM and URI)
   * and get the result.
   */
  if (txn->path) {
    if (nr_txn_should_do_url_rules(txn->status.path_type,
                                   txn->status.background)) {
      if (NR_FAILURE == nr_txn_apply_url_rules(txn, app->url_rules)) {
        goto ignore;
      }
    }

    if (txn->path && ('/' == txn->path[0])) {
      char* newpath = nr_strdup(txn->path + 1);

      nr_free(txn->path);
      txn->path = newpath;
    }
  }

  /*
   * Create the full transaction name using the prefix and path.
   */
  if (prefix) {
    const char* path = txn->path ? txn->path : "unknown";
    char* buf = (char*)nr_alloca(nr_strlen(prefix) + nr_strlen(path) + 2);
    char* bp = nr_strcpy(buf, prefix);

    nr_strcpy(bp, path);
    name = buf;
  }

  /*
   * Apply the txn_rules to the full transaction name and store the results.
   * It is possible that a txn_rule tells us to ignore the transaction
   * completely. This call will store the result in the txn's string pool to
   * be used in the RUM footer and in metrics at the end of the request.
   */
  nr_free(txn->name);
  txn->name = nr_strdup(name);
  if (NR_FAILURE == nr_txn_apply_txn_rules(txn, app->txn_rules)) {
    goto ignore;
  }

  /*
   * Apply any transaction segment terms to the transaction name.
   */
  nr_txn_apply_segment_terms(txn, app->segment_terms);

  nrt_mutex_unlock(&app->app_lock);
  app = 0;

  nr_txn_update_apdex_if_key_txn(txn);

  return NR_SUCCESS;

ignore:
  if (app) {
    nrt_mutex_unlock(&app->app_lock);
  }
  return NR_FAILURE;
}

static char* nr_txn_replace_first_segment(const char* txnname,
                                          const char* new_prefix) {
  const char* slash = nr_strchr(txnname, '/');

  if (NULL == slash) {
    return NULL;
  }

  return nr_formatf("%s%s", new_prefix, slash);
}

void nr_txn_create_apdex_metrics(nrtxn_t* txn, nrtime_t duration) {
  int satisfying = 0;
  int tolerating = 0;
  int failing = 0;
  char* apdex_metric = 0;
  nrtime_t apdex;
  nr_apdex_zone_t apdex_zone;

  if (nrunlikely(0 == txn)) {
    return;
  }

  apdex = txn->options.apdex_t;
  apdex_zone = nr_txn_apdex_zone(txn, duration);

  if (NR_APDEX_SATISFYING == apdex_zone) {
    satisfying += 1;
  } else if (NR_APDEX_TOLERATING == apdex_zone) {
    tolerating += 1;
  } else {
    failing += 1;
  }

  nrm_force_add_apdex(txn->unscoped_metrics, "Apdex", satisfying, tolerating,
                      failing, apdex);

  apdex_metric = nr_txn_replace_first_segment(txn->name, "Apdex");
  if (NULL == apdex_metric) {
    return;
  }

  nrm_add_apdex(txn->unscoped_metrics, apdex_metric, satisfying, tolerating,
                failing, apdex);
  nr_free(apdex_metric);
}

void nr_txn_create_error_metrics(nrtxn_t* txn, const char* txnname) {
  int nl;
  const char* eprefix = "Errors/";
  char* buf;
  char* bp;

  if (nrunlikely((0 == txn) || (0 == txnname) || (0 == txnname[0]))) {
    return;
  }

  nrm_force_add(txn->unscoped_metrics, "Errors/all", 0);

  if (txn->status.background) {
    nrm_force_add(txn->unscoped_metrics, "Errors/allOther", 0);
  } else {
    nrm_force_add(txn->unscoped_metrics, "Errors/allWeb", 0);
  }

  if (txn->options.distributed_tracing_enabled) {
    nr_txn_create_dt_metrics(txn, "ErrorsByCaller", 0);
  }

  nl = nr_strlen(txnname);
  buf = (char*)nr_alloca(nl + 8);
  bp = nr_strcpy(buf, eprefix);

  nr_strcpy(bp, txnname);

  nrm_force_add(txn->unscoped_metrics, buf, 0);
}

#define TOTAL_TIME_SUFFIX "TotalTime"
void nr_txn_create_duration_metrics(nrtxn_t* txn,
                                    const char* txnname,
                                    nrtime_t duration) {
  nrtime_t exclusive = 0;
  nrtime_t total_duration = 0;
  const char* rollup_metric = NULL;
  const char* rollup_total_metric = NULL;
  const char* txnname_slash = NULL;
  char* total_metric = NULL;

  if (nrunlikely((0 == txn) || (0 == txnname) || (0 == txnname[0]))) {
    return;
  }

  if (txn->status.background) {
    rollup_metric = "OtherTransaction/all";
    rollup_total_metric = "OtherTransaction" TOTAL_TIME_SUFFIX;
  } else {
    rollup_metric = "WebTransaction";
    rollup_total_metric = "WebTransaction" TOTAL_TIME_SUFFIX;

    /*
     * "HttpDispatcher" metric is used for the overview graph, and therefore
     * should only be made for web transactions.
     */
    nrm_force_add_ex(txn->unscoped_metrics, "HttpDispatcher", duration, 0);
  }

  /* Name the total time version of the Web/Other transaction name. */
  txnname_slash = nr_strchr(txnname, '/');

  if (NULL == txnname_slash) {
    total_metric = nr_formatf("%s%s", txnname, TOTAL_TIME_SUFFIX);
  } else {
    total_metric = nr_formatf("%.*s%s%s", (int)(txnname_slash - txnname),
                              txnname, TOTAL_TIME_SUFFIX, txnname_slash);
  }

  /* Calculate exclusive time for transaction metric */
  if (duration > txn->root_kids_duration) {
    exclusive = duration - txn->root_kids_duration;
  }

  /*
   * For Total metrics, the exclusive field should match the total field.
   *
   * https://newrelic.atlassian.net/wiki/display/eng/Reporting+asynchronous+transactions
   *
   * NOTE: Here we use the async_duration only for the web transaction not for
   * the rollup:  It is only being used for Guzzle, which is not truly
   * asynchronous, and therefore we want the overview charts to be synchronous.
   */
  total_duration = duration + txn->async_duration;

  nrm_force_add_ex(txn->unscoped_metrics, txnname, duration, exclusive);
  nrm_force_add_ex(txn->unscoped_metrics, total_metric, total_duration,
                   total_duration);
  nrm_force_add_ex(txn->unscoped_metrics, rollup_metric, duration, exclusive);
  nrm_force_add_ex(txn->unscoped_metrics, rollup_total_metric, total_duration,
                   total_duration);

  if (txn->options.distributed_tracing_enabled) {
    nr_txn_create_dt_metrics(txn, "DurationByCaller", duration);
  }

  nro_set_hash_double(txn->intrinsics, "totalTime",
                      ((double)total_duration) / NR_TIME_DIVISOR_D);

  nr_free(total_metric);
}

void nr_txn_create_queue_metric(nrtxn_t* txn) {
  nrtime_t qwait = 0;

  if (NULL == txn) {
    return;
  }

  if (txn->status.background) {
    /* Background transaction should not have queue metrics. */
    return;
  }

  if (0 == txn->status.http_x_start) {
    /* No queue start time has been added. */
    return;
  }

  if (txn->status.http_x_start > txn->root.start_time.when) {
    nrl_verbosedebug(NRL_TXN,
                     "X-Request-Start is in the future: " NR_TIME_FMT
                     " vs " NR_TIME_FMT,
                     txn->status.http_x_start, txn->root.start_time.when);
  }

  /*
   * NOTE:  A queue time metric is created even if the value is zero.
   * Therefore, the count field of this metric will reflect the number
   * of transactions which have received a queue start header, regardless
   * of the time value in the header.
   */
  qwait = nr_txn_queue_time(txn);
  nrm_force_add(txn->unscoped_metrics, "WebFrontend/QueueTime", qwait);
}

/*
 * Axiom is pioneering! Other agents report only a single value, called
 * cpu_time. However, it is useful for the user to have this split out
 * into user and system time. The core app has not caught up yet, but will in
 * the near future. So we actually add three parameters: cpu_time, which is the
 * sum of cpu_user_time and cpu_sys_time. Until the UI shows user and system
 * time differently it will at least use the rolled up value and display it as
 * "CPU burn" in the transaction trace details.
 */
static void nr_txn_create_cpu_metrics(nrtxn_t* txn) {
  nrtime_t user;
  nrtime_t sys;
  nrtime_t combined;

  if (nrunlikely(0 == txn)) {
    return;
  }

  if (txn->user_cpu[NR_CPU_USAGE_END] > txn->user_cpu[NR_CPU_USAGE_START]) {
    user = txn->user_cpu[NR_CPU_USAGE_END] - txn->user_cpu[NR_CPU_USAGE_START];
  } else {
    user = 0;
  }

  if (txn->sys_cpu[NR_CPU_USAGE_END] > txn->sys_cpu[NR_CPU_USAGE_START]) {
    sys = txn->sys_cpu[NR_CPU_USAGE_END] - txn->sys_cpu[NR_CPU_USAGE_START];
  } else {
    sys = 0;
  }

  combined = user + sys;

  {
    double cpu_time = (double)((double)combined / NR_TIME_DIVISOR_D);
    double cpu_user_time = (double)((double)user / NR_TIME_DIVISOR_D);
    double cpu_sys_time = (double)((double)sys / NR_TIME_DIVISOR_D);

    nro_set_hash_double(txn->intrinsics, "cpu_time", cpu_time);
    nro_set_hash_double(txn->intrinsics, "cpu_user_time", cpu_user_time);
    nro_set_hash_double(txn->intrinsics, "cpu_sys_time", cpu_sys_time);
  }
}

static void nr_txn_add_datastore_rollup_metric(const char* name,
                                               int len,
                                               nrtxn_t* txn) {
  char* dest;
  char* src;

  if (NULL == txn) {
    nrl_verbosedebug(NRL_TXN, "%s: NULL txn", __func__);
    return;
  }

  if (NULL == name) {
    nrl_verbosedebug(NRL_TXN, "%s: NULL datastore name", __func__);
    return;
  }

  src = nr_formatf("Datastore/%.*s/all", len, name);
  if (txn->status.background) {
    dest = nr_formatf("Datastore/%.*s/allOther", len, name);
  } else {
    dest = nr_formatf("Datastore/%.*s/allWeb", len, name);
  }

  nrm_duplicate_metric(txn->unscoped_metrics, src, dest);
  nr_free(dest);
  nr_free(src);
}

void nr_txn_create_rollup_metrics(nrtxn_t* txn) {
  if (NULL == txn) {
    return;
  }

  /*
   * Note: These rollup metrics are created here, rather than in the end_node_*
   * functions since the status.background field may change during the course
   * of the transaction.
   */
  if (txn->status.background) {
    nrm_duplicate_metric(txn->unscoped_metrics, "Datastore/all",
                         "Datastore/allOther");
    nrm_duplicate_metric(txn->unscoped_metrics, "External/all",
                         "External/allOther");
  } else {
    nrm_duplicate_metric(txn->unscoped_metrics, "Datastore/all",
                         "Datastore/allWeb");
    nrm_duplicate_metric(txn->unscoped_metrics, "External/all",
                         "External/allWeb");
  }

  nr_string_pool_apply(
      txn->datastore_products,
      (nr_string_pool_apply_func_t)nr_txn_add_datastore_rollup_metric,
      (void*)txn);
}

void nr_txn_destroy_fields(nrtxn_t* txn) {
  int i;

  nr_analytics_events_destroy(&txn->custom_events);
  nr_attributes_destroy(&txn->attributes);
  nro_delete(txn->intrinsics);
  nr_string_pool_destroy(&txn->datastore_products);
  nr_slowsqls_destroy(&txn->slowsqls);
  nr_error_destroy(&txn->error);
  nr_distributed_trace_destroy(&txn->distributed_trace);
  nr_segment_destroy(txn->segment_root);
  nr_stack_destroy_fields(&txn->parent_stack);
  nrm_table_destroy(&txn->unscoped_metrics);
  nrm_table_destroy(&txn->scoped_metrics);
  nr_string_pool_destroy(&txn->trace_strings);
  nr_file_namer_destroy(&txn->match_filenames);

  for (i = 0; i < txn->nodes_used; i++) {
    nr_txn_node_dispose_fields(&txn->nodes[i]);
  }

  nr_free(txn->license);

  nr_free(txn->request_uri);
  nr_free(txn->path);
  nr_free(txn->name);
  nr_free(txn->agent_run_id);
  nr_free(txn->current_node_id);

  nr_free(txn->cat.inbound_guid);
  nr_free(txn->cat.trip_id);
  nr_free(txn->cat.referring_path_hash);
  nro_delete(txn->cat.alternate_path_hashes);
  nr_free(txn->cat.client_cross_process_id);

  nro_delete(txn->app_connect_reply);
  nr_free(txn->primary_app_name);
  nr_synthetics_destroy(&txn->synthetics);
}

void nr_txn_destroy(nrtxn_t** txnptr) {
  if ((0 == txnptr) || (0 == *txnptr)) {
    return;
  }

  nr_txn_destroy_fields(*txnptr);

  nr_realfree((void**)txnptr);
}

nrtime_t nr_txn_duration(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }
  return nr_time_duration(txn->root.start_time.when, txn->root.stop_time.when);
}

nrtime_t nr_txn_unfinished_duration(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }
  return nr_time_duration(txn->root.start_time.when, nr_get_time());
}

void nr_txn_add_error_attributes(nrtxn_t* txn) {
  if (NULL == txn) {
    return;
  }
  if (NULL == txn->error) {
    return;
  }
  nr_txn_set_string_attribute(txn, nr_txn_error_message,
                              nr_error_get_message(txn->error));
  nr_txn_set_string_attribute(txn, nr_txn_error_type,
                              nr_error_get_klass(txn->error));
}

int nr_txn_should_create_apdex_metrics(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }

  if (txn->status.ignore_apdex) {
    return 0;
  }

  if (txn->status.background) {
    /*
     * Currently, background txns do not create apdex metrics.
     * This may change if/once we implement this spec:
     * https://newrelic.atlassian.net/wiki/display/eng/OtherTransactions+as+Key+Transactions
     */
    return 0;
  }

  return 1;
}

void nr_txn_end(nrtxn_t* txn) {
  nrtime_t duration;

  if (0 == txn) {
    return;
  }

  txn->status.recording = 0;

  if (txn->root.stop_time.when) {
    /* The txn has already been stopped. */
    return;
  }

  if (txn->status.ignore) {
    return;
  }
  if (NR_SUCCESS != nr_txn_freeze_name_update_apdex(txn)) {
    return;
  }

  /*
   * Populate the root node's with the txn's name for use in creating
   * the transaction trace json.
   */
  txn->root.name = nr_string_add(txn->trace_strings, txn->name);

  nr_get_cpu_usage(&txn->user_cpu[NR_CPU_USAGE_END],
                   &txn->sys_cpu[NR_CPU_USAGE_END]);
  nr_txn_set_time(txn, &txn->root.stop_time);

  duration = nr_txn_duration(txn);

  /* Similarly, for the segment root node, set its name and stop time,
   * needed for creating the transaction trace json. */
  txn->segment_root->name = txn->root.name;
  txn->segment_root->stop_time = nr_get_time();

#ifdef NR_CAGENT
  /* The number of nodes_used is one of the characteristics
   * used to determine whether a transaction is trace-worthy.
   * In the C agent, the segment_count is used to keep track
   * of the number of segments.  In such a case, at the end
   * of the transaction, update the nodes_used to correctly
   * reflect this characteristic */
  txn->nodes_used = txn->segment_count;
#endif

  /*
   * Create the transaction, rollup, and queue metrics. Done in a separate
   * functions to facilitate testing.
   */
  nr_txn_create_rollup_metrics(txn);
  nr_txn_create_duration_metrics(txn, txn->name, duration);
  nr_txn_create_queue_metric(txn);

  /*
   * Add the CPU times to the agent_customparams hash.
   */
  nr_txn_create_cpu_metrics(txn);

  /*
   * Add CAT intrinsics.
   */
  nr_txn_add_cat_intrinsics(txn, txn->intrinsics);

  /*
   * Add Distributed Tracing intrinsics to the transaction,
   * these will propagate to the transaction traces and
   * error data
   */
  if (txn->options.distributed_tracing_enabled) {
    nr_txn_add_distributed_tracing_intrinsics(txn, txn->intrinsics);
  }

  /*
   * Add synthetics intrinsics.
   */
  if (txn->synthetics) {
    nro_set_hash_string(txn->intrinsics, "synthetics_resource_id",
                        nr_synthetics_resource_id(txn->synthetics));
    nro_set_hash_string(txn->intrinsics, "synthetics_job_id",
                        nr_synthetics_job_id(txn->synthetics));
    nro_set_hash_string(txn->intrinsics, "synthetics_monitor_id",
                        nr_synthetics_monitor_id(txn->synthetics));
  }

  /*
   * If this isn't a background job and we haven't been instructed not to
   * produce Apdex metrics, produce the Apdex metrics now.
   */
  if (nr_txn_should_create_apdex_metrics(txn)) {
    nr_txn_create_apdex_metrics(txn, duration);
  }

  /*
   * If we encountered any errors we have metrics to add.
   */
  if (txn->error) {
    nr_txn_create_error_metrics(txn, txn->name);
    nr_txn_add_error_attributes(txn);
  }
}

nr_status_t nr_txn_set_path(const char* whence,
                            nrtxn_t* txn,
                            const char* path,
                            nr_path_type_t ptype,
                            nr_txn_assignment_t ok_to_override) {
  if (nrunlikely((0 == txn) || (0 == path) || (0 == path[0]))) {
    return NR_FAILURE;
  }

  /*
   * We can't adjust the path type of previously frozen transactions.
   */
  if (0 != txn->status.path_is_frozen) {
    return NR_FAILURE;
  }

  /*
   * We can't adjust the path type of previously non-frozen transactions
   * of higher priority.
   */
  if ((int)ptype < (int)txn->status.path_type) {
    return NR_FAILURE;
  }

  if ((NR_NOT_OK_TO_OVERWRITE == ok_to_override)
      && (ptype == txn->status.path_type)) {
    return NR_FAILURE;
  }

  if (0 != whence) {
    nrl_debug(NRL_FRAMEWORK, NRP_FMT " naming is " NRP_FMT, NRP_CONFIG(whence),
              NRP_FWNAME(path));
  }

  txn->status.path_type = ptype;
  nr_free(txn->path);
  txn->path = nr_strdup(path);

  return NR_SUCCESS;
}

void nr_txn_set_request_uri(nrtxn_t* txn, const char* uri) {
  int i;
  char* without_params;

  if (nrunlikely((0 == txn) || (0 == uri) || (0 == uri[0]))) {
    return;
  }

  /*
   * The stored URL never contains query parameters.  They are instead
   * captured separately using nr_txn_add_request_parameter.
   */
  without_params = nr_strdup(uri);
  for (i = 0; without_params[i]; i++) {
    if (('?' == uri[i]) || ('#' == uri[i]) || (';' == uri[i])) {
      without_params[i] = '\0';
      break;
    }
  }

  nr_free(txn->request_uri);
  txn->request_uri = without_params;

  nrl_verbosedebug(NRL_TXN, "request_uri=" NRP_FMT, NRP_URL(txn->request_uri));
}

nr_status_t nr_txn_record_error_worthy(const nrtxn_t* txn, int priority) {
  if (nrunlikely((0 == txn) || (0 == txn->options.err_enabled)
                 || (0 == txn->status.recording))) {
    return NR_FAILURE;
  }

  if (0 == txn->error) {
    return NR_SUCCESS;
  }

  if (priority < nr_error_priority(txn->error)) {
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

void nr_txn_record_error(nrtxn_t* txn,
                         int priority,
                         const char* errmsg,
                         const char* errclass,
                         const char* stacktrace_json) {
  if (nrunlikely((0 == txn) || (0 == txn->options.err_enabled) || (0 == errmsg)
                 || (0 == errclass) || (0 == txn->status.recording)
                 || (0 == errmsg[0]) || (0 == errclass[0])
                 || (0 == stacktrace_json))) {
    return;
  }

  if (txn->error) {
    if (priority < nr_error_priority(txn->error)) {
      return;
    }
    nr_error_destroy(&txn->error);
  }

  if (txn->high_security) {
    errmsg = NR_TXN_HIGH_SECURITY_ERROR_MESSAGE;
  }

  if (0 == txn->options.allow_raw_exception_messages) {
    errmsg = NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE;
  }

  txn->error = nr_error_create(priority, errmsg, errclass, stacktrace_json,
                               nr_get_time());

  nrl_verbosedebug(NRL_TXN,
                   "recording error priority=%d msg='%.48s' cls='%.48s'",
                   priority, NRSAFESTR(errmsg), NRSAFESTR(errclass));
}

char* nr_txn_create_fn_supportability_metric(const char* function_name,
                                             const char* class_name) {
  return nr_formatf("Supportability/InstrumentedFunction/%s%s%s",
                    class_name ? class_name : "", class_name ? "::" : "",
                    function_name ? function_name : "");
}

void nr_txn_force_single_count(nrtxn_t* txn, const char* metric_name) {
  if (NULL == txn) {
    return;
  }
  if (NULL == metric_name) {
    return;
  }

  nrm_force_add(txn->unscoped_metrics, metric_name, 0);
}

int nr_txn_should_force_persist(const nrtxn_t* txn) {
  if (0 == txn) {
    return 0;
  }

  if (txn->status.has_inbound_record_tt || txn->status.has_outbound_record_tt) {
    return 1;
  }

  return 0;
}

static void nr_txn_change_background_status(nrtxn_t* txn,
                                            const char* reason,
                                            int is_background) {
  if (0 == txn) {
    return;
  }
  if (txn->status.path_is_frozen) {
    /*
     * The transaction name prefix depends on whether or not this is a
     * background task.  Therefore, the background status cannot be
     * changed after the name is frozen.
     */
    nrm_force_add(txn->unscoped_metrics,
                  "Supportability/background_status_change_prevented", 0);
    return;
  }

  txn->status.background = is_background;

  nrl_debug(NRL_INIT, "%.128s: marking txn as %.32s", reason ? reason : "",
            is_background ? "background job" : "web transaction");
}

void nr_txn_set_as_background_job(nrtxn_t* txn, const char* reason) {
  nr_txn_change_background_status(txn, reason, 1);
}

void nr_txn_set_as_web_transaction(nrtxn_t* txn, const char* reason) {
  nr_txn_change_background_status(txn, reason, 0);
}

void nr_txn_set_http_status(nrtxn_t* txn, int http_code) {
  char buf[32];

  if (0 == txn) {
    return;
  }
  if (0 == http_code) {
    return;
  }
  if (txn->status.background) {
    return;
  }

  buf[0] = 0;
  snprintf(buf, sizeof(buf), "%d", http_code);
  nr_txn_set_string_attribute(txn, nr_txn_response_code, buf);
}

#define NR_DEFAULT_USER_ATTRIBUTE_DESTINATIONS NR_ATTRIBUTE_DESTINATION_ALL

nr_status_t nr_txn_add_user_custom_parameter(nrtxn_t* txn,
                                             const char* key,
                                             const nrobj_t* value) {
  if (0 == txn) {
    return NR_FAILURE;
  }
  if (txn->high_security) {
    return NR_FAILURE;
  }

  if (0 == txn->options.custom_parameters_enabled) {
    return NR_FAILURE;
  }

  return nr_attributes_user_add(
      txn->attributes, NR_DEFAULT_USER_ATTRIBUTE_DESTINATIONS, key, value);
}

void nr_txn_add_request_parameter(nrtxn_t* txn,
                                  const char* key,
                                  const char* value,
                                  int legacy_enable) {
  char* buf = NULL;
  uint32_t default_destinations;

  if (0 == txn) {
    return;
  }
  if (0 == key) {
    return;
  }
  if (0 == key[0]) {
    return;
  }
  if (0 == value) {
    return;
  }
  if (txn->high_security || txn->lasp) {
    return;
  }

  /*
   * The deprecated mechanisms for collecting request parameters only affect
   * the default locations for request parameters.  Attribute configuration
   * will therefore take precedence.
   */
  if (legacy_enable) {
    default_destinations
        = NR_ATTRIBUTE_DESTINATION_TXN_TRACE | NR_ATTRIBUTE_DESTINATION_ERROR;
  } else {
    default_destinations = 0;
  }

  buf = nr_formatf(NR_TXN_REQUEST_PARAMETER_ATTRIBUTE_PREFIX "%s", key);
  nr_attributes_agent_add_string(txn->attributes, default_destinations, buf,
                                 value);
  nr_free(buf);
}

nr_status_t nr_txn_set_stop_time(nrtxn_t* txn,
                                 const nrtxntime_t* start,
                                 nrtxntime_t* stop) {
  nr_txn_set_time(txn, stop);

  if (0 == nr_txn_valid_node_end(txn, start, stop)) {
    return NR_FAILURE;
  }
  return NR_SUCCESS;
}

void nr_txn_set_request_referer(nrtxn_t* txn, const char* request_referer) {
  char* clean_referer;

  if (0 == txn) {
    return;
  }
  if (0 == request_referer) {
    return;
  }

  clean_referer = nr_url_clean(request_referer, nr_strlen(request_referer));
  nr_txn_set_string_attribute(txn, nr_txn_request_referer, clean_referer);
  nr_free(clean_referer);
}

void nr_txn_set_request_content_length(nrtxn_t* txn,
                                       const char* content_length) {
  int content_length_int;

  if (NULL == txn) {
    return;
  }
  if (NULL == content_length) {
    return;
  }
  if ('\0' == content_length[0]) {
    return;
  }

  content_length_int = (int)strtol(content_length, NULL, 10);
  if (0 == content_length_int) {
    return;
  }

  nr_txn_set_long_attribute(txn, nr_txn_request_content_length,
                            content_length_int);
}

nrtime_t nr_txn_queue_time(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }
  if (0 == txn->status.http_x_start) {
    return 0;
  }
  /*
   * This comparison prevents overflow in the case that the queue timestamp
   * is after the start of the transaction.  This defensive check could be
   * performed when http_x_start is assigned in nr_txn_set_queue_start: The
   * choice is arbitrary.  In any case, a check is necessary, since the value
   * comes from an HTTP header and is therefore unsafe.
   */
  if (txn->root.start_time.when < txn->status.http_x_start) {
    return 0;
  }
  return txn->root.start_time.when - txn->status.http_x_start;
}

void nr_txn_set_queue_start(nrtxn_t* txn, const char* x_request_start) {
  nrtime_t queue_start = 0;

  if (NULL == txn) {
    return;
  }
  if (NULL == x_request_start) {
    return;
  }

  /*
   * (per
   * https://docs.newrelic.com/docs/apm/other-features/request-queueing/configuring-request-queue-reporting)
   *
   * The leading 't=' is optional.
   */
  if (('t' == x_request_start[0]) && ('=' == x_request_start[1])) {
    queue_start = nr_parse_unix_time(x_request_start + 2);
  } else {
    queue_start = nr_parse_unix_time(x_request_start);
  }

  if (0 == queue_start) {
    nrtime_t now = nr_get_time();
    double now_microseconds = ((double)now) / NR_TIME_DIVISOR_US_D;

    nrl_debug(NRL_TXN,
              "unable to parse HTTP_X_REQUEST_START header " NRP_FMT
              " expected something like 't=%.0f'",
              NRP_PHP(x_request_start), now_microseconds);
  } else {
    txn->status.http_x_start = queue_start;
  }
}

void nr_txn_record_custom_event_internal(nrtxn_t* txn,
                                         const char* type,
                                         const nrobj_t* params,
                                         nrtime_t now) {
  nr_random_t* rnd;

  if (NULL == txn) {
    return;
  }
  if (0 == txn->status.recording) {
    return;
  }
  if (txn->high_security) {
    return;
  }
  if (0 == txn->options.custom_events_enabled) {
    return;
  }

  /*
   * It would be nice to use the random generator in the application structure,
   * but it is not worth the bother of acquiring the app here.  We do not
   * make a copy of the app's generator to put in the transaction since
   * that would be brittle: we do not want each transaction to have an
   * identical generator.
   */
  rnd = nr_random_create();
  nr_random_seed(rnd, now);

  nr_custom_events_add_event(txn->custom_events, type, params, now, rnd);

  nr_random_destroy(&rnd);
}

void nr_txn_record_custom_event(nrtxn_t* txn,
                                const char* type,
                                const nrobj_t* params) {
  nr_txn_record_custom_event_internal(txn, type, params, nr_get_time());
}

int nr_txn_is_synthetics(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }
  return (NR_TXN_TYPE_SYNTHETICS & txn->type) ? 1 : 0;
}

static int nr_txn_is_cat(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }

  if ((NR_TXN_TYPE_CAT_INBOUND & txn->type)
      || (NR_TXN_TYPE_CAT_OUTBOUND & txn->type)) {
    return 1;
  }

  return 0;
}

static int nr_txn_is_dt(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }

  if ((NR_TXN_TYPE_DT_INBOUND & txn->type)
      || (NR_TXN_TYPE_DT_OUTBOUND & txn->type)) {
    return 1;
  }

  return 0;
}

nr_tt_recordsql_t nr_txn_sql_recording_level(const nrtxn_t* txn) {
  if (NULL == txn) {
    return NR_SQL_NONE;
  }

  switch (txn->options.tt_recordsql) {
    case NR_SQL_RAW:
      /* High security overrides raw SQL capture. */
      if (txn->high_security) {
        return NR_SQL_OBFUSCATED;
      }
      return NR_SQL_RAW;
    case NR_SQL_OBFUSCATED:
      return NR_SQL_OBFUSCATED;
    case NR_SQL_NONE: /* fallthrough */
    default:          /* default should never happen. */
      return NR_SQL_NONE;
  }
}

void nr_txn_add_cat_analytics_intrinsics(const nrtxn_t* txn,
                                         nrobj_t* intrinsics) {
  char* alternate_path_hashes = NULL;
  char* path_hash = NULL;
  const char* trip_id = NULL;

  if ((0 == nr_txn_is_cat(txn)) || (NULL == intrinsics)
      || (NR_OBJECT_HASH != nro_type(intrinsics))) {
    return;
  }

  path_hash = nr_txn_current_path_hash(txn);
  alternate_path_hashes = nr_txn_get_alternate_path_hashes(txn);
  trip_id = nr_txn_get_cat_trip_id(txn);

  nro_set_hash_string(intrinsics, "nr.tripId", trip_id);
  nro_set_hash_string(intrinsics, "nr.pathHash", path_hash);

  if (txn->cat.referring_path_hash) {
    nro_set_hash_string(intrinsics, "nr.referringPathHash",
                        txn->cat.referring_path_hash);
  }

  if (txn->cat.inbound_guid) {
    nro_set_hash_string(intrinsics, "nr.referringTransactionGuid",
                        txn->cat.inbound_guid);
  }

  if (alternate_path_hashes) {
    nro_set_hash_string(intrinsics, "nr.alternatePathHashes",
                        alternate_path_hashes);
  }

  nr_free(alternate_path_hashes);
  nr_free(path_hash);
}

void nr_txn_add_cat_intrinsics(const nrtxn_t* txn, nrobj_t* intrinsics) {
  char* path_hash = NULL;
  const char* trip_id = NULL;

  if ((0 == nr_txn_is_cat(txn)) || (NULL == intrinsics)
      || (NR_OBJECT_HASH != nro_type(intrinsics))) {
    return;
  }

  path_hash = nr_txn_current_path_hash(txn);
  trip_id = nr_txn_get_cat_trip_id(txn);

  nro_set_hash_string(intrinsics, "trip_id", trip_id);
  nro_set_hash_string(intrinsics, "path_hash", path_hash);

  nr_free(path_hash);
}

void nr_txn_add_distributed_tracing_intrinsics(const nrtxn_t* txn,
                                               nrobj_t* intrinsics) {
  nr_distributed_trace_t* dt;
  const char* parent_guid;
  const char* parent_txn_id;

  if (NULL == txn) {
    return;
  }

  if (NULL == intrinsics) {
    return;
  }

  dt = txn->distributed_trace;

  /*
   * add the "always add" instrinsics
   */
  nro_set_hash_string(intrinsics, "guid", nr_txn_get_guid(txn));
  nro_set_hash_boolean(intrinsics, "sampled",
                       nr_distributed_trace_is_sampled(dt));
  nro_set_hash_double(intrinsics, "priority",
                      (double)nr_distributed_trace_get_priority(dt));
  nro_set_hash_string(intrinsics, "traceId",
                      nr_distributed_trace_get_trace_id(dt));

  /*
   * add inbound intrinsics
   */
  if (txn->type & NR_TXN_TYPE_DT_INBOUND) {
    nro_set_hash_string(intrinsics, "parent.type",
                        nr_distributed_trace_inbound_get_type(dt));
    nro_set_hash_string(intrinsics, "parent.app",
                        nr_distributed_trace_inbound_get_app_id(dt));
    nro_set_hash_string(intrinsics, "parent.account",
                        nr_distributed_trace_inbound_get_account_id(dt));
    nro_set_hash_string(intrinsics, "parent.transportType",
                        nr_distributed_trace_inbound_get_transport_type(dt));

    nro_set_hash_double(intrinsics, "parent.transportDuration",
                        nr_distributed_trace_inbound_get_timestamp_delta(
                            dt, txn->root.start_time.when)
                            / NR_TIME_DIVISOR);

    parent_guid = nr_distributed_trace_inbound_get_guid(dt);
    if (!nr_strempty(parent_guid)) {
      nro_set_hash_string(intrinsics, "parentSpanId", parent_guid);
    }

    parent_txn_id = nr_distributed_trace_inbound_get_txn_id(dt);
    if (!nr_strempty(parent_txn_id)) {
      nro_set_hash_string(intrinsics, "parentId", parent_txn_id);
    }
  }
}

#define NR_TXN_MAX_ALTERNATE_PATH_HASHES 10
void nr_txn_add_alternate_path_hash(nrtxn_t* txn, const char* path_hash) {
  if ((NULL == txn) || (NULL == path_hash) || ('\0' == path_hash[0])) {
    return;
  }

  /*
   * The limit of 10 alternate path hashes is defined in the spec:
   * https://newrelic.jiveon.com/docs/DOC-1798
   */
  if (nro_getsize(txn->cat.alternate_path_hashes)
      >= NR_TXN_MAX_ALTERNATE_PATH_HASHES) {
    return;
  }

  nro_set_hash_none(txn->cat.alternate_path_hashes, path_hash);
}

nr_apdex_zone_t nr_txn_apdex_zone(const nrtxn_t* txn, nrtime_t duration) {
  if ((NULL == txn) || (NULL != txn->error)) {
    return NR_APDEX_FAILING;
  } else {
    return nr_apdex_zone(txn->options.apdex_t, duration);
  }
}

/*
 * This structure is used when converting an nrobj_t of path hashes into an
 * array (contained within this struct in the hashes field) that we can qsort.
 */
struct hash_state {
  int length;          /* The sum of the string length of each hash */
  int capacity;        /* The number of path hashes allocated */
  int used;            /* The number of path hashes used */
  const char** hashes; /* An array of path hashes */
  char* path_hash;     /* The final path hash, which will be filtered out of the
                          alternate list */
};

static nr_status_t nr_txn_iter_path_hash(const char* key,
                                         const nrobj_t* val NRUNUSED,
                                         struct hash_state* hash_state) {
  if ((NULL == key) || (NULL == hash_state)
      || (hash_state->capacity <= hash_state->used)) {
    return NR_FAILURE;
  }

  /*
   * Check if the hash is also the final path hash, in which case it shouldn't
   * be included in the alternate list, per the spec.
   */
  if (0 == nr_strcmp(key, hash_state->path_hash)) {
    return NR_SUCCESS;
  }

  hash_state->hashes[hash_state->used] = key;
  hash_state->length += nr_strlen(key);
  hash_state->used++;

  return NR_SUCCESS;
}

static int nr_txn_compare_path_hashes(const void* pa, const void* pb) {
  const char* a = *((const char* const*)pa);
  const char* b = *((const char* const*)pb);

  return nr_strcmp(a, b);
}

char* nr_txn_get_alternate_path_hashes(const nrtxn_t* txn) {
  struct hash_state hash_state;
  int i;
  char* result = NULL;

  if (NULL == txn) {
    return NULL;
  }

  hash_state.capacity = nro_getsize(txn->cat.alternate_path_hashes);
  if (0 == hash_state.capacity) {
    return NULL;
  }

  hash_state.path_hash = nr_txn_current_path_hash(txn);
  hash_state.hashes
      = (const char**)nr_calloc(hash_state.capacity, sizeof(const char*));
  hash_state.length = 0;
  hash_state.used = 0;
  nro_iteratehash(txn->cat.alternate_path_hashes,
                  (nrhashiter_t)nr_txn_iter_path_hash, (void*)&hash_state);

  if (0 == hash_state.used) {
    goto end;
  }

  qsort(hash_state.hashes, hash_state.used, sizeof(const char*),
        nr_txn_compare_path_hashes);

  result = (char*)nr_zalloc(hash_state.length + hash_state.used);
  for (i = 0; i < hash_state.used; i++) {
    if (i > 0) {
      nr_strcat(result, ",");
    }
    nr_strcat(result, hash_state.hashes[i]);
  }

end:
  nr_free(hash_state.hashes);
  nr_free(hash_state.path_hash);

  return result;
}

const char* nr_txn_get_cat_trip_id(const nrtxn_t* txn) {
  if (NULL == txn) {
    return NULL;
  }

  return txn->cat.trip_id ? txn->cat.trip_id : nr_txn_get_guid(txn);
}

const char* nr_txn_get_guid(const nrtxn_t* txn) {
  if (NULL == txn) {
    return NULL;
  }

  return nr_distributed_trace_get_txn_id(txn->distributed_trace);
}

void nr_txn_set_guid(nrtxn_t* txn, const char* guid) {
  if (NULL == txn) {
    return;
  }

  if (NULL == txn->distributed_trace) {
    txn->distributed_trace = nr_distributed_trace_create();
  }

  nr_distributed_trace_set_txn_id(txn->distributed_trace, guid);
}

char* nr_txn_current_path_hash(const nrtxn_t* txn) {
  const char* name;

  if (NULL == txn) {
    return NULL;
  }

  /*
   * If the transaction has yet to have its name frozen, we'll use the path for
   * calculating the hash, and if that is unavailable we'll use a placeholder.
   */
  if (txn->name) {
    name = txn->name;
  } else if (txn->path) {
    name = txn->path;
  } else {
    name = "<unknown>";
  }

  return nr_hash_cat_path(name, txn->primary_app_name,
                          txn->cat.referring_path_hash);
}

char* nr_txn_get_path_hash(nrtxn_t* txn) {
  char* path_hash = nr_txn_current_path_hash(txn);

  nr_txn_add_alternate_path_hash(txn, path_hash);

  return path_hash;
}

int nr_txn_is_account_trusted(const nrtxn_t* txn, int account_id) {
  int idx = 0;
  const nrobj_t* trusted_account_ids = NULL;

  if (NULL == txn) {
    return 0;
  }

  if (account_id <= 0) {
    return 0;
  }

  trusted_account_ids
      = nro_get_hash_array(txn->app_connect_reply, "trusted_account_ids", 0);
  idx = nro_find_array_int(trusted_account_ids, account_id);

  return (idx > 0);
}

bool nr_txn_is_account_trusted_dt(const nrtxn_t* txn, const char* trusted_key) {
  const char* trusted_account_id = NULL;

  if (NULL == txn) {
    return false;
  }

  if (NULL == trusted_key) {
    return false;
  }

  trusted_account_id = nro_get_hash_string(txn->app_connect_reply,
                                           "trusted_account_key", NULL);

  return (0 == nr_strcmp(trusted_key, trusted_account_id));
}

int nr_txn_should_save_trace(const nrtxn_t* txn, nrtime_t duration) {
  if (NULL == txn) {
    return 0;
  }

  if (txn->nodes_used < 1) {
    return 0;
  }

  /*
   * We always want to save synthetics transactions.
   */
  if (nr_txn_is_synthetics(txn)) {
    return 1;
  }

  /*
   * Otherwise, let's check the duration against threshold.
   */
  return (duration >= txn->options.tt_threshold);
}

void nr_txn_adjust_exclusive_time(nrtxn_t* txn, nrtime_t duration) {
  if (NULL == txn) {
    return;
  }
  if (NULL == txn->cur_kids_duration) {
    return;
  }
  *(txn->cur_kids_duration) += duration;
}

void nr_txn_add_async_duration(nrtxn_t* txn, nrtime_t duration) {
  if (NULL == txn) {
    return;
  }

  txn->async_duration += duration;
}

int nr_txn_valid_node_end(const nrtxn_t* txn,
                          const nrtxntime_t* start,
                          const nrtxntime_t* stop) {
  if (NULL == txn) {
    return 0;
  }
  if (NULL == start) {
    return 0;
  }
  if (NULL == stop) {
    return 0;
  }
  if (0 == txn->status.recording) {
    return 0;
  }
  if (start->when < txn->root.start_time.when) {
    /*
     * If the start time is after the transaction has started then the
     * transaction has been changed during the the course of the call.
     * We do not record information about function calls which span
     * transactions.
     */
    return 0;
  }
  if (start->when > stop->when) {
    return 0;
  }
  if (start->stamp > stop->stamp) {
    return 0;
  }
  return 1;
}

int nr_txn_event_should_add_guid(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }
  if (nr_txn_is_dt(txn)) {
    return 0;
  }
  if (nr_txn_is_synthetics(txn)) {
    return 1;
  }
  if (nr_txn_is_cat(txn)) {
    return 1;
  }
  return 0;
}

char* nr_txn_get_primary_app_name(const char* appname) {
  char* delimiter;

  if ((NULL == appname) || ('\0' == appname[0])) {
    return NULL;
  }

  delimiter = nr_strchr(appname, ';');
  if (NULL == delimiter) {
    return nr_strdup(appname);
  }
  return nr_strndup(appname, delimiter - appname);
}

double nr_txn_start_time_secs(const nrtxn_t* txn) {
  nrtime_t start = nr_txn_start_time(txn);

  return ((double)start) / NR_TIME_DIVISOR_D;
}

nrtime_t nr_txn_start_time(const nrtxn_t* txn) {
  if (NULL == txn) {
    return 0;
  }
  return txn->root.start_time.when;
}

void nr_txn_add_file_naming_pattern(nrtxn_t* txn, const char* user_pattern) {
  if (NULL == txn) {
    return;
  }

  if (NULL == user_pattern) {
    return;
  }

  if (0 == txn->status.recording) {
    return;
  }

  txn->match_filenames
      = nr_file_namer_append(txn->match_filenames, user_pattern);
}

void nr_txn_add_match_files(nrtxn_t* txn, const char* comma_separated_list) {
  nrobj_t* rs;
  int ns;
  int i;

  rs = nr_strsplit(comma_separated_list, ",", 0);
  ns = nro_getsize(rs);
  for (i = 0; i < ns; i++) {
    const char* s = nro_get_array_string(rs, i + 1, NULL);
    nr_txn_add_file_naming_pattern(txn, s);
  }
  nro_delete(rs);
}

void nr_txn_match_file(nrtxn_t* txn, const char* filename) {
  char* match;

  if (NULL == txn) {
    return;
  }

  if (NULL == filename) {
    return;
  }

  if (0 == txn->status.recording) {
    return;
  }

  if (NULL == txn->match_filenames) {
    return;
  }

  if (txn->status.path_type >= NR_PATH_TYPE_ACTION) {
    return;
  }

  match = nr_file_namer_match(txn->match_filenames, filename);

  if (NULL == match) {
    return;
  }

  nr_txn_set_path("File naming", txn, match, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  nr_free(match);
}

static void nr_txn_add_metric_total_as_attribute(nrobj_t* attributes,
                                                 nrmtable_t* metrics,
                                                 const char* metric_name,
                                                 const char* attribute_name) {
  int64_t total;
  const nrmetric_t* metric = nrm_find(metrics, metric_name);

  if (metric) {
    total = nrm_total(metric);
    nro_set_hash_double(attributes, attribute_name,
                        ((double)total) / NR_TIME_DIVISOR_D);
  }
}

static void nr_txn_add_metric_count_as_attribute(nrobj_t* attributes,
                                                 nrmtable_t* metrics,
                                                 const char* metric_name,
                                                 const char* attribute_name) {
  int64_t count;
  const nrmetric_t* metric = nrm_find(metrics, metric_name);

  if (metric) {
    count = nrm_count(metric);
    nro_set_hash_int(attributes, attribute_name, (int)count);
  }
}

/*
 * This implements the agent Error Events spec:
 * https://source.datanerd.us/agents/agent-specs/blob/master/Error-Events.md
 * We only omit 'gcCumulative' which doesn't apply and 'port' which is too hard.
 */
nr_analytics_event_t* nr_error_to_event(const nrtxn_t* txn) {
  nr_analytics_event_t* event;
  nrobj_t* params;
  nrobj_t* agent_attributes;
  nrobj_t* user_attributes;
  nrtime_t duration;
  nrtime_t when;

  if (0 == txn) {
    return NULL;
  }
  if (0 == txn->options.error_events_enabled) {
    return NULL;
  }
  if (!txn->error) {
    return NULL;
  }

  duration = nr_txn_duration(txn);
  when = nr_error_get_time(txn->error);

  params = nro_new_hash();
  nro_set_hash_string(params, "type", "TransactionError");
  nro_set_hash_double(params, "timestamp", ((double)when) / NR_TIME_DIVISOR_D);
  nro_set_hash_string(params, "error.class", nr_error_get_klass(txn->error));
  nro_set_hash_string(params, "error.message",
                      nr_error_get_message(txn->error));
  nro_set_hash_string(params, "transactionName", txn->name);
  nro_set_hash_double(params, "duration",
                      ((double)duration) / NR_TIME_DIVISOR_D);

  nr_txn_add_metric_total_as_attribute(
      params, txn->unscoped_metrics, "WebFrontend/QueueTime", "queueDuration");
  nr_txn_add_metric_total_as_attribute(params, txn->unscoped_metrics,
                                       "External/all", "externalDuration");
  nr_txn_add_metric_total_as_attribute(params, txn->unscoped_metrics,
                                       "Datastore/all", "databaseDuration");

  nr_txn_add_metric_count_as_attribute(params, txn->unscoped_metrics,
                                       "Datastore/all", "databaseCallCount");
  nr_txn_add_metric_count_as_attribute(params, txn->unscoped_metrics,
                                       "External/all", "externalCallCount");

  nro_set_hash_string(params, "nr.transactionGuid", nr_txn_get_guid(txn));

  if (txn->cat.inbound_guid) {
    nro_set_hash_string(params, "nr.referringTransactionGuid",
                        txn->cat.inbound_guid);
  }

  if (txn->synthetics) {
    nro_set_hash_string(params, "nr.syntheticsResourceId",
                        nr_synthetics_resource_id(txn->synthetics));
    nro_set_hash_string(params, "nr.syntheticsJobId",
                        nr_synthetics_job_id(txn->synthetics));
    nro_set_hash_string(params, "nr.syntheticsMonitorId",
                        nr_synthetics_monitor_id(txn->synthetics));
  }
  if (txn->options.distributed_tracing_enabled) {
    nr_txn_add_distributed_tracing_intrinsics(txn, params);
  }

  agent_attributes = nr_attributes_agent_to_obj(txn->attributes,
                                                NR_ATTRIBUTE_DESTINATION_ERROR);
  user_attributes = nr_attributes_user_to_obj(txn->attributes,
                                              NR_ATTRIBUTE_DESTINATION_ERROR);
  event = nr_analytics_event_create(params, agent_attributes, user_attributes);

  nro_delete(params);
  nro_delete(agent_attributes);
  nro_delete(user_attributes);

  return event;
}

nrobj_t* nr_txn_event_intrinsics(const nrtxn_t* txn) {
  nrobj_t* params;
  nrtime_t duration;

  duration = nr_txn_duration(txn);

  params = nro_new_hash();
  nro_set_hash_string(params, "type", "Transaction");
  nro_set_hash_string(params, "name", txn->name);
  nro_set_hash_double(params, "timestamp", nr_txn_start_time_secs(txn));
  nro_set_hash_double(params, "duration",
                      ((double)duration) / NR_TIME_DIVISOR_D);

  nro_set_hash_double(
      params, "totalTime",
      ((double)duration + txn->async_duration) / NR_TIME_DIVISOR_D);

  if (nr_txn_event_should_add_guid(txn)) {
    nro_set_hash_string(params, "nr.guid", nr_txn_get_guid(txn));
  }

  if (nr_txn_should_create_apdex_metrics(txn)) {
    char apdex;
    char* apdex_str = NULL;

    apdex = nr_apdex_zone_label(nr_txn_apdex_zone(txn, duration));
    apdex_str = nr_formatf("%c", apdex);
    nro_set_hash_string(params, "nr.apdexPerfZone", apdex_str);
    nr_free(apdex_str);
  }

  if (txn->synthetics) {
    nro_set_hash_string(params, "nr.syntheticsResourceId",
                        nr_synthetics_resource_id(txn->synthetics));
    nro_set_hash_string(params, "nr.syntheticsJobId",
                        nr_synthetics_job_id(txn->synthetics));
    nro_set_hash_string(params, "nr.syntheticsMonitorId",
                        nr_synthetics_monitor_id(txn->synthetics));
  }
  nr_txn_add_cat_analytics_intrinsics(txn, params);

  nr_txn_add_metric_total_as_attribute(
      params, txn->unscoped_metrics, "WebFrontend/QueueTime", "queueDuration");
  nr_txn_add_metric_total_as_attribute(params, txn->unscoped_metrics,
                                       "External/all", "externalDuration");
  nr_txn_add_metric_total_as_attribute(params, txn->unscoped_metrics,
                                       "Datastore/all", "databaseDuration");
  nr_txn_add_metric_count_as_attribute(params, txn->unscoped_metrics,
                                       "Datastore/all", "databaseCallCount");

  if (txn->options.distributed_tracing_enabled) {
    nr_txn_add_distributed_tracing_intrinsics(txn, params);
  }

  /*
   * Sets the error intrinsic, as defined in the attribute catalog
   * https://pages.datanerd.us/engineering-management/architecture-notes/notes/123/#error
   */
  if (txn->error) {
    nro_set_hash_boolean(params, "error", true);
  } else {
    nro_set_hash_boolean(params, "error", false);
  }

  return params;
}

/*
 * Refer to:
 * https://source.datanerd.us/agents/agent-specs/blob/master/Transaction-Events-PORTED.md
 * https://source.datanerd.us/agents/agent-specs/blob/master/Synthetics-PORTED.md
 */
nr_analytics_event_t* nr_txn_to_event(const nrtxn_t* txn) {
  nr_analytics_event_t* event;
  nrobj_t* params;
  nrobj_t* agent_attributes;
  nrobj_t* user_attributes;

  if (0 == txn) {
    return NULL;
  }
  if (0 == txn->options.analytics_events_enabled) {
    return NULL;
  }

  params = nr_txn_event_intrinsics(txn);
  agent_attributes = nr_attributes_agent_to_obj(
      txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT);
  user_attributes = nr_attributes_user_to_obj(
      txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT);
  event = nr_analytics_event_create(params, agent_attributes, user_attributes);

  nro_delete(params);
  nro_delete(agent_attributes);
  nro_delete(user_attributes);

  return event;
}

void nr_txn_name_from_function(nrtxn_t* txn,
                               const char* funcname,
                               const char* classname) {
  const char* name = NULL;
  char* buf = NULL;

  if (NULL == txn) {
    return;
  }

  if (NULL == funcname) {
    return;
  }

  /* Optimization: avoid allocation if not necessary. */
  if (txn->status.path_type >= NR_PATH_TYPE_FUNCTION) {
    return;
  }

  name = funcname;
  if (classname) {
    buf = nr_formatf("%s::%s", classname, funcname);
    name = buf;
  }

  nr_txn_set_path("name from function", txn, name, NR_PATH_TYPE_FUNCTION,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_free(buf);
}

void nr_txn_ignore(nrtxn_t* txn) {
  if (NULL == txn) {
    return;
  }

  txn->status.ignore = 1;

  /* Stop recording too to save time */
  txn->status.recording = 0;

  nrl_debug(NRL_API, "ignoring this transaction");
}

nr_status_t nr_txn_add_custom_metric(nrtxn_t* txn,
                                     const char* name,
                                     double value_ms) {
  if (NULL == txn) {
    return NR_FAILURE;
  }
  if (NULL == name) {
    return NR_FAILURE;
  }
  if (0 == txn->status.recording) {
    return NR_FAILURE;
  }

  if (isnan(value_ms) || isinf(value_ms)) {
    const char* kind = isnan(value_ms) ? "NaN" : "Infinity";

    nrl_warning(NRL_API,
                "unable to add custom metric '%s': "
                "invalid custom metric value %s",
                NRSAFESTR(name), kind);
    return NR_FAILURE;
  }

  nrm_add(txn->unscoped_metrics, name,
          (nrtime_t)(NR_TIME_DIVISOR_MS_D * value_ms));

  nrl_debug(NRL_API, "adding custom metric '%s' with value of %f",
            NRSAFESTR(name), value_ms);

  return NR_SUCCESS;
}

bool nr_txn_is_current_path_named(const nrtxn_t* txn, const char* path) {
  if (NULL == txn) {
    return false;
  }
  if (NULL == path) {
    return false;
  }

  if (0 == nr_strcmp(txn->path, path)) {
    return true;
  }

  return false;
}

bool nr_txn_should_create_span_events(const nrtxn_t* txn) {
  return nr_distributed_trace_is_sampled(txn->distributed_trace)
         && txn->options.distributed_tracing_enabled
         && txn->options.span_events_enabled;
}

char* nr_txn_create_distributed_trace_payload(nrtxn_t* txn) {
  nr_distributed_trace_payload_t* payload;
  char* text = NULL;

  if (NULL == txn) {
    goto end;
  }

  if (!txn->options.distributed_tracing_enabled) {
    nrl_info(NRL_CAT,
             "cannot create distributed tracing payload when distributed "
             "tracing is disabled");
    goto end;
  }

  if (!txn->options.span_events_enabled
      && !txn->options.analytics_events_enabled) {
    nrl_info(NRL_CAT,
             "cannot create a distributed tracing payload when BOTH "
             "transaction events (analytics_events_enabled) AND span "
             "events (span_events_enabled) are false");
    goto end;
  }

  /*
   * span = segment = trace node
   *
   * Maybe one day, the id of the current span can be obtained in a clear and
   * concise way:
   *
   *   nr_distributed_trace_set_guid(txn->distributed_trace,
   *                                 nr_txn_current_span_id(txn));
   *
   * In the meantime, a workaround has to be used:
   *
   *  1. A new guid is assigned to txn->current_node_id, the current
   *     time is assigned to txn->current_node_time.
   *     txn->current_node_id is used as the guid (current span id) for the DT
   *     payload.
   *  2. When a new trace node is created, it is checked whether
   *     txn->current_node_time falls in between the start and stop
   *     times of the trace node. If so, the trace node represents the
   *     current span and txn->current_node_id gets assigned to this trace node.
   *
   * This works for the following reasons:
   *
   *  - A segment is created only when it ends.
   *  - As a DT payload is created in the current segment, the current segment
   *    is an external segment.
   *  - Currently only DT auto-instrumentation of certain PHP calls (e. g.
   *    file_get_contents, curl_exec) is supported.
   *  - The segments for those calls don't have children, but are leaves of the
   *    segment tree.
   *  - Due to the nature of the transaction tracer, every child segment ends
   *    before its parent.
   *  - Thus, the current segment is a leave in the segment tree, and
   *    the next segment/trace node to be created is the current
   *    segment.
   */
  if (NULL == txn->current_node_id) {
    txn->current_node_id = nr_guid_create(txn->rnd);
    txn->current_node_time = nr_get_time();
  }

  if (nr_txn_should_create_span_events(txn)) {
    nr_distributed_trace_set_guid(txn->distributed_trace, txn->current_node_id);
  }

  payload = nr_distributed_trace_payload_create(
      txn->distributed_trace,
      nr_distributed_trace_inbound_get_guid(txn->distributed_trace));
  text = nr_distributed_trace_payload_as_text(payload);
  nr_distributed_trace_payload_destroy(&payload);

end:
  if (text) {
    nr_txn_force_single_count(txn, NR_DISTRIBUTED_TRACE_CREATE_SUCCESS);
  } else {
    nr_txn_force_single_count(txn, NR_DISTRIBUTED_TRACE_CREATE_EXCEPTION);
  }
  return text;
}

bool nr_txn_accept_distributed_trace_payload_httpsafe(
    nrtxn_t* txn,
    const char* payload,
    const char* transport_type) {
  bool rv;
  char* decoded_payload = nr_b64_decode(payload, 0);

  if (NULL == decoded_payload) {
    nrl_warning(NRL_CAT, "cannot base64 decode distributed tracing payload %s",
                payload);
    nr_txn_force_single_count(txn, NR_DISTRIBUTED_TRACE_ACCEPT_PARSE_EXCEPTION);
    return false;
  }

  rv = nr_txn_accept_distributed_trace_payload(txn, decoded_payload,
                                               transport_type);

  nr_free(decoded_payload);

  return rv;
}

bool nr_txn_accept_distributed_trace_payload(nrtxn_t* txn,
                                             const char* str_payload,
                                             const char* transport_type) {
  nr_distributed_trace_t* dt;
  const char* error = NULL;
  const char* trusted_key = NULL;
  nrobj_t* obj_payload;
  bool create_successful = false;

  if (NULL == txn || NULL == txn->distributed_trace) {
    return false;
  }

  if (!txn->options.distributed_tracing_enabled) {
    nrl_info(NRL_CAT,
             "cannot accept distributed tracing payload when distributed "
             "tracing is disabled");
    nr_txn_force_single_count(txn, NR_DISTRIBUTED_TRACE_ACCEPT_EXCEPTION);
    return false;
  }

  // Check if Accept or Create has previously been called
  create_successful = (NULL
                       != nrm_find(txn->unscoped_metrics,
                                   NR_DISTRIBUTED_TRACE_CREATE_SUCCESS));

  if (nr_distributed_trace_inbound_is_set(txn->distributed_trace)) {
    nr_txn_force_single_count(txn, NR_DISTRIBUTED_TRACE_ACCEPT_MULTIPLE);
    return false;
  } else if (create_successful) {
    nr_txn_force_single_count(txn,
                              NR_DISTRIBUTED_TRACE_ACCEPT_CREATE_BEFORE_ACCEPT);
    return false;
  }

  dt = txn->distributed_trace;

  obj_payload
      = nr_distributed_trace_convert_payload_to_object(str_payload, &error);

  // Check if payload was invalid
  if (NULL == obj_payload) {
    nr_txn_force_single_count(txn, error);
    return false;
  }

  // Make sure the payload is trusted.
  trusted_key = nr_distributed_trace_object_get_trusted_key(obj_payload);
  if (!trusted_key) {
    trusted_key = nr_distributed_trace_object_get_account_id(obj_payload);
  }
  if (0 == nr_txn_is_account_trusted_dt(txn, trusted_key)) {
    nr_txn_force_single_count(txn,
                              NR_DISTRIBUTED_TRACE_ACCEPT_UNTRUSTED_ACCOUNT);
    nro_delete(obj_payload);
    return false;
  }

  // attempt to accept payload
  if (!nr_distributed_trace_accept_inbound_payload(
          txn->distributed_trace, obj_payload, transport_type, &error)) {
    nr_txn_force_single_count(txn, error);
    nro_delete(obj_payload);
    return false;
  }

  /*
   * Set the correct transport type
   * - If transport type was not specified, check web transaction type.
   * - If non-web set to "unknown", otherwise set to "http"
   */
  if (NULL == transport_type) {
    if (txn->status.background) {
      nr_distributed_trace_inbound_set_transport_type(dt, "Unknown");
    } else {
      nr_distributed_trace_inbound_set_transport_type(dt, "HTTP");
    }
  } else {
    nr_distributed_trace_inbound_set_transport_type(dt, transport_type);
  }

  // Accept was successful
  nr_txn_force_single_count(txn, NR_DISTRIBUTED_TRACE_ACCEPT_SUCCESS);

  nr_txn_create_dt_metrics(txn, "TransportDuration",
                           nr_distributed_trace_inbound_get_timestamp_delta(
                               dt, txn->root.start_time.when)
                               / NR_TIME_DIVISOR);

  txn->type |= NR_TXN_TYPE_DT_INBOUND;

  nro_delete(obj_payload);
  return true;
}

nr_segment_t* nr_txn_get_current_segment(nrtxn_t* txn) {
  if (nrunlikely(NULL == txn)) {
    return NULL;
  }
  return nr_stack_get_top(&txn->parent_stack);
}

void nr_txn_set_current_segment(nrtxn_t* txn, nr_segment_t* segment) {
  if (nrunlikely(NULL == txn)) {
    return;
  }
  nr_stack_push(&txn->parent_stack, (void*)segment);
}

void nr_txn_retire_current_segment(nrtxn_t* txn) {
  if (nrunlikely(NULL == txn)) {
    return;
  }
  nr_stack_pop(&txn->parent_stack);
}

nr_txnnode_attributes_t* nr_txnnode_attributes_create() {
  return (nr_txnnode_attributes_t*)nr_zalloc(sizeof(nr_txnnode_attributes_t));
}

void nr_txnnode_attributes_destroy(nr_txnnode_attributes_t* attributes) {
  if (NULL == attributes) {
    return;
  }
  switch (attributes->type) {
    case NR_DATASTORE:
      nr_free(attributes->datastore.component);
      break;
    case NR_EXTERNAL:
      break;
    case NR_CUSTOM:
    default:
      break;
  }
  nr_free(attributes);
}

void nr_txnnode_set_attributes(nrtxnnode_t* node,
                               const nr_txnnode_attributes_t* attributes) {
  node->attributes = nr_txnnode_attributes_create();
  if (NULL == attributes) {
    node->attributes->type = NR_CUSTOM;
    return;
  }
  switch (attributes->type) {
    case NR_DATASTORE:
      node->attributes->type = NR_DATASTORE;
      node->attributes->datastore.component
          = nr_strdup(attributes->datastore.component);
      break;
    case NR_EXTERNAL:
      node->attributes->type = NR_EXTERNAL;
      break;
    case NR_CUSTOM:
    default:
      node->attributes->type = NR_CUSTOM;
      break;
  }
}
