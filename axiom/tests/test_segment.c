#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_segment_private.h"
#include "nr_segment.h"
#include "util_memory.h"

#include "tlib_main.h"

/* A simple list type to affirm that traversing the tree of segments in
 * prefix order is correct.  This type is for test purposes only.
 */
#define NR_TEST_LIST_CAPACITY 10
typedef struct _nr_test_list_t {
  size_t capacity;
  int used;
  nr_segment_t* elements[NR_TEST_LIST_CAPACITY];
} nr_test_list_t;

static void test_iterator_callback(nr_segment_t* segment, void* userdata) {
  if (nrunlikely(NULL == segment || NULL == userdata)) {
    return;
  } else {
    nr_test_list_t* list = (nr_test_list_t*)userdata;
    list->elements[list->used] = segment;
    list->used = list->used + 1;
  }
}

static void test_segment_start(void) {
  nr_segment_t* s = NULL;
  nr_segment_t* t = NULL;
  nr_segment_t* u = NULL;

  nr_segment_t* prev_parent = NULL;

  nrtxn_t txnv = {0};

  /* Mock up the parent stack used by the txn */
  nr_stack_init(&txnv.parent_stack, 32);

  prev_parent = nr_txn_get_current_segment(&txnv);
  tlib_pass_if_null(
      "Before any segments are started, the current segment must be NULL",
      prev_parent);

  /*
   * Test : Bad parameters.
   */
  s = nr_segment_start(NULL, NULL, NULL);
  tlib_pass_if_null("Starting a segment on a NULL txn must not succeed", s);

  /*
   * Test : Normal operation.
   *
   * Starting a segment with a NULL, or implicit, parent. The parenting
   * information for the segment must be supplied by the parent_stack on the
   * transaction. Let's start and end three segments and make sure that the
   * family of segments is well-formed.  Affirm that the parent, child, and
   * sibling relationships are all as expected.
   */

  /* Start a segment and affirm that it is well-formed */
  s = nr_segment_start(&txnv, NULL, NULL);
  tlib_pass_if_not_null("Starting a segment on a valid txn must succeed", s);

  tlib_pass_if_ptr_equal(
      "The most-recently started segment must be the transaction's current "
      "segment",
      nr_txn_get_current_segment(&txnv), s);

  tlib_pass_if_not_null(
      "Starting a segment on a valid txn must allocate space for children",
      &s->children);

  tlib_pass_if_uint64_t_equal("A started segment has default color WHITE",
                              s->color, NR_SEGMENT_WHITE);
  tlib_pass_if_uint64_t_equal("A started segment has default type CUSTOM",
                              s->type, NR_SEGMENT_CUSTOM);
  tlib_pass_if_ptr_equal("A started segment must save its transaction", s->txn,
                         &txnv);
  tlib_fail_if_uint64_t_equal("A started segment has an initialized start time",
                              s->start_time, 0);
  tlib_pass_if_not_null("A started segment has a hash for user attributes",
                        s->user_attributes);
  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the previously "
      "current segment as parent",
      s->parent, prev_parent);

  /* Start and end a second segment, t */
  prev_parent = nr_txn_get_current_segment(&txnv);
  t = nr_segment_start(&txnv, NULL, NULL);
  tlib_pass_if_not_null("Starting a segment on a valid txn must succeed", t);
  tlib_pass_if_ptr_equal(
      "The most recently started segment must be the transaction's current "
      "segment",
      nr_txn_get_current_segment(&txnv), t);

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the previously "
      "current segment as parent",
      t->parent, prev_parent);

  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(t), "Expected true");

  tlib_pass_if_ptr_equal(
      "The most recently started segment has ended; the current segment must "
      "be its parent",
      nr_txn_get_current_segment(&txnv), s);

  /* Start a third segment.  Its sibling should be the second segment, t */
  prev_parent = nr_txn_get_current_segment(&txnv);
  u = nr_segment_start(&txnv, NULL, NULL);
  tlib_pass_if_not_null("Starting a segment on a valid txn must succeed", u);
  tlib_pass_if_ptr_equal(
      "The most recently started segment must be the transaction's current "
      "segment",
      nr_txn_get_current_segment(&txnv), u);

  tlib_pass_if_ptr_equal(
      "A segment started with an implicit parent must have the previously "
      "current segment as parent",
      u->parent, prev_parent);

  tlib_pass_if_ptr_equal(
      "A segment started with a NULL parent must have the expected "
      "previous siblings",
      nr_segment_children_get_prev(&s->children, u), t);

  tlib_pass_if_null(
      "A segment started with a NULL parent must have the expected "
      "next siblings",
      nr_segment_children_get_next(&s->children, u));

  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(u), "Expected true");

  /* Let the matriarch sleep */
  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(s), "Expected true");

  /* Clean up */
  nr_segment_children_destroy_fields(&s->children);
  nr_segment_destroy_fields(s);

  nr_segment_children_destroy_fields(&t->children);
  nr_segment_destroy_fields(t);

  nr_segment_children_destroy_fields(&u->children);
  nr_segment_destroy_fields(u);

  nr_stack_destroy_fields(&txnv.parent_stack);

  nr_free(s);
  nr_free(t);
  nr_free(u);
}

