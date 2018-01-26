#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "segment.h"
#include "test.h"

static void test_segment_validate_success(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("Should pass", "param"));
}

static void test_segment_validate_failure(void** state NRUNUSED) {
  assert_false(newrelic_validate_segment_param("Should/fail", "param"));
}

static void test_segment_validate_null_in(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param(NULL, "param"));
}

static void test_segment_validate_empty_in(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("", NULL));
}

static void test_segment_validate_null_name(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("Should pass", NULL));
}

static void test_segment_validate_empty_name(void** state NRUNUSED) {
  assert_true(newrelic_validate_segment_param("Should pass", ""));
}

int main(void) {
  const struct CMUnitTest segment_tests[] = {
      cmocka_unit_test(test_segment_validate_success),
      cmocka_unit_test(test_segment_validate_failure),
      cmocka_unit_test(test_segment_validate_null_in),
      cmocka_unit_test(test_segment_validate_empty_in),
      cmocka_unit_test(test_segment_validate_null_name),
      cmocka_unit_test(test_segment_validate_empty_name),
  };

  return cmocka_run_group_tests(segment_tests, NULL, NULL);
}