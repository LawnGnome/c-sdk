#include "nr_axiom.h"

#include "nr_segment_traces.h"
#include "nr_segment_tree.h"
#include "nr_segment_private.h"
#include "test_node_helpers.h"
#include "util_number_converter.h"
#include "util_set.h"

#include "tlib_main.h"

static void test_assemble_data_bad_params(void) {
  nrtxn_t txn = {0};
  size_t trace_limit = 1;
  size_t span_limit = 1;
  nr_segment_tree_result_t result = {.trace_json = NULL};

  /*
   * Test : Bad parameters
   */
  nr_segment_tree_assemble_data(&txn, NULL, trace_limit, span_limit);

  nr_segment_tree_assemble_data(NULL, &result, trace_limit, span_limit);
  tlib_pass_if_null(
      "Traversing the segments of a NULL transaction must NOT populate a "
      "result",
      result.trace_json);

  tlib_pass_if_null(
      "Traversing the segments of a segment-less transaction must NOT create "
      "an unscoped metric table",
      result.unscoped_metrics);

  tlib_pass_if_null(
      "Traversing the segments of a segment-less transaction must NOT create "
      "a scoped metric table",
      result.scoped_metrics);

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_null(
      "Traversing a segment-less transaction must NOT populate a "
      "result",
      result.trace_json);

  tlib_pass_if_null(
      "Traversing the segments of a segment-less transaction must NOT create "
      "an unscoped metric table",
      result.unscoped_metrics);

  tlib_pass_if_null(
      "Traversing the segments of a segment-less transaction must NOT create "
      "a scoped metric table",
      result.scoped_metrics);

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, 0);

  tlib_pass_if_null(
      "Traversing the segments of a segment-less transaction must NOT create "
      "an unscoped metric table",
      result.unscoped_metrics);

  tlib_pass_if_null(
      "Traversing the segments of a segment-less transaction must NOT create "
      "a scoped metric table",
      result.scoped_metrics);
}

static void test_assemble_data_one_only_with_metrics(void) {
  nrtxn_t txn = {0};
  size_t trace_limit = 1;
  size_t span_limit = 0;
  nr_segment_tree_result_t result = {.trace_json = NULL};
  nrobj_t* obj;

  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));

  root->start_time = 1000;
  root->stop_time = 4000;

  /* Mock up the transaction */
  txn.segment_count = 1;
  txn.segment_root = root;
  txn.trace_strings = nr_string_pool_create();

  /* Mock up the segment */
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  nr_segment_add_metric(root, "Custom/Unscoped", false);
  nr_segment_add_metric(root, "Custom/Scoped", true);

  txn.options.tt_threshold = 5000;

  /*
   * Test : A too-short transaction does not yield a trace.
   */

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_null(
      "Traversing the segments of a should-not-trace transaction must NOT "
      "populate a trace JSON result",
      result.trace_json);

  tlib_pass_if_not_null(
      "Traversing the segments of a should-not-trace transaction must create "
      "an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-not-trace transaction must create "
      "unscoped metrics as needed",
      1, nrm_table_size(result.unscoped_metrics));
  tlib_pass_if_not_null(
      "Traversing the segments of a should-not-trace transaction must create a "
      "specific unscoped metric",
      nrm_find(result.unscoped_metrics, "Custom/Unscoped"));

  tlib_pass_if_not_null(
      "Traversing the segments of a should-not-trace transaction must create "
      "a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-not-trace transaction must create "
      "scoped metrics as needed",
      1, nrm_table_size(result.scoped_metrics));
  tlib_pass_if_not_null(
      "Traversing the segments of a should-not-trace transaction must create a "
      "specific scoped metric",
      nrm_find(result.scoped_metrics, "Custom/Scoped"));

  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  /*
   * Test : A zero limit does not yield a trace.
   */

  /* Make the transaction long enough so that a trace should be made */
  root->stop_time = 10000;

  /* The necessity for mocking up nodes_used will be eliminated when
   * the notion of nodes is removed from nrtxn_t.  For now, mock
   * this up so that nr_txn_should_save_trace() returns true.  */
  txn.nodes_used = 1;

  nr_segment_tree_assemble_data(&txn, &result, 0, span_limit);
  tlib_pass_if_null(
      "Traversing the segments of a 0-limit trace must NOT populate a trace "
      "JSON result",
      result.trace_json);

  tlib_pass_if_not_null(
      "Traversing the segments of a 0-limit transaction must create "
      "an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a 0-limit transaction must create "
      "unscoped metrics as needed",
      1, nrm_table_size(result.unscoped_metrics));
  tlib_pass_if_not_null(
      "Traversing the segments of a 0-limit transaction must create a "
      "specific unscoped metric",
      nrm_find(result.unscoped_metrics, "Custom/Unscoped"));

  tlib_pass_if_not_null(
      "Traversing the segments of a 0-limit transaction must create "
      "a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a 0-limit transaction must create "
      "scoped metrics as needed",
      1, nrm_table_size(result.scoped_metrics));
  tlib_pass_if_not_null(
      "Traversing the segments of a 0-limit transaction must create a "
      "specific scoped metric",
      nrm_find(result.scoped_metrics, "Custom/Scoped"));

  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  /*
   * Test : Normal operation
   */

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must populate a "
      "trace JSON result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON",
      result.trace_json,
      "[[0,{},{},[0,9,\"ROOT\",{},[[0,9,\"`0\",{},[]]]],{}],["
      "\"WebTransaction\\/*\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create "
      "an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace transaction must create "
      "unscoped metrics as needed",
      1, nrm_table_size(result.unscoped_metrics));
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create a "
      "specific unscoped metric",
      nrm_find(result.unscoped_metrics, "Custom/Unscoped"));

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create "
      "a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace transaction must create "
      "scoped metrics as needed",
      1, nrm_table_size(result.scoped_metrics));
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create a "
      "specific scoped metric",
      nrm_find(result.scoped_metrics, "Custom/Scoped"));

  nro_delete(obj);
  nr_string_pool_destroy(&txn.trace_strings);
  nr_free(result.trace_json);
  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  nr_segment_destroy(root);
}