static void test_segment_start_async(void) {
  nr_segment_t* s = NULL;
  nr_segment_t first_born;
  nr_segment_t third_born;

  nrtxn_t txnv = {0};
  nr_segment_t mother
      = {.type = NR_SEGMENT_CUSTOM, .txn = &txnv, .parent = NULL};

  /* Mock up the parent stack used by the txn */
  nr_stack_init(&txnv.parent_stack, 32);
  txnv.trace_strings = nr_string_pool_create();

  /*
   * Test : Bad parameters.
   */
  s = nr_segment_start(NULL, &mother, "async_context");
  tlib_pass_if_null("Starting a segment on a NULL txn must not succeed", s);

  /*
   * Test : Async operation. Starting a segment with an explicit parent,
   * supplied as a parameter to nr_segment_start().
   */

  /* Build a mock parent with an array of children */
  nr_segment_children_init(&mother.children);
  nr_segment_children_add(&mother.children, &first_born);

  s = nr_segment_start(&txnv, &mother, "async_context");
  tlib_pass_if_not_null(
      "Starting a segment on a valid txn and an explicit parent must succeed",
      s);

  tlib_pass_if_null(
      "The most recently started, explicitly-parented segment must not alter "
      "the transaction's current segment",
      nr_txn_get_current_segment(&txnv));

  tlib_pass_if_not_null(
      "Starting a segment on a valid txn must allocate space for children",
      &s->children);
  tlib_pass_if_uint64_t_equal("A started segment has default type CUSTOM",
                              s->type, NR_SEGMENT_CUSTOM);
  tlib_pass_if_ptr_equal("A started segment must save its transaction", s->txn,
                         &txnv);
  tlib_fail_if_uint64_t_equal("A started segment has an initialized start time",
                              s->start_time, 0);
  tlib_pass_if_not_null("A started segment has a hash for user attributes",
                        s->user_attributes);
  tlib_pass_if_int_equal(
      "A started segment has an initialized async context", s->async_context,
      nr_string_find(s->txn->trace_strings, "async_context"));

  tlib_pass_if_ptr_equal(
      "A segment started with an explicit parent must have the explicit "
      "parent",
      s->parent, &mother);

  tlib_pass_if_ptr_equal(
      "A segment started with an explicit parent must have the explicit "
      "previous siblings",
      nr_segment_children_get_prev(&mother.children, s), &first_born);

  nr_segment_children_add(&mother.children, &third_born);
  tlib_pass_if_ptr_equal(
      "A segment started with an explicit parent must have the explicit "
      "next siblings",
      nr_segment_children_get_next(&mother.children, s), &third_born);

  /* Clean up */
  nr_segment_children_destroy_fields(&mother.children);
  nr_segment_destroy_fields(&mother);

  nr_segment_children_destroy_fields(&s->children);
  nr_segment_destroy_fields(s);

  nr_stack_destroy_fields(&txnv.parent_stack);
  nr_string_pool_destroy(&txnv.trace_strings);

  nr_free(s);
}

