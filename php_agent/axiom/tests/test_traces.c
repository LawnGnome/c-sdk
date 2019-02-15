#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_traces.h"
#include "nr_traces_private.h"
#include "nr_span_event_private.h"
#include "nr_txn.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

nrapp_t* nr_app_verify_id(nrapplist_t* applist NRUNUSED,
                          const char* agent_run_id NRUNUSED) {
  return 0;
}
#define SPAN_EVENT_COMPARE(evt, expect_name, expect_category, expect_parent,  \
                           expect_start, expect_duration)                     \
  tlib_pass_if_not_null(expect_name, evt);                                    \
  tlib_pass_if_str_equal("name", nr_span_event_get_name(evt), expect_name);   \
  tlib_pass_if_time_equal("category", (evt)->type, expect_category);          \
  tlib_pass_if_true("parent", nr_span_event_get_parent(evt) == expect_parent, \
                    "%p=%p", nr_span_event_get_parent(evt), expect_parent);   \
  tlib_pass_if_time_equal("start", (evt)->timestamp, expect_start);           \
  tlib_pass_if_time_equal("duration", (evt)->duration, expect_duration);

#define SPAN_EVENT_COMPARE_DATASTORE(span_event, expected_host,                \
                                     expected_db_name, expected_statement,     \
                                     expected_address)                         \
  tlib_pass_if_str_equal("host", expected_host,                                \
                         nr_span_event_get_datastore(                          \
                             span_event, NR_SPAN_DATASTORE_PEER_HOSTNAME));    \
  tlib_pass_if_str_equal("address", expected_address,                          \
                         nr_span_event_get_datastore(                          \
                             span_event, NR_SPAN_DATASTORE_PEER_ADDRESS));     \
  tlib_pass_if_str_equal(                                                      \
      "database name", expected_db_name,                                       \
      nr_span_event_get_datastore(span_event, NR_SPAN_DATASTORE_DB_INSTANCE)); \
  tlib_pass_if_str_equal("Statement", expected_statement,                      \
                         nr_span_event_get_datastore(                          \
                             span_event, NR_SPAN_DATASTORE_DB_STATEMENT));     \
  tlib_pass_if_true(                                                           \
      "category", NR_SPAN_DATASTORE == nr_span_event_get_category(span_event), \
      "%d==%d", NR_SPAN_DATASTORE, nr_span_event_get_category(span_event));

#define SPAN_EVENT_COMPARE_EXTERNAL(span_event, expected_url, expected_method, \
                                    expected_component)                        \
  tlib_pass_if_str_equal(                                                      \
      "url", expected_url,                                                     \
      nr_span_event_get_external(span_event, NR_SPAN_EXTERNAL_URL));           \
  tlib_pass_if_str_equal(                                                      \
      "method", expected_method,                                               \
      nr_span_event_get_external(span_event, NR_SPAN_EXTERNAL_METHOD));        \
  tlib_pass_if_str_equal(                                                      \
      "component", expected_component,                                         \
      nr_span_event_get_external(span_event, NR_SPAN_EXTERNAL_COMPONENT));     \
                                                                               \
  tlib_pass_if_true(                                                           \
      "category", NR_SPAN_HTTP == nr_span_event_get_category(span_event),      \
      "%d==%d", NR_SPAN_HTTP, nr_span_event_get_category(span_event));

static void delete_span_events(nr_span_event_t* events[],
                               int span_events_size) {
  for (int i = 0; i < span_events_size && events[i]; i++) {
    nr_span_event_destroy(&events[i]);
  }
}

static void destroy_txnnode_attributes(nrtxn_t txn) {
  for (int i = 0; i < txn.nodes_used; i++) {
    nr_txnnode_attributes_destroy(txn.nodes[i].attributes);
  }
}

#define test_buffer_contents(...) \
  test_buffer_contents_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_buffer_contents_fn(const char* testname,
                                    nrbuf_t* buf,
                                    const char* expected,
                                    const char* file,
                                    int line) {
  const char* cs;
  nrobj_t* obj;

  nr_buffer_add(buf, "", 1);
  cs = (const char*)nr_buffer_cptr(buf);

  test_pass_if_true(testname, 0 == nr_strcmp(cs, expected), "cs=%s expected=%s",
                    NRSAFESTR(cs), NRSAFESTR(expected));

  if (nr_strcmp(cs, expected)) {
    printf("got:      %s\n", NRSAFESTR(cs));
    printf("expected: %s\n", NRSAFESTR(expected));
  }

  obj = nro_create_from_json(cs);
  test_pass_if_true(testname, 0 != obj, "obj=%p", obj);
  nro_delete(obj);

  nr_buffer_reset(buf);
}