static void test_assemble_data_one_only_without_metrics(void) {
  nrtxn_t txn = {0};
  size_t trace_limit = 1;
  size_t span_limit = 0;
  nr_segment_tree_result_t result = {.trace_json = NULL};
  nrobj_t* obj;

  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));

  root->start_time = 1000;
  root->stop_time = 4000;

  /* Mock up the transaction */
  txn.segment_count = 1;
  txn.segment_root = root;
  txn.trace_strings = nr_string_pool_create();

  /* Mock up the segment */
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.options.tt_threshold = 5000;

  /*
   * Test : A too-short transaction does not yield a trace.
   */

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_null(
      "Traversing the segments of a should-not-trace transaction must NOT "
      "populate a trace JSON result",
      result.trace_json);

  tlib_pass_if_not_null(
      "Traversing the segments of a should-not-trace transaction must create "
      "an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-not-trace transaction must create "
      "unscoped metrics as needed",
      0, nrm_table_size(result.unscoped_metrics));

  tlib_pass_if_not_null(
      "Traversing the segments of a should-not-trace transaction must create "
      "a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-not-trace transaction must create "
      "scoped metrics as needed",
      0, nrm_table_size(result.scoped_metrics));

  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  /*
   * Test : A zero limit does not yield a trace.
   */

  /* Make the transaction long enough so that a trace should be made */
  root->stop_time = 10000;

  /* The necessity for mocking up nodes_used will be eliminated when
   * the notion of nodes is removed from nrtxn_t.  For now, mock
   * this up so that nr_txn_should_save_trace() returns true.  */
  txn.nodes_used = 1;

  nr_segment_tree_assemble_data(&txn, &result, 0, span_limit);
  tlib_pass_if_null(
      "Traversing the segments of a 0-limit trace must NOT populate a trace "
      "JSON result",
      result.trace_json);

  tlib_pass_if_not_null(
      "Traversing the segments of a 0-limit transaction must create "
      "an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a 0-limit transaction must create "
      "unscoped metrics as needed",
      0, nrm_table_size(result.unscoped_metrics));

  tlib_pass_if_not_null(
      "Traversing the segments of a 0-limit transaction must create "
      "a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a 0-limit transaction must create "
      "scoped metrics as needed",
      0, nrm_table_size(result.scoped_metrics));

  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  /*
   * Test : Normal operation
   */

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must populate a "
      "trace JSON result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON",
      result.trace_json,
      "[[0,{},{},[0,9,\"ROOT\",{},[[0,9,\"`0\",{},[]]]],{}],["
      "\"WebTransaction\\/*\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create "
      "an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace transaction must create "
      "unscoped metrics as needed",
      0, nrm_table_size(result.unscoped_metrics));

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create "
      "a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace transaction must create "
      "scoped metrics as needed",
      0, nrm_table_size(result.scoped_metrics));

  nro_delete(obj);
  nr_string_pool_destroy(&txn.trace_strings);
  nr_free(result.trace_json);
  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  nr_segment_destroy(root);
}

