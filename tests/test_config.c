#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"

#define CONFIG_NAME ("Test Config Name")
#define LICENSE_KEY ("Thisisafortycharacterkeyabcdefghijklmnop")
#define TOO_SHORT_LICENSE_KEY ("abc123def456")


static void test_setup(void** state) {
  assert_false(0);
}

static void test_config_null_app_name(void** state) {
  newrelic_config_t* config;
  config = newrelic_new_config(NULL, "abc");
  assert_null(config);
}

static void test_config_null_license_key(void** state) {
  newrelic_config_t* config;
  config = newrelic_new_config("Test App", NULL);
  assert_null(config);
}

static void test_config_short_license_key(void** state) {
  newrelic_config_t* config;
  config = newrelic_new_config("Test App", "SMRT");
  assert_null(config);
}

static void test_config_long_license_key(void** state) {
  newrelic_config_t* config;
  config = newrelic_new_config(
      "Test App",
      "This is the license key that never ends, yes it goes on and on my "
      "friends.  Some people, starting licensing it not knowing what it was, "
      "but they'll continue licensing forever just because.");
  assert_null(config);
}

static void test_config_justright_license_key(void** state) {
  newrelic_config_t* config;
  config = newrelic_new_config("Test App",
                               "Thisisafortycharacterkeyabcdefghijklmnop");
  assert_non_null(config);
  free(config);
}

int main(void) {
  const struct CMUnitTest license_tests[] = {
      cmocka_unit_test(test_setup),
      cmocka_unit_test(test_config_null_app_name),
      cmocka_unit_test(test_config_null_license_key),
      cmocka_unit_test(test_config_short_license_key),
      cmocka_unit_test(test_config_long_license_key),
      cmocka_unit_test(test_config_justright_license_key),
  };

  return cmocka_run_group_tests(license_tests, NULL, NULL);
}