static void test_json_print_segments(void) {
  nrtxn_t txn;
  int rv;
  nrbuf_t* buf;
  nrpool_t* node_names;
  nr_harvest_trace_node_t* nodes = NULL;

  nr_memset(&txn, 0, sizeof(nrtxn_t));
  buf = nr_buffer_create(4096, 4096);
  node_names = nr_string_pool_create();

  txn.trace_strings = nr_string_pool_create();

  txn.root.start_time.when = 1000;
  txn.root.start_time.stamp = 1000;
  txn.root.stop_time.when = 10000;
  txn.root.stop_time.stamp = 10000;

  txn.root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.nodes_used = 1;

  txn.nodes[0].start_time.when = 2000;
  txn.nodes[0].start_time.stamp = 2000;
  txn.nodes[0].stop_time.when = 4000;
  txn.nodes[0].stop_time.stamp = 4000;
  txn.nodes[0].name = nr_string_add(txn.trace_strings, "Mongo/alpha");

  nodes = nr_harvest_trace_sort_nodes(&txn);

  /*
   * Test : Bad parameters
   */
  rv = nr_traces_json_print_segments(0, 0, 0, 0, nodes, node_names, NULL, 0,
                                     NULL);
  tlib_pass_if_true("zero params", -1 == rv, "rv=%d", rv);

  rv = nr_traces_json_print_segments(0, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("no buf", -1 == rv, "rv=%d", rv);

  rv = nr_traces_json_print_segments(buf, 0, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("no txn", -1 == rv, "rv=%d", rv);

  rv = nr_traces_json_print_segments(buf, &txn, 0, 0, nodes, node_names, NULL,
                                     0, NULL);
  tlib_pass_if_true("no root", -1 == rv, "rv=%d", rv);

  rv = nr_traces_json_print_segments(buf, 0, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("no txn", -1 == rv, "rv=%d", rv);

  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, -1, nodes,
                                     node_names, NULL, 0, NULL);
  tlib_pass_if_true("negative index", -1 == rv, "rv=%d", rv);

  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, 0, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("no nodes", -1 == rv, "rv=%d", rv);

  /*
   * Test : Success!
   */
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("success", 1 == rv, "rv=%d", rv);
  test_buffer_contents("success", buf, "[0,9,\"`0\",{},[[1,3,\"`1\",{},[]]]]");

  /*
   * Test : Node with Bad Stamps
   */
  txn.nodes[0].start_time.stamp = 2000;
  txn.nodes[0].stop_time.stamp = 2000;
  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("equal stamps", -1 == rv, "rv=%d", rv);
  nr_buffer_reset(buf);

  txn.nodes[0].start_time.stamp = 3000;
  txn.nodes[0].stop_time.stamp = 2000;
  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("out of order stamps", -1 == rv, "rv=%d", rv);
  nr_buffer_reset(buf);

  txn.nodes[0].start_time.stamp = 2000;
  txn.nodes[0].stop_time.stamp = 4000;

  /*
   * Test : Node Stop Before Node Start
   */
  txn.nodes[0].start_time.when = 4000;
  txn.nodes[0].stop_time.when = 2000;
  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("start before stop", 1 == rv, "rv=%d", rv);
  test_buffer_contents("start before stop", buf,
                       "[0,9,\"`0\",{},[[3,3,\"`1\",{},[]]]]");

  /*
   * Test : Node Start Before Txn Start
   */
  txn.nodes[0].start_time.when = 500;
  txn.nodes[0].stop_time.when = 4000;
  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("start before txn start", 1 == rv, "rv=%d", rv);
  test_buffer_contents("start before txn start", buf,
                       "[0,9,\"`0\",{},[[0,3,\"`1\",{},[]]]]");

  /*
   * Test : Node Start and Stop Before Txn Start
   */
  txn.nodes[0].start_time.when = 500;
  txn.nodes[0].stop_time.when = 600;
  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("start before txn start", 1 == rv, "rv=%d", rv);
  test_buffer_contents("start before txn start", buf,
                       "[0,9,\"`0\",{},[[0,0,\"`1\",{},[]]]]");

  txn.nodes[0].start_time.when = 2000;
  txn.nodes[0].stop_time.when = 4000;

  /*
   * Test : Unknown Name
   */
  txn.nodes[0].name = 0;
  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("unknown name", 1 == rv, "rv=%d", rv);
  test_buffer_contents("unknown name", buf,
                       "[0,9,\"`0\",{},[[1,3,\"`2\",{},[]]]]");

  /*
   * Test : Node with Data
   */
  txn.nodes[0].name
      = nr_string_add(txn.trace_strings, "External/domain.com/all");
  txn.nodes[0].data_hash = nro_new_hash();
  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);
  nro_set_hash_string(txn.nodes[0].data_hash, "uri", "domain.com");
  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("node with data", 1 == rv, "rv=%d", rv);
  test_buffer_contents("node with data", buf,
                       "[0,9,\"`0\",{},"
                       "[[1,3,\"`3\",{\"uri\":\"domain.com\"},[]]]]");
  nro_delete(txn.nodes[0].data_hash);

  /*
   * Test : Multiple Segments
   *
   * Call stack is depicted by 'Layer Cake' transaction diagrams.
   */

  txn.nodes[0].name = nr_string_add(txn.trace_strings, "A");
  txn.nodes[1].name = nr_string_add(txn.trace_strings, "B");
  txn.nodes[2].name = nr_string_add(txn.trace_strings, "C");
  txn.nodes_used = 3;

  /*
   *         --C--
   *       ----B----
   *     ------A------
   */
  txn.nodes[0].start_time.when = 2000;
  txn.nodes[0].start_time.stamp = 2000;

  txn.nodes[1].start_time.when = 3000;
  txn.nodes[1].start_time.stamp = 3000;

  txn.nodes[2].start_time.when = 4000;
  txn.nodes[2].start_time.stamp = 4000;

  txn.nodes[2].stop_time.when = 5000;
  txn.nodes[2].stop_time.stamp = 5000;

  txn.nodes[1].stop_time.when = 6000;
  txn.nodes[1].stop_time.stamp = 6000;

  txn.nodes[0].stop_time.when = 7000;
  txn.nodes[0].stop_time.stamp = 7000;

  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);

  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("three nodes", 3 == rv, "rv=%d", rv);
  test_buffer_contents("towers of hanoi", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`4\",{},[[2,5,\"`5\",{},[[3,4,"
                       "\"`6\",{},[]]]]]]]]");

  /*
   *      --B-- --C--
   *     ------A------
   */
  txn.nodes[0].start_time.when = 2000;
  txn.nodes[0].start_time.stamp = 2000;

  txn.nodes[1].start_time.when = 3000;
  txn.nodes[1].start_time.stamp = 3000;
  txn.nodes[1].stop_time.when = 4000;
  txn.nodes[1].stop_time.stamp = 4000;

  txn.nodes[2].start_time.when = 5000;
  txn.nodes[2].start_time.stamp = 5000;
  txn.nodes[2].stop_time.when = 6000;
  txn.nodes[2].stop_time.stamp = 6000;

  txn.nodes[0].stop_time.when = 7000;
  txn.nodes[0].stop_time.stamp = 7000;

  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);

  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("three nodes", 3 == rv, "rv=%d", rv);
  test_buffer_contents("two kids", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`4\",{},[[2,3,\"`5\",{},[]],[4,"
                       "5,\"`6\",{},[]]]]]]");

  /*
   *  --A--  --B--  --C--
   */
  txn.nodes[0].start_time.when = 2000;
  txn.nodes[0].start_time.stamp = 2000;
  txn.nodes[0].stop_time.when = 3000;
  txn.nodes[0].stop_time.stamp = 3000;

  txn.nodes[1].start_time.when = 4000;
  txn.nodes[1].start_time.stamp = 4000;
  txn.nodes[1].stop_time.when = 5000;
  txn.nodes[1].stop_time.stamp = 5000;

  txn.nodes[2].start_time.when = 6000;
  txn.nodes[2].start_time.stamp = 6000;
  txn.nodes[2].stop_time.when = 7000;
  txn.nodes[2].stop_time.stamp = 7000;

  nr_free(nodes);
  nodes = nr_harvest_trace_sort_nodes(&txn);

  rv = nr_traces_json_print_segments(buf, &txn, &txn.root, 0, nodes, node_names,
                                     NULL, 0, NULL);
  tlib_pass_if_true("three nodes", 3 == rv, "rv=%d", rv);
  test_buffer_contents("sequential nodes", buf,
                       "[0,9,\"`0\",{},[[1,2,\"`4\",{},[]],[3,4,\"`5\",{},[]],["
                       "5,6,\"`6\",{},[]]]]");

  nr_string_pool_destroy(&node_names);
  nr_string_pool_destroy(&txn.trace_strings);
  nr_buffer_destroy(&buf);
  nr_free(nodes);
}