#define NR_TEST_SEGMENT_TREE_SIZE 4
static void test_assemble_data(void) {
  int i;
  nrobj_t* obj;
  nr_segment_t* current;

  nrtxn_t txn = {0};

  nrtime_t start_time = 1000;
  nrtime_t stop_time = 10000;

  size_t trace_limit = NR_TEST_SEGMENT_TREE_SIZE;
  size_t span_limit = 0;
  char* segment_names[NR_TEST_SEGMENT_TREE_SIZE];

  nr_segment_tree_result_t result = {.trace_json = NULL};
  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));

  txn.trace_strings = nr_string_pool_create();

  root->start_time = start_time;
  root->stop_time = stop_time;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;
  current = root;

  for (i = 0; i < NR_TEST_SEGMENT_TREE_SIZE; i++) {
    nr_segment_t* segment = nr_zalloc(sizeof(nr_segment_t));

    segment_names[i] = nr_alloca(5 * sizeof(char));
    nr_itoa(segment_names[i], 5, i);

    segment->start_time = start_time + ((i + 1) * 1000);
    segment->stop_time = stop_time - ((i + 1) * 1000);
    segment->name = nr_string_add(txn.trace_strings, segment_names[i]);

    nr_segment_add_metric(segment, segment_names[i], false);
    nr_segment_add_metric(segment, segment_names[i], true);

    nr_segment_children_init(&current->children);
    nr_segment_add_child(current, segment);

    current = segment;
  }

  txn.segment_count = NR_TEST_SEGMENT_TREE_SIZE;
  /* The necessity for mocking up nodes_used will be eliminated when
   * the notion of nodes is removed from nrtxn_t.  For now, mock
   * this up so that nr_txn_should_save_trace() returns true.  */
  txn.nodes_used = NR_TEST_SEGMENT_TREE_SIZE;

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-sample transaction must populate a "
      "result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON with all "
      "segments",
      result.trace_json,
      "[[0,{},{},[0,9,\"ROOT\",{},[[0,9,\"`0\",{},[[1,8,\"`1\",{},[[2,7,\"`2\","
      "{},[[3,6,\"`3\",{},[[4,5,\"`4\",{},[]]]]]]]]]]]],{}],["
      "\"WebTransaction\\/*\",\"0\",\"1\",\"2\",\"3\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create "
      "an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace transaction must create "
      "unscoped metrics as needed",
      NR_TEST_SEGMENT_TREE_SIZE, nrm_table_size(result.unscoped_metrics));

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create "
      "a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace transaction must create "
      "scoped metrics as needed",
      NR_TEST_SEGMENT_TREE_SIZE, nrm_table_size(result.scoped_metrics));

  nro_delete(obj);
  nr_string_pool_destroy(&txn.trace_strings);
  nr_free(result.trace_json);
  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  nr_segment_destroy(root);
}

