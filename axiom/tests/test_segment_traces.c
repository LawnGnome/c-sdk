#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_segment.h"
#include "nr_segment_private.h"
#include "nr_segment_traces.h"
#include "util_memory.h"
#include "util_minmax_heap.h"

#include "tlib_main.h"

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

static void test_json_print_bad_parameters(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};
  nr_segment_t root = {.type = NR_SEGMENT_CUSTOM,
                       .txn = &txn,
                       .start_time = 1000,
                       .stop_time = 10000};
  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /*
   * Test : Bad parameters
   */
  rv = nr_segment_traces_json_print_segments(NULL, NULL, NULL, NULL);
  tlib_pass_if_true("Return value must be -1 when input params are NULL",
                    -1 == rv, "rv=%d", rv);

  rv = nr_segment_traces_json_print_segments(NULL, &txn, &root, segment_names);
  tlib_pass_if_true("Return value must be -1 when input buff is NULL", -1 == rv,
                    "rv=%d", rv);

  rv = nr_segment_traces_json_print_segments(buf, NULL, &root, segment_names);
  tlib_pass_if_true("Return value must be -1 when input txn is NULL", -1 == rv,
                    "rv=%d", rv);

  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, NULL);
  tlib_pass_if_true("Return value must be -1 when input pool is NULL", -1 == rv,
                    "rv=%d", rv);


  /* Clean up */
  nr_string_pool_destroy(&segment_names);
  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_root_only(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};
  nr_segment_t root = {.type = NR_SEGMENT_CUSTOM,
                       .txn = &txn,
                       .start_time = 1000,
                       .stop_time = 10000};
  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 2;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();

  /* Create a single mock segment */
  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("Printing JSON for a single root segment must succeed",
                    0 == rv, "rv=%d", rv);
  test_buffer_contents("success", buf, "[0,9,\"`0\",{},[]]");

  /* Clean up */
  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_bad_segments(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  nr_segment_t root = {.type = NR_SEGMENT_CUSTOM,
                       .txn = &txn,
                       .start_time = 1000,
                       .stop_time = 10000};
  nr_segment_t child = {.type = NR_SEGMENT_CUSTOM,
                        .txn = &txn,
                        .start_time = 2000,
                        .stop_time = 2000};

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 2;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();

  /* Create a collection of mock segments */

  /*    ------root-------
   *       --child--
   *
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&child.children);

  nr_segment_add_child(&root, &child);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  child.name = nr_string_add(txn.trace_strings, "Mongo/alpha");

  /*
   * Test : Segment with bad stamps
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true(
      "Printing JSON for a segment that has equal start and stop must fail",
      -1 == rv, "rv=%d", rv);
  nr_buffer_reset(buf);

  /*
   * Test : Segment stop before segment start
   */
  child.start_time = 4000;
  child.stop_time = 2000;
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true(
      "Printing JSON for a segment that has out of order start and stop must "
      "fail",
      -1 == rv, "rv=%d", rv);
  nr_buffer_reset(buf);

  /*
   * Test : Segment start before transaction start
   */
  child.start_time = 500;
  child.stop_time = 4000;
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true(
      "Printing JSON for a segment whose start is before the transaction start "
      "must succeed",
      0 == rv, "rv=%d", rv);
  test_buffer_contents("start before txn start", buf,
                       "[0,9,\"`0\",{},[[0,3,\"`1\",{},[]]]]");
  nr_buffer_reset(buf);

  /*
   * Test : Segment start and stop before transaction start
   */
  child.start_time = 500;
  child.stop_time = 600;
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true(
      "Printing JSON for a segment whose start and stop are before the "
      "transaction start must succeed",
      0 == rv, "rv=%d", rv);
  test_buffer_contents("start before txn start", buf,
                       "[0,9,\"`0\",{},[[0,0,\"`1\",{},[]]]]");
  nr_buffer_reset(buf);

  /*
   * Test : Segment with unknown name
   */
  child.start_time = 2000;
  child.stop_time = 4000;
  child.name = 0;
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true(
      "Printing JSON for a segment with an unknown name must succeed", 0 == rv,
      "rv=%d", rv);
  test_buffer_contents("unknown name", buf,
                       "[0,9,\"`0\",{},[[1,3,\"`2\",{},[]]]]");

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&child.children);
  nr_segment_destroy_fields(&child);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segment_with_data(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t child = {.txn = &txn, .start_time = 2000, .stop_time = 4000};

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 2;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();

  /* Create a collection of mock segments */

  /*    ------root-------
   *       --child--
   *
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&child.children);

  nr_segment_add_child(&root, &child);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  child.name = nr_string_add(txn.trace_strings, "External/domain.com/all");
  child.user_attributes = nro_new_hash();
  nro_set_hash_string(child.user_attributes, "uri", "domain.com");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("Printing JSON for a segment with data must succeed",
                    0 == rv, "rv=%d", rv);
  test_buffer_contents("node with data", buf,
                       "[0,9,\"`0\",{},"
                       "[[1,3,\"`1\",{\"uri\":\"domain.com\"},[]]]]");
  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&child.children);
  nr_segment_destroy_fields(&child);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_two_nodes(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t child = {.txn = &txn, .start_time = 2000, .stop_time = 4000};

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 2;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();

  /* Create a collection of mock segments */

  /*    ------root-------
   *       --child--
   *
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&child.children);

  nr_segment_add_child(&root, &child);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  child.name = nr_string_add(txn.trace_strings, "Mongo/alpha");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("Printing JSON for a root+child pair must succeed", 0 == rv,
                    "rv=%d", rv);
  test_buffer_contents("success", buf, "[0,9,\"`0\",{},[[1,3,\"`1\",{},[]]]]");

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&child.children);
  nr_segment_destroy_fields(&child);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_hanoi(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t A = {.txn = &txn, .start_time = 2000, .stop_time = 7000};
  nr_segment_t B = {.txn = &txn, .start_time = 3000, .stop_time = 6000};
  nr_segment_t C = {.txn = &txn, .start_time = 4000, .stop_time = 5000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 4;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();

  /* Create a collection of mock segments */

  /*    ------root-------
   *       ----A----
   *       ----B----
   *         --C--
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("Printing JSON for a cascade of four segments must succeed",
                    0 == rv, "rv=%d", rv);
  test_buffer_contents("towers of hanoi", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{},[[2,5,\"`2\",{},[[3,4,"
                       "\"`3\",{},[]]]]]]]]");

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&A.children);
  nr_segment_children_destroy_fields(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_three_siblings(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t A = {.txn = &txn, .start_time = 2000, .stop_time = 3000};
  nr_segment_t B = {.txn = &txn, .start_time = 4000, .stop_time = 5000};
  nr_segment_t C = {.txn = &txn, .start_time = 6000, .stop_time = 7000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 4;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();

  /* Create a collection of mock segments */

  /*
   *  --A--  --B--  --C--
   */

  nr_segment_children_init(&root.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&root, &B);
  nr_segment_add_child(&root, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("Printing JSON for a rooted set of triplets must succeed",
                    0 == rv, "rv=%d", rv);
  test_buffer_contents("sequential nodes", buf,
                       "[0,9,\"`0\",{},[[1,2,\"`1\",{},[]],[3,4,\"`2\",{},[]],["
                       "5,6,\"`3\",{},[]]]]");

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_two_generations(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t A = {.txn = &txn, .start_time = 2000, .stop_time = 7000};
  nr_segment_t B = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  nr_segment_t C = {.txn = &txn, .start_time = 5000, .stop_time = 6000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 4;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();

  /* Create a collection of mock segments */

  /*    ------root-------
   *     ------A------
   *      --B-- --C--
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&A, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("success", 0 == rv, "rv=%d", rv);
  test_buffer_contents("two kids", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{},[[2,3,\"`2\",{},[]],[4,"
                       "5,\"`3\",{},[]]]]]]");

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&A.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_async_basic(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

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

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t loop_segment = {.txn = &txn, .start_time = 2000, .stop_time = 4000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 3;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();
  txn.async_duration = 1;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &loop_segment);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  loop_segment.name = nr_string_add(txn.trace_strings, "loop");
  loop_segment.async_context = nr_string_add(txn.trace_strings, "async");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("Printing JSON for a basic async scenario must succeed",
                    0 == rv, "rv=%d", rv);
  test_buffer_contents("basic", buf,
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

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&main_segment.children);

  nr_segment_destroy_fields(&main_segment);
  nr_segment_destroy_fields(&loop_segment);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_async_multi_child(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  /*
   * Multiple children test: main context lasts the same timespan as ROOT, and
   * spawns one child context with three nodes for part of its run time, one of
   * which has a duplicated name.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |--- a_a ---|--- b ---|    | a_b  |
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t a_a = {.txn = &txn, .start_time = 2000, .stop_time = 4000};
  nr_segment_t b = {.txn = &txn, .start_time = 4000, .stop_time = 6000};
  nr_segment_t a_b = {.txn = &txn, .start_time = 7000, .stop_time = 8000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 5;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();
  txn.async_duration = 1;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &a_a);
  nr_segment_add_child(&main_segment, &b);
  nr_segment_add_child(&main_segment, &a_b);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  a_a.name = nr_string_add(txn.trace_strings, "a");
  a_a.async_context = nr_string_add(txn.trace_strings, "async");

  b.name = nr_string_add(txn.trace_strings, "b");
  b.async_context = nr_string_add(txn.trace_strings, "async");

  a_b.name = nr_string_add(txn.trace_strings, "a");
  a_b.async_context = nr_string_add(txn.trace_strings, "async");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("success", 0 == rv, "rv=%d", rv);
  test_buffer_contents(
      "Printing JSON for a three-child async scenario must succeed", buf,
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

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&main_segment.children);

  nr_segment_destroy_fields(&main_segment);
  nr_segment_destroy_fields(&a_a);
  nr_segment_destroy_fields(&b);
  nr_segment_destroy_fields(&a_b);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_async_multi_context(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  /*
   * Multiple contexts test: main context lasts the same timespan as ROOT, and
   * spawns three child contexts with a mixture of nodes.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * alpha                          |--- a_a --|--- b --|   | a_b |
   * beta                                |--- c ---|
   * gamma                                                             | d  |
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t a_a = {.txn = &txn, .start_time = 2000, .stop_time = 4000};
  nr_segment_t b = {.txn = &txn, .start_time = 4000, .stop_time = 6000};
  nr_segment_t a_b = {.txn = &txn, .start_time = 7000, .stop_time = 8000};
  nr_segment_t c = {.txn = &txn, .start_time = 3000, .stop_time = 5000};
  nr_segment_t d = {.txn = &txn, .start_time = 9000, .stop_time = 10000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 5;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();
  txn.async_duration = 1;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &a_a);
  nr_segment_add_child(&main_segment, &b);
  nr_segment_add_child(&main_segment, &a_b);
  nr_segment_add_child(&main_segment, &c);
  nr_segment_add_child(&main_segment, &d);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  a_a.name = nr_string_add(txn.trace_strings, "a");
  a_a.async_context = nr_string_add(txn.trace_strings, "alpha");

  b.name = nr_string_add(txn.trace_strings, "b");
  b.async_context = nr_string_add(txn.trace_strings, "alpha");

  a_b.name = nr_string_add(txn.trace_strings, "a");
  a_b.async_context = nr_string_add(txn.trace_strings, "alpha");

  c.name = nr_string_add(txn.trace_strings, "c");
  c.async_context = nr_string_add(txn.trace_strings, "beta");

  d.name = nr_string_add(txn.trace_strings, "d");
  d.async_context = nr_string_add(txn.trace_strings, "gamma");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("success", 0 == rv, "rv=%d", rv);
  test_buffer_contents("multiple contexts", buf,
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

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&main_segment.children);
  nr_segment_destroy_fields(&main_segment);

  nr_segment_destroy_fields(&a_a);
  nr_segment_destroy_fields(&b);
  nr_segment_destroy_fields(&a_b);
  nr_segment_destroy_fields(&c);
  nr_segment_destroy_fields(&d);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_async_context_nesting(void) {
  int rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

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

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t a = {.txn = &txn, .start_time = 2000, .stop_time = 4000};
  nr_segment_t b = {.txn = &txn, .start_time = 4000, .stop_time = 7000};
  nr_segment_t g = {.txn = &txn, .start_time = 7200, .stop_time = 8000};

  /* b begets f and c, in that order */
  nr_segment_t f = {.txn = &txn, .start_time = 5000, .stop_time = 7000};
  nr_segment_t c = {.txn = &txn, .start_time = 6000, .stop_time = 7000};

  /* a begets d */
  nr_segment_t d = {.txn = &txn, .start_time = 3000, .stop_time = 10000};

  /* d begets e */
  nr_segment_t e = {.txn = &txn, .start_time = 5000, .stop_time = 7000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 9;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();
  txn.async_duration = 1;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);
  nr_segment_children_init(&a.children);
  nr_segment_children_init(&b.children);
  nr_segment_children_init(&d.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &a);
  nr_segment_add_child(&main_segment, &b);

  nr_segment_add_child(&main_segment, &g);

  nr_segment_add_child(&a, &d);
  nr_segment_add_child(&d, &e);

  nr_segment_add_child(&b, &f);
  nr_segment_add_child(&b, &c);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  a.name = nr_string_add(txn.trace_strings, "a");
  b.name = nr_string_add(txn.trace_strings, "b");
  c.name = nr_string_add(txn.trace_strings, "c");
  d.name = nr_string_add(txn.trace_strings, "d");
  d.async_context = nr_string_add(txn.trace_strings, "alpha");

  e.name = nr_string_add(txn.trace_strings, "e");
  e.async_context = nr_string_add(txn.trace_strings, "alpha");

  f.name = nr_string_add(txn.trace_strings, "f");
  f.async_context = nr_string_add(txn.trace_strings, "beta");

  g.name = nr_string_add(txn.trace_strings, "g");
  g.async_context = nr_string_add(txn.trace_strings, "gamma");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("success", 0 == rv, "rv=%d", rv);
  test_buffer_contents("context nesting", buf,
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

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_destroy_fields(&main_segment.children);
  nr_segment_children_destroy_fields(&a.children);
  nr_segment_children_destroy_fields(&b.children);
  nr_segment_children_destroy_fields(&d.children);

  nr_segment_destroy_fields(&main_segment);
  nr_segment_destroy_fields(&a);
  nr_segment_destroy_fields(&b);
  nr_segment_destroy_fields(&c);
  nr_segment_destroy_fields(&d);
  nr_segment_destroy_fields(&e);
  nr_segment_destroy_fields(&f);
  nr_segment_destroy_fields(&g);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_async_with_data(void) {
  int rv;
  nrbuf_t* buf;
  nrobj_t* hash;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  /*
   * Data hash testing: ensure that we never overwrite a data hash, and also
   * ensure that we never modify it.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |- loop --|
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t loop = {.txn = &txn, .start_time = 2000, .stop_time = 4000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 5;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();
  txn.async_duration = 1;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &loop);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  loop.name = nr_string_add(txn.trace_strings, "loop");
  loop.async_context = nr_string_add(txn.trace_strings, "async");

  hash = nro_create_from_json("{\"foo\":\"bar\"}");
  main_segment.user_attributes = hash;
  loop.user_attributes = hash;

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, &txn, &root, segment_names);
  tlib_pass_if_true("success", 0 == rv, "rv=%d", rv);
  test_buffer_contents(
      "basic", buf,
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

  /* Clean up */
  nr_segment_children_destroy_fields(&root.children);
  nr_segment_children_destroy_fields(&main_segment.children);

  nr_segment_destroy_fields(&root);
  nr_segment_destroy_fields(&main_segment);

  nr_string_pool_destroy(&txn.trace_strings);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}
static void test_segment_trace_tree_to_heap(void) {
  nr_segment_t mini = {.start_time = 100, .stop_time = 200};
  nr_segment_t midi = {.start_time = 100, .stop_time = 300};
  nr_segment_t maxi = {.start_time = 100, .stop_time = 400};

  /*
   * Test : Normal operation.  Insert three segments into a
   * two-slot heap and affirm that the expected pair are
   * the min and max members of the heap.  It's an
   * indirect way of testing that nr_segment_compare()
   * is working, but I want to affirm all the right pieces
   * are in place for a heap of segments.
   */
  nr_minmax_heap_t* heap = nr_segment_traces_heap_create(2);

  nr_minmax_heap_insert(heap, (void*)&mini);
  nr_minmax_heap_insert(heap, (void*)&midi);
  nr_minmax_heap_insert(heap, (void*)&maxi);

  tlib_pass_if_ptr_equal(
      "After inserting the midi-value segment, it must be the min value in the "
      "heap",
      nr_minmax_heap_peek_min(heap), &midi);

  tlib_pass_if_ptr_equal(
      "After inserting the max-value segment, it must be the max value in the "
      "heap",
      nr_minmax_heap_peek_max(heap), &maxi);

  nr_minmax_heap_destroy(&heap);
}

static void test_trace_create_data_bad_parameters(void) {
  nrtxn_t txn = {0};
  char* out;
  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");

  /*
   * Test : Bad parameters
   */
  out = nr_segment_traces_create_data(
      NULL, 2 * NR_TIME_DIVISOR, agent_attributes, user_attributes, intrinsics);
  tlib_pass_if_null(
      "A NULL transaction pointer must not succeed in creating a trace", out);

  out = nr_segment_traces_create_data(
      &txn, 2 * NR_TIME_DIVISOR, agent_attributes, user_attributes, intrinsics);
  tlib_pass_if_null(
      "A zero-sized transaction must not succeed in creating a trace", out);

  txn.segment_count = 1;
  out = nr_segment_traces_create_data(&txn, 0, agent_attributes,
                                      user_attributes, intrinsics);
  tlib_pass_if_null(
      "A zero-duration transaction must not succeed in creating a trace", out);


  txn.segment_count = 2001;
    out = nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, agent_attributes,
                                        user_attributes, intrinsics);
    tlib_pass_if_null(
        "A transaction with more than 2000 segments must not succeed in creating a trace", out);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
}

static void test_trace_create_data(void) {
  nrtxn_t txn = {0};
  char* out;
  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");
  nrobj_t* obj;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 1000, .stop_time = 10000};
  nr_segment_t A = {.txn = &txn, .start_time = 2000, .stop_time = 3000};
  nr_segment_t B = {.txn = &txn, .start_time = 4000, .stop_time = 5000};
  // clang-format on

  /* Mock up a transaction */
  txn.segment_count = 3;
  txn.segment_root = &root;
  txn.trace_strings = nr_string_pool_create();
  txn.name = nr_strdup("WebTransaction/*");

  /* Mock up a tree of segments */
  /* Create a collection of mock segments */

  /*    ------root-------
   *      --A-- --B--
   */

  nr_segment_children_init(&root.children);
  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&root, &B);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");

  /*
   * Test : Normal operation
   */

  out = nr_segment_traces_create_data(
      &txn, 2 * NR_TIME_DIVISOR, agent_attributes, user_attributes, intrinsics);

  tlib_pass_if_str_equal(
      "A multi-node transaction must succeed in creating a trace", out,
      "[[0,{},{},[0,2000,\"ROOT\",{},[[0,9,\"`0\",{},[[1,2,"
      "\"`1\",{},[]],[3,4,\"`2\",{},[]]]]]],"
      "{\"agentAttributes\":[\"agent_attributes\"],"
      "\"userAttributes\":[\"user_attributes\"],"
      "\"intrinsics\":[\"intrinsics\"]}],"
      "[\"WebTransaction\\/*\",\"A\",\"B\"]]");

  obj = nro_create_from_json(out);
  tlib_pass_if_not_null(
      "A multi-node transaction must succeed in creating valid json", obj);

  /* Clean up */
  nro_delete(obj);
  nr_free(out);
  nr_free(txn.name);

  nr_segment_children_destroy_fields(&root.children);
  nr_segment_destroy_fields(&root);
  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);

  nr_string_pool_destroy(&txn.trace_strings);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_json_print_bad_parameters();
  test_json_print_segments_root_only();
  test_json_print_segments_bad_segments();
  test_json_print_segment_with_data();
  test_json_print_segments_two_nodes();
  test_json_print_segments_hanoi();
  test_json_print_segments_three_siblings();
  test_json_print_segments_two_generations();
  test_json_print_segments_async_basic();
  test_json_print_segments_async_multi_child();
  test_json_print_segments_async_multi_context();
  test_json_print_segments_async_context_nesting();
  test_json_print_segments_async_with_data();
  test_segment_trace_tree_to_heap();
  test_trace_create_data_bad_parameters();
  test_trace_create_data();
}