static void test_set_name(void) {
  nrtxn_t txnv = {0};
  nr_segment_t segment
      = {.type = NR_SEGMENT_CUSTOM, .txn = &txnv, .parent = NULL};

  /* Mock up transaction */
  txnv.trace_strings = nr_string_pool_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Setting a name on a NULL segment must not succeed",
                     nr_segment_set_name(NULL, "name"), "Expected false");

  tlib_pass_if_false("Setting a NULL name on a segment must not succeed",
                     nr_segment_set_name(&segment, NULL), "Expected false");

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_true("Setting a name on a segment must succeed",
                    nr_segment_set_name(&segment, "name"), "Expected true");

  tlib_pass_if_int_equal("A named segment has the expected name", segment.name,
                         nr_string_find(segment.txn->trace_strings, "name"));

  /* Clean up */
  nr_string_pool_destroy(&txnv.trace_strings);
}

static void test_add_child(void) {
  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Adding a NULL child to a parent must not succeed",
                     nr_segment_add_child(&mother, NULL), "Expected false");

  tlib_pass_if_false("Adding a child to a NULL parent must not succeed",
                     nr_segment_add_child(NULL, &segment), "Expected false");
}

static void test_set_parent_to_same(void) {
  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Setting a parent on a NULL segment must not succeed",
                     nr_segment_set_parent(NULL, &mother), "Expected false");

  /*
   *  Test : Normal operation.
   */
  tlib_pass_if_true(
      "Setting a well-formed segment with the same parent must succeed",
      nr_segment_set_parent(&mother, NULL), "Expected true");

  tlib_pass_if_null(
      "Setting a well-formed segment with a NULL parent means the segment must "
      "have a NULL parent",
      mother.parent);
}

static void test_set_null_parent(void) {
  nr_segment_t thing_one = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t thing_two = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /* Build mock segments, each with an array of children */
  nr_segment_children_init(&mother.children);
  nr_segment_add_child(&mother, &thing_one);

  nr_segment_children_init(&segment.children);
  nr_segment_add_child(&segment, &thing_two);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         thing_one.parent, &mother);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         thing_two.parent, &segment);

  /*
   *  Test : Normal operation.  Reparent a segment with a NULL parent.
   */
  tlib_pass_if_true(
      "Setting a well-formed segment with a new parent must succeed",
      nr_segment_set_parent(&segment, &mother), "Expected true");

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have that new parent",
      segment.parent, &mother);

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected prev siblings",
      nr_segment_children_get_prev(&mother.children, &segment), &thing_one);

  tlib_pass_if_null(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected next siblings",
      nr_segment_children_get_next(&mother.children, &segment));

  /* Clean up */
  nr_segment_children_destroy_fields(&mother.children);
  nr_segment_destroy_fields(&mother);

  nr_segment_children_destroy_fields(&segment.children);
  nr_segment_destroy_fields(&segment);
}

static void test_set_non_null_parent(void) {
  nr_segment_t thing_one = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t thing_two = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  nr_segment_t mother = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};

  /* Build mock segments, each with an array of children */
  nr_segment_children_init(&segment.children);
  nr_segment_add_child(&segment, &thing_two);

  nr_segment_children_init(&mother.children);
  nr_segment_add_child(&mother, &thing_one);
  nr_segment_add_child(&mother, &segment);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         thing_one.parent, &mother);

  tlib_pass_if_ptr_equal("Affirm my nuclear family is well-formed",
                         segment.parent, &mother);
  /*
   *  Test : Normal operation.  Re-parent a segment with a non-NULL parent.
   */
  tlib_pass_if_true(
      "Setting a well-formed segment with a new parent must succeed",
      nr_segment_set_parent(&thing_one, &segment), "Expected true");

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have that new parent",
      thing_one.parent, &segment);

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected prev siblings",
      nr_segment_children_get_prev(&segment.children, &thing_one), &thing_two);

  tlib_pass_if_null(
      "Setting a well-formed segment with a new parent means the segment must "
      "have expected next siblings",
      nr_segment_children_get_next(&segment.children, &thing_one));

  tlib_pass_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the old parent "
      "must "
      "have a new first child",
      mother.children.children[0], &segment);

  tlib_fail_if_ptr_equal(
      "Setting a well-formed segment with a new parent means the segment must "
      "not be a child of its old parent",
      mother.children.children[0], &thing_one);

  /* Clean up */
  nr_segment_children_destroy_fields(&mother.children);
  nr_segment_destroy_fields(&mother);

  nr_segment_children_destroy_fields(&segment.children);
  nr_segment_destroy_fields(&segment);
}

