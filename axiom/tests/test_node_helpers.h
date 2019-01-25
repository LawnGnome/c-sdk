/*
 * This file contains support for test_node tests.
 */
#ifndef TEST_NODE_HELPERS_HDR
#define TEST_NODE_HELPERS_HDR

#include <stdint.h>

#include "nr_txn.h"
#include "util_metrics.h"
#include "util_time.h"

#define test_metric_table_size(...) \
  test_metric_table_size_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_txn_untouched(X1, X2) \
  test_txn_untouched_fn((X1), (X2), __FILE__, __LINE__)
#define test_one_node_populated(...) \
  test_one_node_populated_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_metric_created(...) \
  test_node_helper_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)

extern nrtxn_t* new_txn(int background);
extern void test_metric_table_size_fn(const char* testname,
                                      const nrmtable_t* metrics,
                                      int expected_size,
                                      const char* file,
                                      int line);
extern void test_txn_untouched_fn(const char* testname,
                                  const nrtxn_t* txn,
                                  const char* file,
                                  int line);

extern void test_one_node_populated_fn(const char* testname,
                                       const nrtxn_t* txn,
                                       const char* expected_name,
                                       nrtime_t duration,
                                       const char* expected_data_hash,
                                       const char* file,
                                       int line);

extern void test_node_helper_metric_created_fn(const char* testname,
                                               nrmtable_t* metrics,
                                               uint32_t flags,
                                               nrtime_t duration,
                                               const char* name,
                                               const char* file,
                                               int line);

#endif /* TEST_NODE_HELPERS_HDR */