/*
 * This function intentionally doesn't cover every field in a nrtxnnode_t, but
 * just what the async tests need for now.
 */
static void add_node_to_txn(nrtxn_t* txn,
                            nrtime_t start,
                            nrtime_t stop,
                            const char* name,
                            const char* async_context,
                            nrobj_t* data_hash,
                            const nr_txnnode_attributes_t* attributes) {
  nrtxnnode_t* node = txn->nodes + (txn->nodes_used++);

  node->start_time.stamp = (int)start;
  node->start_time.when = start;
  node->stop_time.stamp = (int)stop;
  node->stop_time.when = stop;
  node->name = nr_string_add(txn->trace_strings, name);
  node->async_context
      = async_context ? nr_string_add(txn->trace_strings, async_context) : 0;
  node->data_hash = data_hash;
  nr_txnnode_set_attributes(node, attributes);
}

typedef struct {
  nrtxn_t txn;
  nrpool_t* node_names;
  nrbuf_t* buf;
  nr_harvest_trace_node_t* nodes;
} txn_context_t;

static void txn_context_init(txn_context_t* txn_context) {
  nr_memset(&txn_context->txn, 0, sizeof(nrtxn_t));
  txn_context->node_names = nr_string_pool_create();
  txn_context->buf = nr_buffer_create(4096, 4096);

  txn_context->txn.trace_strings = nr_string_pool_create();

  txn_context->txn.root.start_time.when = 1000;
  txn_context->txn.root.start_time.stamp = 1000;
  txn_context->txn.root.stop_time.when = 10000;
  txn_context->txn.root.stop_time.stamp = 10000;
  txn_context->txn.root.name
      = nr_string_add(txn_context->txn.trace_strings, "WebTransaction/*");

  txn_context->txn.async_duration = 1;
}

