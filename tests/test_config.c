#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "config.h"
#include "test.h"

#define LICENSE_KEY ("Thisisafortycharacterkeyabcdefghijklmnop")
#define TOO_SHORT_LICENSE_KEY ("abc123def456")

static void test_config_setup(void** state NRUNUSED) {
  assert_false(0);
}

static void test_config_null_app_name(void** state NRUNUSED) {
  newrelic_config_t* config;
  config = newrelic_new_config(NULL, LICENSE_KEY);
  assert_null(config);
}

static void test_config_null_license_key(void** state NRUNUSED) {
  newrelic_config_t* config;
  config = newrelic_new_config("Test App", NULL);
  assert_null(config);
}

static void test_config_short_license_key(void** state NRUNUSED) {
  newrelic_config_t* config;
  config = newrelic_new_config("Test App", TOO_SHORT_LICENSE_KEY);
  assert_null(config);
}

static void test_config_long_license_key(void** state NRUNUSED) {
  newrelic_config_t* config;
  config = newrelic_new_config(
      "Test App",
      "This is the license key that never ends, yes it goes on and on my "
      "friends.  Some people, starting licensing it not knowing what it was, "
      "but they'll continue licensing forever just because.");
  assert_null(config);
}

static void test_config_justright_license_key(void** state NRUNUSED) {
  newrelic_config_t* config;
  config = newrelic_new_config("Test App", LICENSE_KEY);
  assert_non_null(config);

  /* Test non-zero defaults. */
  assert_true(config->transaction_tracer.enabled);
  assert_true(NEWRELIC_THRESHOLD_IS_APDEX_FAILING == config->transaction_tracer.threshold);

  free(config);
}

int main(void) {
  const struct CMUnitTest license_tests[] = {
      cmocka_unit_test(test_config_setup),
      cmocka_unit_test(test_config_null_app_name),
      cmocka_unit_test(test_config_null_license_key),
      cmocka_unit_test(test_config_short_license_key),
      cmocka_unit_test(test_config_long_license_key),
      cmocka_unit_test(test_config_justright_license_key),
  };

  return cmocka_run_group_tests(license_tests, NULL, NULL);
}