static void test_set_timing(void) {
  nr_segment_t segment = {.start_time = 1234, .stop_time = 5678};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Setting timing on a NULL segment must not succeed",
                     nr_segment_set_timing(NULL, 0, 0), "Expected false");

  /*
   *  Test : Normal operation.
   */
  tlib_pass_if_true("Setting timing on a well-formed segment must succeed",
                    nr_segment_set_timing(&segment, 2000, 5), "Expected false");

  tlib_pass_if_true(
      "Setting timing on a well-formed segment must alter the start time",
      2000 == segment.start_time, "Expected true");

  tlib_pass_if_true(
      "Setting timing on a well-formed segment must alter the stop time",
      2005 == segment.stop_time, "Expected true");
}

static void test_end_segment(void) {
  nrtxn_t txnv = {.segment_count = 0};
  nr_segment_t s = {.start_time = 1234, .stop_time = 0, .txn = &txnv};
  nr_segment_t t = {.start_time = 1234, .stop_time = 5678, .txn = &txnv};
  nr_segment_t u = {.txn = 0};

  /* Mock up the parent stack used by the txn */
  nr_stack_init(&txnv.parent_stack, 32);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Ending a NULL segment must not succeed",
                     nr_segment_end(NULL), "Expected false");

  tlib_pass_if_false("Ending a segment with a NULL txn must not succeed",
                     nr_segment_end(&u), "Expected false");

  tlib_pass_if_true(
      "Ending ill-formed segments must not alter the transaction's segment "
      "count",
      0 == txnv.segment_count, "Expected true");

  /*
   * Test : Normal operation. Ending a segment with
   * stop_time = 0.
   */
  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(&s), "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment with a zero-value stop "
      "time must alter the stop time",
      0 != t.stop_time, "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment must increment the "
      "transaction's segment count by 1",
      1 == txnv.segment_count, "Expected true");

  /*
   * Test : Normal operation. Ending a segment with stop_time != 0.
   */
  tlib_pass_if_true("Ending a well-formed segment must succeed",
                    nr_segment_end(&t), "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment with a non-zero stop "
      "time must not alter the stop time",
      5678 == t.stop_time, "Expected true");

  tlib_pass_if_true(
      "Ending a well-formed segment must increment the transaction's segment "
      "count by 1",
      2 == txnv.segment_count, "Expected true");

  /* Clean up */
  nr_stack_destroy_fields(&txnv.parent_stack);
}

static void test_segment_iterate_nulls(void) {
  nr_segment_t segment = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /*
   * Test : Bad parameters.
   */
  nr_segment_iterate(NULL, (nr_segment_iter_t)test_iterator_callback, &list);
  nr_segment_iterate(&segment, (nr_segment_iter_t)NULL, &list);
  nr_segment_iterate(&segment, (nr_segment_iter_t)test_iterator_callback, NULL);

  tlib_pass_if_int_equal(
      "Traversing with bad parameters must result in an empty list", list.used,
      0);
}

static void test_segment_iterate_bachelor(void) {
  nr_segment_t bachelor_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t bachelor_2 = {.type = NR_SEGMENT_CUSTOM, .name = 2};

  nr_test_list_t list_1 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};
  nr_test_list_t list_2 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Bachelor 1 has no room for children; Bachelor 2 does.  Each
   * bachelor needs be regarded as a leaf node.
   */
  nr_segment_children_init(&bachelor_2.children);

  /*
   * Test : Normal operation.  Traversing a tree of 1.
   */
  nr_segment_iterate(&bachelor_1, (nr_segment_iter_t)test_iterator_callback,
                     &list_1);

  tlib_pass_if_int_equal(
      "Traversing a tree of one node must create a one-node list",
      list_1.elements[0]->name, bachelor_1.name);

  /*
   * Test : Normal operation.  Traversing a tree of 1, where
   *        the node has allocated room for children.
   */
  nr_segment_iterate(&bachelor_2, (nr_segment_iter_t)test_iterator_callback,
                     &list_2);

  tlib_pass_if_int_equal(
      "Traversing a tree of one node must create a one-node list",
      list_2.elements[0]->name, bachelor_2.name);

  /* Clean up */
  nr_segment_children_destroy_fields(&bachelor_2.children);
}