static void txn_context_deinit(txn_context_t* txn_context) {
  destroy_txnnode_attributes(txn_context->txn);
  nr_string_pool_destroy(&txn_context->txn.trace_strings);
  nr_string_pool_destroy(&txn_context->node_names);
  nr_buffer_destroy(&txn_context->buf);
  nr_free(txn_context->nodes);
}

static txn_context_t* txn_context_create(void) {
  txn_context_t* txn_context = (txn_context_t*)nr_malloc(sizeof(txn_context_t));

  txn_context_init(txn_context);
  return txn_context;
}

static void txn_context_destroy(txn_context_t** txn_context_ptr) {
  txn_context_deinit(*txn_context_ptr);
  nr_realfree((void**)txn_context_ptr);
}

static void txn_context_reset(txn_context_t* txn_context) {
  txn_context_deinit(txn_context);
  txn_context_init(txn_context);
}

static void txn_context_prepare(txn_context_t* txn_context) {
  txn_context->nodes = nr_harvest_trace_sort_nodes(&txn_context->txn);
}

static void test_json_print_segments_async(void) {
  int rv;
  txn_context_t* tc = txn_context_create();
  nrobj_t* hash;

  /*
   * Basic test: main context lasts the same timespan as ROOT, and spawns one
   * child context for part of its run time.
   *
   * These diagrams all follow the same pattern: time is shown in seconds on
   * the first row, followed by the ROOT node, and then individual contexts
   * with their nodes.  The "main" context indicates that no async_context will
   * be attached to nodes in that context.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |- loop --|
   */
  add_node_to_txn(&tc->txn, 1000, 10000, "main", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 2000, 4000, "loop", "async", NULL, NULL);
  txn_context_prepare(tc);
  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, NULL, 0, NULL);
  tlib_pass_if_true("basic", 2 == rv, "rv=%d", rv);
  test_buffer_contents("basic", tc->buf,
                       "["
                       "0,9,\"`0\",{},"
                       "["
                       "["
                       "0,9,\"`1\",{},"
                       "["
                       "[1,3,\"`2\",{\"async_context\":\"`3\"},[]]"
                       "]"
                       "]"
                       "]"
                       "]");

  /*
   * Multiple children test: main context lasts the same timespan as ROOT, and
   * spawns one child context with three nodes for part of its run time, one of
   * which has a duplicated name.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |--- a ---|--- b ---|    | a  |
   */
  txn_context_reset(tc);
  add_node_to_txn(&tc->txn, 1000, 10000, "main", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 2000, 4000, "a", "async", NULL, NULL);
  add_node_to_txn(&tc->txn, 4000, 6000, "b", "async", NULL, NULL);
  add_node_to_txn(&tc->txn, 7000, 8000, "a", "async", NULL, NULL);
  txn_context_prepare(tc);
  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, NULL, 0, NULL);
  tlib_pass_if_true("multiple children", 4 == rv, "rv=%d", rv);
  test_buffer_contents("multiple children", tc->buf,
                       "["
                       "0,9,\"`0\",{},"
                       "["
                       "["
                       "0,9,\"`1\",{},"
                       "["
                       "[1,3,\"`2\",{\"async_context\":\"`3\"},[]],"
                       "[3,5,\"`4\",{\"async_context\":\"`3\"},[]],"
                       "[6,7,\"`2\",{\"async_context\":\"`3\"},[]]"
                       "]"
                       "]"
                       "]"
                       "]");

  /*
   * Multiple contexts test: main context lasts the same timespan as ROOT, and
   * spawns three child contexts with a mixture of nodes.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * alpha                          |--- a ---|--- b ---|    | a  |
   * beta                                |--- c ---|
   * gamma                                                             | d  |
   */
  txn_context_reset(tc);
  add_node_to_txn(&tc->txn, 1000, 10000, "main", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 2000, 4000, "a", "alpha", NULL, NULL);
  add_node_to_txn(&tc->txn, 4000, 6000, "b", "alpha", NULL, NULL);
  add_node_to_txn(&tc->txn, 7000, 8000, "a", "alpha", NULL, NULL);
  add_node_to_txn(&tc->txn, 3000, 5000, "c", "beta", NULL, NULL);
  add_node_to_txn(&tc->txn, 9000, 10000, "d", "gamma", NULL, NULL);
  txn_context_prepare(tc);
  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, NULL, 0, NULL);
  tlib_pass_if_true("multiple contexts", 6 == rv, "rv=%d", rv);
  test_buffer_contents("multiple contexts", tc->buf,
                       "["
                       "0,9,\"`0\",{},"
                       "["
                       "["
                       "0,9,\"`1\",{},"
                       "["
                       "[1,3,\"`2\",{\"async_context\":\"`3\"},[]],"
                       "[3,5,\"`4\",{\"async_context\":\"`3\"},[]],"
                       "[6,7,\"`2\",{\"async_context\":\"`3\"},[]],"
                       "[2,4,\"`5\",{\"async_context\":\"`6\"},[]],"
                       "[8,9,\"`7\",{\"async_context\":\"`8\"},[]]"
                       "]"
                       "]"
                       "]"
                       "]");

  /*
   * Context nesting test: contexts spawned from different main thread
   * contexts.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   *                                |--- a ---|----- b ------|
   *                                                    | c  |
   * alpha                               |---------- d ---------------------|
   *                                               |--- e ---|
   * beta                                          |--- f ---|
   * gamma                                                    | g |
   */
  txn_context_reset(tc);
  add_node_to_txn(&tc->txn, 1000, 10000, "main", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 2000, 4000, "a", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 4000, 7000, "b", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 6000, 7000, "c", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 3000, 10000, "d", "alpha", NULL, NULL);
  add_node_to_txn(&tc->txn, 5000, 7000, "e", "alpha", NULL, NULL);
  add_node_to_txn(&tc->txn, 5000, 7000, "f", "beta", NULL, NULL);
  add_node_to_txn(&tc->txn, 7200, 8000, "g", "gamma", NULL, NULL);
  txn_context_prepare(tc);
  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, NULL, 0, NULL);
  tlib_pass_if_true("context nesting", 8 == rv, "rv=%d", rv);
  test_buffer_contents("context nesting", tc->buf,
                       "["
                       "0,9,\"`0\",{},"
                       "["
                       "["
                       "0,9,\"`1\",{},"
                       "["
                       "[1,3,\"`2\",{},"
                       "["
                       "[2,9,\"`3\",{\"async_context\":\"`4\"},"
                       "["
                       "[4,6,\"`5\",{\"async_context\":\"`4\"},[]]"
                       "]"
                       "]"
                       "]"
                       "],"
                       "[3,6,\"`6\",{},"
                       "["
                       "[4,6,\"`7\",{\"async_context\":\"`8\"},[]],"
                       "[5,6,\"`9\",{},[]]"
                       "]"
                       "],"
                       "[6,7,\"`10\",{\"async_context\":\"`11\"},[]]"
                       "]"
                       "]"
                       "]"
                       "]");

  /*
   * Data hash testing: ensure that we never overwrite a data hash, and also
   * ensure that we never modify it.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |- loop --|
   */
  txn_context_reset(tc);
  hash = nro_create_from_json("{\"foo\":\"bar\"}");
  add_node_to_txn(&tc->txn, 1000, 10000, "main", NULL, hash, NULL);
  add_node_to_txn(&tc->txn, 2000, 4000, "loop", "async", hash, NULL);
  txn_context_prepare(tc);
  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, NULL, 0, NULL);
  tlib_pass_if_true("basic", 2 == rv, "rv=%d", rv);
  test_buffer_contents(
      "basic", tc->buf,
      "["
      "0,9,\"`0\",{},"
      "["
      "["
      "0,9,\"`1\",{\"foo\":\"bar\"},"
      "["
      "[1,3,\"`2\",{\"async_context\":\"`3\",\"foo\":\"bar\"},[]]"
      "]"
      "]"
      "]"
      "]");

  nro_delete(hash);
  txn_context_destroy(&tc);
}