static void test_assemble_data_with_sampling(void) {
  int i;
  nrobj_t* obj;
  nr_segment_t* current;

  nrtxn_t txn = {0};

  nrtime_t start_time = 1000;
  nrtime_t stop_time = 10000;

  size_t trace_limit = NR_TEST_SEGMENT_TREE_SIZE;
  size_t span_limit = 0;
  char* segment_names[NR_TEST_SEGMENT_TREE_SIZE];

  nr_segment_tree_result_t result = {.trace_json = NULL};
  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));

  txn.trace_strings = nr_string_pool_create();

  root->start_time = start_time;
  root->stop_time = stop_time;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;
  current = root;

  for (i = 0; i < NR_TEST_SEGMENT_TREE_SIZE; i++) {
    nr_segment_t* segment = nr_zalloc(sizeof(nr_segment_t));

    segment_names[i] = nr_alloca(5 * sizeof(char));
    nr_itoa(segment_names[i], 5, i);

    segment->start_time = start_time + ((i + 1) * 1000);
    segment->stop_time = stop_time - ((i + 1) * 1000);
    segment->name = nr_string_add(txn.trace_strings, segment_names[i]);

    nr_segment_add_metric(segment, segment_names[i], false);
    nr_segment_add_metric(segment, segment_names[i], true);

    nr_segment_children_init(&current->children);
    nr_segment_add_child(current, segment);

    current = segment;
  }

  txn.segment_count = NR_TEST_SEGMENT_TREE_SIZE;
  txn.nodes_used = NR_TEST_SEGMENT_TREE_SIZE;

  /*
   * Test : Normal operation with sampling
   */
  trace_limit = 2;
  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must populate a result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create expected trace JSON with two nodes only",
      result.trace_json,
      "[[0,{},{},[0,9,\"ROOT\",{},[[0,9,\"`0\",{},[[1,8,\"`1\",{},[]]]]]],{}],["
      "\"WebTransaction\\/*\",\"0\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create valid JSON",
      obj);

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create unscoped metrics as needed",
      NR_TEST_SEGMENT_TREE_SIZE, nrm_table_size(result.unscoped_metrics));

  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create scoped metrics as needed",
      NR_TEST_SEGMENT_TREE_SIZE, nrm_table_size(result.scoped_metrics));

  nro_delete(obj);
  nr_string_pool_destroy(&txn.trace_strings);
  nr_free(result.trace_json);
  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  nr_segment_destroy(root);
}

#define NR_TEST_SEGMENT_EXTENDED_TREE_SIZE 3000
static void test_assemble_data_with_extended_sampling(void) {
  int i;
  nrtxn_t txn = {0};
  size_t trace_limit = 4;
  size_t span_limit = 0;
  nr_segment_tree_result_t result = {.trace_json = NULL};
  nrobj_t* obj;
  nr_segment_t* current;
  char* segment_names[NR_TEST_SEGMENT_EXTENDED_TREE_SIZE];

  nr_segment_t* root = nr_zalloc(sizeof(nr_segment_t));

  txn.trace_strings = nr_string_pool_create();

  root->start_time = 1000;
  root->stop_time = 35000;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;
  current = root;

  for (i = 0; i < NR_TEST_SEGMENT_EXTENDED_TREE_SIZE; i++) {
    nr_segment_t* segment = nr_zalloc(sizeof(nr_segment_t));

    segment_names[i] = nr_alloca(5 * sizeof(char));
    nr_itoa(segment_names[i], 5, i);

    segment->start_time = i;
    segment->stop_time = i * 10 + 1;
    segment->name = nr_string_add(txn.trace_strings, segment_names[i]);

    nr_segment_add_metric(segment, segment_names[i], false);
    nr_segment_add_metric(segment, segment_names[i], true);

    nr_segment_children_init(&current->children);
    nr_segment_add_child(current, segment);

    current = segment;
  }

  txn.segment_count = NR_TEST_SEGMENT_EXTENDED_TREE_SIZE;

  /* The necessity for mocking up nodes_used will be eliminated when
   * the notion of nodes is removed from nrtxn_t.  For now, mock
   * this up so that nr_txn_should_save_trace() returns true.  */
  txn.nodes_used = NR_TEST_SEGMENT_EXTENDED_TREE_SIZE;

  nr_segment_tree_assemble_data(&txn, &result, trace_limit, span_limit);
  tlib_pass_if_not_null(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must populate a result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create expected trace JSON with the four longest "
      "segments",
      result.trace_json,
      "[[0,{},{},[0,34,\"ROOT\",{},[[0,34,\"`0\",{},[[1,28,\"`1\",{},[[1,28,\"`"
      "2\",{},[[1,28,\"`3\",{},[]]]]]]]]]],{}],[\"WebTransaction\\/"
      "*\",\"2997\",\"2998\",\"2999\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create valid JSON",
      obj);

  tlib_pass_if_not_null(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create an unscoped metric table",
      result.unscoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create unscoped metrics as needed, but subject to the "
      "overall metric limit",
      NR_METRIC_DEFAULT_LIMIT + 1, nrm_table_size(result.unscoped_metrics));

  tlib_pass_if_not_null(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create a scoped metric table",
      result.scoped_metrics);
  tlib_pass_if_int_equal(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create scoped metrics as needed, but subject to the "
      "overall metric limit",
      NR_METRIC_DEFAULT_LIMIT + 1, nrm_table_size(result.scoped_metrics));

  nro_delete(obj);
  nr_string_pool_destroy(&txn.trace_strings);
  nr_free(result.trace_json);
  nrm_table_destroy(&result.scoped_metrics);
  nrm_table_destroy(&result.unscoped_metrics);

  nr_segment_destroy(root);
}