static void test_segment_iterate(void) {
  int i;
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare eight segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};

  nr_segment_t grown_child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t grown_child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 3};
  nr_segment_t grown_child_3 = {.type = NR_SEGMENT_CUSTOM, .name = 7};

  nr_segment_t child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 2};
  nr_segment_t child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 4};
  nr_segment_t child_3 = {.type = NR_SEGMENT_CUSTOM, .name = 5};
  nr_segment_t child_4 = {.type = NR_SEGMENT_CUSTOM, .name = 6};

  /* Build a mock tree of segments */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_2);
  nr_segment_add_child(&grandmother, &grown_child_3);

  nr_segment_children_init(&grown_child_1.children);
  nr_segment_add_child(&grown_child_1, &child_1);

  nr_segment_children_init(&grown_child_2.children);
  nr_segment_add_child(&grown_child_2, &child_2);
  nr_segment_add_child(&grown_child_2, &child_3);
  nr_segment_add_child(&grown_child_2, &child_4);

  /*
   * The mock tree looks like this:
   *
   *               --------(0)grandmother---------
   *                /             |              \
   *    (1)grown_child_1   (3)grown_child_2    (7)grown_child_3
   *       /                /      |      \
   * (2)child_1    (4)child_2  (5)child_3  (6)child_4
   *
   *
   * In pre-order, that's: 0 1 2 3 4 5 6 7
   */

  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has eight elements", 8, list.used);


  for (i = 0; i < list.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal("A traversed tree must supply grey nodes",
                                list.elements[i]->color, NR_SEGMENT_GREY);
  }

  /* Clean up */
  nr_segment_children_destroy_fields(&grandmother.children);
  nr_segment_children_destroy_fields(&grown_child_1.children);
  nr_segment_children_destroy_fields(&grown_child_2.children);
}

/* The C Agent API gives customers the ability to arbitrarily parent a segment
 * with any other segment.  It is  possible that one could make a mistake in
 * manually parenting segments and introduce a cycle into the tree.  Test that
 * tree iteration is hardened against this possibility.
 */
static void test_segment_iterate_cycle_one(void) {
  int i;
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare three segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};
  nr_segment_t grown_child = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t child = {.type = NR_SEGMENT_CUSTOM, .name = 2};

  /* Build a mock ill-formed tree of segments; the tree shall have a cycle. */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child);

  nr_segment_children_init(&grown_child.children);
  nr_segment_add_child(&grown_child, &child);

  nr_segment_children_init(&child.children);
  nr_segment_add_child(&child, &grandmother);

  /*
   * The ill-formed tree looks like this:
   *
   *                      +-----<------+
   *                      |            |
   *               (0)grandmother      |
   *                      |            |
   *                (1)grown_child     |
   *                      |            |
   *                  (2)child         |
   *                      |            |
   *                      +----->------+
   *
   * In pre-order, that's: 0 1 2
   *     but oooooh, there's a cycle.  That's not a tree, it's a graph!
   */
  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has three elements", 3, list.used);


  for (i = 0; i < list.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A one-time traversed tree must supply grey nodes",
        list.elements[i]->color, NR_SEGMENT_GREY);
  }

  /* Clean up */
  nr_segment_children_destroy_fields(&grandmother.children);
  nr_segment_children_destroy_fields(&grown_child.children);
  nr_segment_children_destroy_fields(&child.children);
}