static nrtxn_t* sample_txn_for_trace(void) {
  nrtxn_t* txn = (nrtxn_t*)nr_calloc(1, sizeof(*txn));

  txn->trace_strings = nr_string_pool_create();

  /*
   * Fill options and status.
   */
  txn->request_uri = nr_strdup("/request/uri.php");

  /*
   * Fill root.
   */
  txn->root.start_time.when = 1000;
  txn->root.start_time.stamp = 1000;
  txn->root.stop_time.when = 10000;
  txn->root.stop_time.stamp = 10000;
  txn->name = nr_strdup("WebTransaction/*");
  txn->root.name = nr_string_add(txn->trace_strings, txn->name);

  /*
   * Note : These nodes are out of order, and therefore the sorting is tested.
   */
  txn->nodes[0].name = nr_string_add(txn->trace_strings, "B");
  txn->nodes[1].name = nr_string_add(txn->trace_strings, "A");
  txn->nodes[0].start_time.when = 4000;
  txn->nodes[0].start_time.stamp = 4000;
  txn->nodes[0].stop_time.when = 5000;
  txn->nodes[0].stop_time.stamp = 5000;
  txn->nodes[1].start_time.when = 2000;
  txn->nodes[1].start_time.stamp = 2000;
  txn->nodes[1].stop_time.when = 3000;
  txn->nodes[1].stop_time.stamp = 3000;
  txn->nodes_used = 2;

  /*
   * Populate RUM values.
   */
  nr_txn_set_guid(txn, "837ab461e0946f4f");

  return txn;
}