static void test_nearest_sampled_ancestor(void) {
  nr_set_t* set;
  nr_segment_t* ancestor = NULL;
  nrtxn_t txn = {0};

  nr_segment_t root = {.name = 0, .txn = &txn};
  nr_segment_t A = {.name = 1, .txn = &txn};
  nr_segment_t B = {.name = 2, .txn = &txn};
  nr_segment_t child = {.name = 3, .txn = &txn};
  txn.segment_root = &root;

  /*
   *         ----------Root-----------
   *                  /
   *            -----A-----
   *                /
   *           ----B----
   *              /
   *          -child-
   */

  set = nr_set_create();
  nr_set_insert(set, (void*)&child);

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &child);

  /*
   * Test : Bad parameters
   */
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(NULL, &child);
  tlib_pass_if_null("Passing in a NULL set returns NULL", ancestor);

  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, NULL);
  tlib_pass_if_null("Passing in a NULL segment returns NULL", ancestor);

  /*
   * Test : There is no sampled ancestor
   */
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_null(
      "Passing in a set without any sampled ancestors returns NULL", ancestor);
  /*
   * Test : There is a sampled ancestor
   */
  nr_segment_add_child(&root, &A);
  nr_set_insert(set, (void*)&root);
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_ptr_equal("The returned ancestor should be the root", &root,
                         ancestor);

  nr_set_destroy(&set);
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&A.children);
  nr_segment_children_destroy_fields(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&child);
}

static void test_nearest_sampled_ancestor_cycle(void) {
  nr_set_t* set;
  nr_segment_t* ancestor = NULL;
  nrtxn_t txn = {0};

  nr_segment_t root = {.name = 0, .txn = &txn};
  nr_segment_t A = {.name = 1, .txn = &txn};
  nr_segment_t B = {.name = 2, .txn = &txn};
  nr_segment_t child = {.name = 3, .txn = &txn};

  txn.segment_root = &root;

  set = nr_set_create();
  nr_set_insert(set, (void*)&child);

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &child);

  /*
   * Test : There is a cycle in the tree that does not include the target node.
   * The target node is the only one sampled.
   */

  /*
   *         ----------Root-----------
   *                  /      |
   *            -----A-----  |
   *                /        |
   *           ----B----     |
   *              /    |     |
   *          -child-  |     |
   *                   +-->--+
   */
  nr_segment_add_child(&B, &root);
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_null(
      "Passing in a tree with a cycle and no sampled ancestors returns NULL",
      ancestor);

  // Test : There is a cycle but the node has a sampled parent.
  nr_set_insert(set, (void*)&A);
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_ptr_equal("The returned ancestor should be A", &A, ancestor);

  nr_set_destroy(&set);
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&A.children);
  nr_segment_children_destroy_fields(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&child);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_assemble_data_bad_params();
  test_assemble_data_one_only_with_metrics();
  test_assemble_data_one_only_without_metrics();
  test_assemble_data();
  test_assemble_data_with_sampling();
  test_assemble_data_with_extended_sampling();
  test_nearest_sampled_ancestor();
  test_nearest_sampled_ancestor_cycle();
}
