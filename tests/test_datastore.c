
#include <stdarg.h>
#include <stddef.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "datastore.h"
#include "util_memory.h"

#include "test.h"

/* Create a datastore segment interesting enough for testing purposes */
static newrelic_datastore_segment_t* mock_datastore_segment(
    newrelic_txn_t* txn) {
  newrelic_datastore_segment_t* segment_ptr
      = nr_zalloc(sizeof(newrelic_datastore_segment_t));
  segment_ptr->txn = txn;
  segment_ptr->params.start.stamp = 1;
  segment_ptr->params.start.when = 1;

  return segment_ptr;
}

/* Declare prototypes for mocks. */
void __wrap_nr_txn_end_node_datastore(nrtxn_t* txn,
                                      const nr_node_datastore_params_t* params);
/*
 * Purpose: Mock to validate that appropriate values are passed into
 * nr_txn_end_node_external().
 */
void __wrap_nr_txn_end_node_datastore(
    nrtxn_t* txn,
    const nr_node_datastore_params_t* params) {
  check_expected(txn);
  check_expected(params);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles invalid inputs
 * correctly.
 */
static void test_start_datastore_segment_invalid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_datastore_segment_params_t params = {
      .product = NULL,
  };

  assert_null(newrelic_start_datastore_segment(NULL, NULL));
  assert_null(newrelic_start_datastore_segment(txn, NULL));
  assert_null(newrelic_start_datastore_segment(NULL, &params));

  /* A NULL product should also result in a NULL. */
  assert_null(newrelic_start_datastore_segment(txn, &params));

  /* Now we'll test library and procedure values including slashes, which are
   * prohibited because they do terrible things to metric names and APM in
   * turn. */
  params.product = NEWRELIC_DATASTORE_OTHER;

  params.collection = "foo/bar";
  assert_null(newrelic_start_datastore_segment(txn, &params));
  params.collection = NULL;

  params.operation = "foo/bar";
  assert_null(newrelic_start_datastore_segment(txn, &params));
  params.operation = NULL;

  params.host = "foo/bar";
  assert_null(newrelic_start_datastore_segment(txn, &params));
  params.host = NULL;
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles an
 * empty-stringed .product input correctly and generates a fully
 * default-valued segment.
 */
static void test_start_datastore_segment_empty_product(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_datastore_segment_params_t params = {.product = ""};
  newrelic_datastore_segment_t* segment;

  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);
  assert_ptr_equal(txn, segment->txn);
  assert_int_not_equal(0, segment->params.start.when);

  /* Affirm that the product string was properly duplicated;
   * An empty string is transformed to NEWRELIC_DATASTORE_OTHER */
  assert_string_equal(NEWRELIC_DATASTORE_OTHER,
                      segment->params.datastore.string);
  assert_ptr_not_equal(NEWRELIC_DATASTORE_OTHER,
                       segment->params.datastore.string);

  /* Affirm that the product type was correctly populated */
  assert_int_equal(NR_DATASTORE_OTHER, segment->params.datastore.type);

  /* Affirm correct defaults everywhere else */
  assert_string_equal("other", segment->params.collection);
  assert_ptr_not_equal("other", segment->params.collection);

  assert_string_equal("other", segment->params.operation);
  assert_ptr_not_equal("other", segment->params.operation);

  assert_null(segment->params.sql.sql);
  assert_null(segment->params.sql.plan_json);
  assert_null(segment->params.sql.input_query);

  assert_non_null(segment->params.callbacks.backtrace);
  assert_null(segment->params.callbacks.modify_table_name);

  /* Affirm that an instance was created. Assume good things about
   * the underlying unit-tests for nr_datastore_instance_create()
   * getting called with all NULL values. */
  assert_non_null(segment->params.instance);

  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles
 * valid inputs correctly.
 */
static void test_start_datastore_segment_valid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  char* collection = "Collection";
  char* operation = "Operation";
  char* host = "Host";
  char* port_path_or_id = "Port";
  char* database_name = "Name";
  char* query = "Query";

  newrelic_datastore_segment_params_t params
      = {.product = NEWRELIC_DATASTORE_MYSQL,
         .collection = collection,
         .operation = operation,
         .host = host,
         .port_path_or_id = port_path_or_id,
         .database_name = database_name,
         .query = query};

  newrelic_datastore_segment_t* segment;

  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);
  assert_ptr_equal(txn, segment->txn);
  assert_int_not_equal(0, segment->params.start.when);

  /* Affirm that the product string was properly duplicated;
   * An empty string is transformed to NEWRELIC_DATASTORE_OTHER */
  assert_string_equal(NEWRELIC_DATASTORE_MYSQL,
                      segment->params.datastore.string);
  assert_ptr_not_equal(NEWRELIC_DATASTORE_MYSQL,
                       segment->params.datastore.string);

  /* Affirm that the product type was correctly populated */
  assert_int_equal(NR_DATASTORE_MYSQL, segment->params.datastore.type);

  /* Affirm correct defaults everywhere else */
  assert_string_equal(collection, segment->params.collection);
  assert_ptr_not_equal(collection, segment->params.collection);

  assert_string_equal(operation, segment->params.operation);
  assert_ptr_not_equal(operation, segment->params.operation);

  assert_string_equal(query, segment->params.sql.sql);
  assert_ptr_not_equal(query, segment->params.sql.sql);

  assert_null(segment->params.sql.plan_json);
  assert_null(segment->params.sql.input_query);

  assert_non_null(segment->params.callbacks.backtrace);
  assert_null(segment->params.callbacks.modify_table_name);

  /* Affirm that an instance was created and correctly populated. */
  assert_non_null(segment->params.instance);

  assert_string_equal(host, segment->params.instance->host);
  assert_ptr_not_equal(host, segment->params.instance->host);

  assert_string_equal(port_path_or_id,
                      segment->params.instance->port_path_or_id);
  assert_ptr_not_equal(port_path_or_id,
                       segment->params.instance->port_path_or_id);

  assert_string_equal(database_name, segment->params.instance->database_name);
  assert_ptr_not_equal(database_name, segment->params.instance->database_name);

  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_end_datastore_segment() handles NULL inputs
 * correctly.
 */