static void test_harvest_trace_create_data(void) {
  nrtxn_t* txn;
  char* out;
  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");
  nrobj_t* obj;

  out = nr_harvest_trace_create_data(NULL, 2 * NR_TIME_DIVISOR,
                                     agent_attributes, user_attributes,
                                     intrinsics, NULL, 0);

  tlib_pass_if_null("null txn", out);

  txn = (nrtxn_t*)nr_zalloc(sizeof(nrtxn_t));
  out = nr_harvest_trace_create_data(txn, 2 * NR_TIME_DIVISOR, agent_attributes,
                                     user_attributes, intrinsics, NULL, 0);
  tlib_pass_if_null("zero txn", out);
  nr_free(txn);

  txn = sample_txn_for_trace();

  out = nr_harvest_trace_create_data(txn, 0, agent_attributes, user_attributes,
                                     intrinsics, NULL, 0);
  tlib_pass_if_null("zero duration", out);

  out = nr_harvest_trace_create_data(txn, 2 * NR_TIME_DIVISOR, agent_attributes,
                                     user_attributes, intrinsics, NULL, 0);
  tlib_pass_if_str_equal("zero duration", out,
                         "[[0,{},{},[0,2000,\"ROOT\",{},[[0,9,\"`0\",{},[[1,2,"
                         "\"`1\",{},[]],[3,4,\"`2\",{},[]]]]]],"
                         "{\"agentAttributes\":[\"agent_attributes\"],"
                         "\"userAttributes\":[\"user_attributes\"],"
                         "\"intrinsics\":[\"intrinsics\"]}],"
                         "[\"WebTransaction\\/*\",\"A\",\"B\"]]");
  obj = nro_create_from_json(out);
  tlib_pass_if_not_null("valid json", obj);
  nro_delete(obj);
  nr_free(out);
  nr_txn_destroy(&txn);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
}