static void test_segment_iterate_cycle_two(void) {
  int i;
  nr_test_list_t list_1 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};
  nr_test_list_t list_2 = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare three segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};
  nr_segment_t grown_child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t grown_child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 2};
  nr_segment_t child = {.type = NR_SEGMENT_CUSTOM, .name = 3};

  /* Build a mock ill-formed tree of segments; the tree shall have a cycle. */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_2);

  nr_segment_children_init(&grown_child_1.children);
  nr_segment_add_child(&grown_child_1, &grandmother);

  nr_segment_children_init(&grown_child_2.children);
  nr_segment_add_child(&grown_child_2, &child);

  /*
   * The ill-formed tree looks like this:
   *
   *  +---------->----------+
   *  |                      |
   *  |               (0)grandmother
   *  |                 /       \
   *  |    (1)grown_child_1    (2)grown_child_2
   *  |            |            /
   *  +------------+     (3)child
   *
   *
   * In pre-order, that's: 0 1 2 3
   *     but oooooh, there's a cycle.  That's not a tree, it's a graph!
   */
  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list_1);

  tlib_pass_if_uint64_t_equal("The subsequent list has four elements", 4, list_1.used);


  for (i = 0; i < list_1.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list_1.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A one-time traversed tree must supply grey nodes",
        list_1.elements[i]->color, NR_SEGMENT_GREY);
  }

  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list_2);

  tlib_pass_if_uint64_t_equal("The subsequent list has four elements", 4, list_2.used);

  for (i = 0; i < list_2.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list_2.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A two-time traversed tree must supply white nodes",
        list_2.elements[i]->color, NR_SEGMENT_WHITE);
  }

  /* Clean up */
  nr_segment_children_destroy_fields(&grandmother.children);
  nr_segment_children_destroy_fields(&grown_child_1.children);
  nr_segment_children_destroy_fields(&grown_child_2.children);
}

static void test_segment_iterate_with_amputation(void) {
  int i;
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  /* Declare seven segments; give them .name values in pre-order */
  nr_segment_t grandmother = {.type = NR_SEGMENT_CUSTOM, .name = 0};

  nr_segment_t grown_child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 1};
  nr_segment_t grown_child_2 = {.type = NR_SEGMENT_CUSTOM, .name = 3};

  nr_segment_t child_1 = {.type = NR_SEGMENT_CUSTOM, .name = 2};

  /* Build a mock tree of segments */
  nr_segment_children_init(&grandmother.children);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_1);
  nr_segment_add_child(&grandmother, &grown_child_2);

  nr_segment_children_init(&grown_child_1.children);
  nr_segment_add_child(&grown_child_1, &child_1);

  /*
   * The mock tree looks like this:
   *
   *               --------(0)grandmother---------
   *                /             |              \
   *    (1)grown_child_1   (1)grown_child_1    (3)grown_child_2
   *       |                  |
   * (2)child_1          (2)child_1
   *
   *
   * In pre-order, that's: 0 1 2 1 2 3
   *   Except!  Segment 1 "grown_child_1" appears twice in the tree.  The
   * implementation of nr_segment_iterate() is such that every unique segment is
   * traversed only once. This means that the second child of the grandmother,
   * and all of its children, will be amputated from the subsequent trace.
   *
   * So the expected traversal is: 0 1 2 3
   */
  nr_segment_iterate(&grandmother, (nr_segment_iter_t)test_iterator_callback,
                     &list);

  tlib_pass_if_uint64_t_equal("The subsequent list has four elements", 4, list.used);

  for (i = 0; i < list.used; i++) {
    tlib_pass_if_int_equal("A tree must be traversed pre-order",
                           list.elements[i]->name, i);
    tlib_pass_if_uint64_t_equal(
        "A one-time traversed tree must supply grey nodes",
        list.elements[i]->color, NR_SEGMENT_GREY);
  }

  /* Clean up */
  nr_segment_children_destroy_fields(&grandmother.children);
  nr_segment_children_destroy_fields(&grown_child_1.children);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_segment_start();
  test_segment_start_async();
  test_set_name();
  test_add_child();
  test_set_parent_to_same();
  test_set_null_parent();
  test_set_non_null_parent();
  test_set_timing();
  test_end_segment();
  test_segment_iterate_bachelor();
  test_segment_iterate_nulls();
  test_segment_iterate();
  test_segment_iterate_cycle_one();
  test_segment_iterate_cycle_two();
  test_segment_iterate_with_amputation();
}