static void test_end_datastore_segment_null_inputs(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  assert_false(newrelic_end_datastore_segment(NULL, NULL));
  assert_false(newrelic_end_datastore_segment(txn, NULL));
}

/*
 * Purpose: Test that newrelic_end_datastore_segment() handles a
 * valid segment but invalid transaction.
 */
static void test_end_datastore_segment_invalid_txn(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_datastore_segment_t* segment_ptr = mock_datastore_segment(txn);

  /* This should destroy the given segment, even though the transaction is
   * invalid. */
  assert_false(newrelic_end_datastore_segment(NULL, &segment_ptr));
  assert_null(segment_ptr);
}

/*
 * Purpose: Test that newrelic_end_datastore_segment() handles a
 * valid segment but a different transaction than which the segment
 * was started.
 */
static void test_end_datastore_segment_different_txn(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_datastore_segment_t* segment_ptr = mock_datastore_segment(txn);

  /* A different transaction should result in failure and destroy the segment.
   */
  assert_false(newrelic_end_datastore_segment(txn + 1, &segment_ptr));
  assert_null(segment_ptr);
}

/*
 * Purpose: Test that newrelic_end_datastore_segment() handles
 * valid inputs.
 */
static void test_end_datastore_segment_valid(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;
  newrelic_datastore_segment_t* segment_ptr = mock_datastore_segment(txn);

  expect_value(__wrap_nr_txn_end_node_datastore, txn, txn);
  expect_value(__wrap_nr_txn_end_node_datastore, params, &segment_ptr->params);

  assert_true(newrelic_end_datastore_segment(txn, &segment_ptr));
  assert_null(segment_ptr);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles inputs from
 * cross agent tests
 */
static void test_start_datastore_segment_all_params_without_query(
    void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_datastore_segment_params_t params = {
      .product = "MySQL",
      .collection = "users",
      .operation = "INSERT",
      .host = "db-server-1",
      .port_path_or_id = "3306",
      .database_name = "my_db",
  };
  newrelic_datastore_segment_t* segment;
  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);

  assert_string_equal(params.product, segment->params.datastore.string);
  assert_string_equal(params.collection, segment->params.collection);
  assert_string_equal(params.operation, segment->params.operation);
  assert_string_equal(params.host, segment->params.instance->host);
  assert_string_equal(params.port_path_or_id,
                      segment->params.instance->port_path_or_id);
  assert_string_equal(params.database_name,
                      segment->params.instance->database_name);

  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles a missing
 * database name
 */
static void test_start_datastore_segment_database_name_missing(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_datastore_segment_params_t params = {
      .product = "MySQL",
      .collection = "users",
      .operation = "INSERT",
      .host = "db-server-1",
      .port_path_or_id = "3306",
  };
  newrelic_datastore_segment_t* segment;
  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);

  assert_string_equal(params.product, segment->params.datastore.string);
  assert_string_equal(params.collection, segment->params.collection);
  assert_string_equal(params.operation, segment->params.operation);
  assert_string_equal(params.host, segment->params.instance->host);
  assert_string_equal(params.port_path_or_id,
                      segment->params.instance->port_path_or_id);
  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles a missing port
 */
static void test_start_datastore_segment_host_and_port_missing(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_datastore_segment_params_t params = {
      .product = "MySQL",
      .collection = "users",
      .operation = "INSERT",
      .database_name = "my_db",
  };
  newrelic_datastore_segment_t* segment;
  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);

  assert_string_equal(params.product, segment->params.datastore.string);
  assert_string_equal(params.collection, segment->params.collection);
  assert_string_equal(params.operation, segment->params.operation);
  assert_string_equal(params.database_name,
                      segment->params.instance->database_name);
  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles a missing host
 */
static void test_start_datastore_segment_host_missing(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_datastore_segment_params_t params = {
      .product = "MySQL",
      .collection = "users",
      .operation = "INSERT",
      .port_path_or_id = "3306",
      .database_name = "my_db",
  };
  newrelic_datastore_segment_t* segment;
  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);

  assert_string_equal(params.product, segment->params.datastore.string);
  assert_string_equal(params.collection, segment->params.collection);
  assert_string_equal(params.operation, segment->params.operation);
  assert_string_equal(params.port_path_or_id,
                      segment->params.instance->port_path_or_id);
  assert_string_equal(params.database_name,
                      segment->params.instance->database_name);
  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles a missing collection
 */
static void test_start_datastore_segment_missing_collection(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_datastore_segment_params_t params = {
      .product = "MySQL",
      .operation = "INSERT",
      .host = "db-server-1",
      .port_path_or_id = "3306",
      .database_name = "my_db",
  };
  newrelic_datastore_segment_t* segment;
  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);

  assert_string_equal(params.product, segment->params.datastore.string);
  assert_string_equal(params.operation, segment->params.operation);
  assert_string_equal(params.host, segment->params.instance->host);
  assert_string_equal(params.port_path_or_id,
                      segment->params.instance->port_path_or_id);
  assert_string_equal(params.database_name,
                      segment->params.instance->database_name);
  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() swaps out localhost
 */
static void test_start_datastore_segment_localhost_replacement(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_datastore_segment_params_t params = {
      .product = "MySQL",
      .collection = "users",
      .operation = "INSERT",
      .host = "localhost",
      .port_path_or_id = "3306",
      .database_name = "my_db",
  };
  newrelic_datastore_segment_t* segment;
  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);

  assert_string_equal(params.product, segment->params.datastore.string);
  assert_string_equal(params.collection, segment->params.collection);
  assert_string_equal(params.operation, segment->params.operation);
  assert_string_not_equal(params.host, segment->params.instance->host);
  assert_string_equal(params.port_path_or_id,
                      segment->params.instance->port_path_or_id);
  assert_string_equal(params.database_name,
                      segment->params.instance->database_name);
  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Test that newrelic_start_datastore_segment() handles a non-numerical
 * port (i.e for unix sockets)
 */
static void test_start_datastore_segment_socket_path_port(void** state) {
  newrelic_txn_t* txn = (newrelic_txn_t*)*state;

  newrelic_datastore_segment_params_t params = {
      .product = "MySQL",
      .collection = "users",
      .operation = "INSERT",
      .host = "db-server-1",
      .port_path_or_id = "/var/mysql/mysql.sock",
      .database_name = "my_db",
  };
  newrelic_datastore_segment_t* segment;
  segment = newrelic_start_datastore_segment(txn, &params);
  assert_non_null(segment);

  assert_string_equal(params.product, segment->params.datastore.string);
  assert_string_equal(params.collection, segment->params.collection);
  assert_string_equal(params.operation, segment->params.operation);
  assert_string_equal(params.host, segment->params.instance->host);
  assert_string_equal(params.port_path_or_id,
                      segment->params.instance->port_path_or_id);
  assert_string_equal(params.database_name,
                      segment->params.instance->database_name);
  newrelic_destroy_datastore_segment(&segment);
}

/*
 * Purpose: Main entry point (i.e. runs the tests)
 */
int main(void) {
  const struct CMUnitTest datastore_tests[] = {
      cmocka_unit_test(test_start_datastore_segment_invalid),
      cmocka_unit_test(test_start_datastore_segment_empty_product),
      cmocka_unit_test(test_start_datastore_segment_valid),
      cmocka_unit_test(test_end_datastore_segment_null_inputs),
      cmocka_unit_test(test_end_datastore_segment_invalid_txn),
      cmocka_unit_test(test_end_datastore_segment_different_txn),
      cmocka_unit_test(test_end_datastore_segment_valid),
      cmocka_unit_test(
          test_start_datastore_segment_all_params_without_query),
      cmocka_unit_test(test_start_datastore_segment_database_name_missing),
      cmocka_unit_test(test_start_datastore_segment_host_and_port_missing),
      cmocka_unit_test(test_start_datastore_segment_host_missing),
      cmocka_unit_test(test_start_datastore_segment_missing_collection),
      cmocka_unit_test(test_start_datastore_segment_localhost_replacement),
      cmocka_unit_test(test_start_datastore_segment_socket_path_port),

  };

  return cmocka_run_group_tests(datastore_tests, txn_group_setup,
                                txn_group_teardown);
}