static void test_harvest_trace_create_data_spans(void) {
  nrtxn_t* txn;
  char* out;
  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");
  nr_span_event_t* span_events[NR_TXN_MAX_NODES + 1] = {NULL};
  int span_events_size = sizeof(span_events) / sizeof(span_events[0]);

  txn = sample_txn_for_trace();

  out = nr_harvest_trace_create_data(txn, 0, agent_attributes, user_attributes,
                                     intrinsics, span_events, span_events_size);
  tlib_pass_if_null("span events disabled", span_events[0]);

  /* Tweak options to create span events. */
  txn->options.distributed_tracing_enabled = true;
  txn->options.span_events_enabled = true;
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);

  out = nr_harvest_trace_create_data(txn, 2 * NR_TIME_DIVISOR, agent_attributes,
                                     user_attributes, intrinsics, span_events,
                                     span_events_size);
  nr_free(out);

  SPAN_EVENT_COMPARE(span_events[0], "WebTransaction/*", NR_SPAN_GENERIC, NULL,
                     1000, 9000)
  SPAN_EVENT_COMPARE(span_events[1], "A", NR_SPAN_GENERIC, span_events[0], 2000,
                     1000)
  SPAN_EVENT_COMPARE(span_events[2], "B", NR_SPAN_GENERIC, span_events[0], 4000,
                     1000)
  tlib_pass_if_null("no more events", span_events[3]);

  nr_txn_destroy(&txn);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
  delete_span_events(span_events, span_events_size);
}

static void test_json_print_segments_async_spans(void) {
  int rv;
  txn_context_t* tc = txn_context_create();
  nr_span_event_t* span_events[NR_TXN_MAX_NODES + 1] = {NULL};
  int span_events_size = sizeof(span_events) / sizeof(span_events[0]);

  /*
   * Context nesting test: contexts spawned from different main thread
   * contexts.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   *                                |--- a ---|----- b ------|
   *                                                    | c  |
   * alpha                               |---------- d ---------------------|
   *                                               |--- e ---|
   * beta                                          |--- f ---|
   * gamma                                                    | g |
   */
  add_node_to_txn(&tc->txn, 1000, 10000, "main", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 2000, 4000, "a", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 4000, 7000, "b", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 6000, 7000, "c", NULL, NULL, NULL);
  add_node_to_txn(&tc->txn, 3000, 10000, "d", "alpha", NULL, NULL);
  add_node_to_txn(&tc->txn, 5000, 7000, "e", "alpha", NULL, NULL);
  add_node_to_txn(&tc->txn, 5000, 7000, "f", "beta", NULL, NULL);
  add_node_to_txn(&tc->txn, 7200, 8000, "g", "gamma", NULL, NULL);
  txn_context_prepare(tc);
  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, span_events,
                                     span_events_size, NULL);
  tlib_pass_if_true("context nesting", 8 == rv, "rv=%d", rv);

  SPAN_EVENT_COMPARE(span_events[0], "WebTransaction/*", NR_SPAN_GENERIC, NULL,
                     1000, 9000)
  SPAN_EVENT_COMPARE(span_events[1], "main", NR_SPAN_GENERIC, span_events[0],
                     1000, 9000)
  SPAN_EVENT_COMPARE(span_events[2], "a", NR_SPAN_GENERIC, span_events[1], 2000,
                     2000)
  SPAN_EVENT_COMPARE(span_events[3], "d", NR_SPAN_GENERIC, span_events[2], 3000,
                     7000)
  SPAN_EVENT_COMPARE(span_events[4], "e", NR_SPAN_GENERIC, span_events[3], 5000,
                     2000)
  SPAN_EVENT_COMPARE(span_events[5], "b", NR_SPAN_GENERIC, span_events[1], 4000,
                     3000)
  SPAN_EVENT_COMPARE(span_events[6], "f", NR_SPAN_GENERIC, span_events[5], 5000,
                     2000)
  SPAN_EVENT_COMPARE(span_events[7], "c", NR_SPAN_GENERIC, span_events[5], 6000,
                     1000)
  SPAN_EVENT_COMPARE(span_events[8], "g", NR_SPAN_GENERIC, span_events[1], 7200,
                     800)
  tlib_pass_if_null("no more events", span_events[9]);

  txn_context_destroy(&tc);
  delete_span_events(span_events, span_events_size);
}

static void test_json_print_datastore(void) {
  int rv;
  txn_context_t* tc = txn_context_create();
  nr_span_event_t* span_events[NR_TXN_MAX_NODES + 1] = {NULL};
  int span_events_size = sizeof(span_events) / sizeof(span_events[0]);
  nrobj_t* hash;
  nrobj_t* hash_event_2;
  nr_txnnode_attributes_t* attributes1;
  nr_txnnode_attributes_t* attributes2;

  hash = nro_create_from_json(
      "{\"host\":\"localhost\",\"port_path_or_id\":\"3306\","
      "\"sql\":\"SELECT * FROM "
      "ORDERS;\",\"database_name\":\"ORDERS\"}");
  attributes1 = nr_txnnode_attributes_create();
  attributes1->type = NR_DATASTORE;
  attributes1->datastore.component = nr_strdup("Mongo/alpha");
  add_node_to_txn(&tc->txn, 2000, 4000, "Mongo/alpha", NULL, hash, attributes1);
  nr_txnnode_attributes_destroy(attributes1);

  hash_event_2 = nro_create_from_json(
      "{\"host\":\"somewhere\",\"port_path_or_"
      "id\":\"8801\",\"sql\":\"DELETE\","
      "\"database_name\":\"CUSTOMERS\"}");
  attributes2 = nr_txnnode_attributes_create();
  attributes2->type = NR_DATASTORE;
  attributes2->datastore.component = nr_strdup("MySQL/beta");
  add_node_to_txn(&tc->txn, 3000, 4000, "MySQL/beta", NULL, hash_event_2,
                  attributes2);
  nr_txnnode_attributes_destroy(attributes2);
  txn_context_prepare(tc);

  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, span_events,
                                     span_events_size, NULL);

  tlib_pass_if_true("correct number of spans", 2 == rv, "rv==%d", rv);
  SPAN_EVENT_COMPARE_DATASTORE(span_events[1], "localhost", "ORDERS",
                               "SELECT * FROM ORDERS;", "localhost:3306");
  SPAN_EVENT_COMPARE_DATASTORE(span_events[2], "somewhere", "CUSTOMERS",
                               "DELETE", "somewhere:8801");

  nro_delete(hash);
  nro_delete(hash_event_2);
  txn_context_destroy(&tc);
  delete_span_events(span_events, span_events_size);
}

static void test_json_print_external_spans(void) {
  int rv;
  txn_context_t* tc = txn_context_create();
  nr_span_event_t* span_events[NR_TXN_MAX_NODES + 1] = {NULL};
  int span_events_size = sizeof(span_events) / sizeof(span_events[0]);
  nrobj_t* hash;
  nrobj_t* hash_event_2;
  nr_txnnode_attributes_t* attributes1;
  nr_txnnode_attributes_t* attributes2;

  hash = nro_create_from_json(
      "{\"procedure\":\"POST\",\"uri\":\"wombats.com:8080\",\"library\":\"file_"
      "get_contents\"}");
  attributes1 = nr_txnnode_attributes_create();
  attributes1->type = NR_EXTERNAL;
  add_node_to_txn(&tc->txn, 1000, 4000, "file_get_contents/first", NULL, hash,
                  attributes1);
  nr_txnnode_attributes_destroy(attributes1);

  hash_event_2 = nro_create_from_json(
      "{\"uri\":\"example.com\",\"library\":\"Guzzle 4\"}");
  attributes2 = nr_txnnode_attributes_create();
  attributes2->type = NR_EXTERNAL;
  add_node_to_txn(&tc->txn, 2000, 4000, "Guzzle/second", NULL, hash_event_2,
                  attributes2);
  nr_txnnode_attributes_destroy(attributes2);

  txn_context_prepare(tc);

  rv = nr_traces_json_print_segments(tc->buf, &tc->txn, &tc->txn.root, 0,
                                     tc->nodes, tc->node_names, span_events,
                                     span_events_size, NULL);

  tlib_pass_if_true("correct number of spans", 2 == rv, "rv==%d", rv);
  SPAN_EVENT_COMPARE_EXTERNAL(span_events[1], "wombats.com:8080", "POST",
                              "file_get_contents");
  SPAN_EVENT_COMPARE_EXTERNAL(span_events[2], "example.com", NULL, "Guzzle 4");

  nro_delete(hash);
  nro_delete(hash_event_2);
  txn_context_destroy(&tc);
  delete_span_events(span_events, span_events_size);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_json_print_segments();
  test_json_print_segments_async();
  test_json_print_segments_async_spans();
  test_harvest_trace_create_data();
  test_harvest_trace_create_data_spans();
  test_json_print_datastore();
  test_json_print_external_spans();
}
