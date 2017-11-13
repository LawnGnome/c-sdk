#include <stdlib.h>
#include <stdio.h>

#include "tlib_main.h"
#include "nr_axiom.h"
#include "libnewrelic.h"
#include "util_strings.h"

#define CONFIG_NAME ("Test Config Name")
#define LICENSE_KEY ("Thisisafortycharacterkeyabcdefghijklmnop")
#define TOO_SHORT_LICENSE_KEY ("abc123def456")

static void test_new_config_null(void) {
  /* Test all the cases in which newrelic_new_config returns NULL. */
  const char* cases[] = {
      NULL,
      NULL,
      "Configuration must be NULL when both parameters are NULL",
      NULL,
      LICENSE_KEY,
      "Configuration must be NULL when app_name is NULL",
      CONFIG_NAME,
      NULL,
      "Configuration must be NULL when license key is NULL",
      CONFIG_NAME,
      TOO_SHORT_LICENSE_KEY,
      "Configuration must be NULL when license key is incorrect length"};

  int case_size = 3;
  int number_of_cases = sizeof(cases) / case_size / sizeof(char*);
  int i = 0;

  for (i = 0; i < case_size * number_of_cases; i += 3) {
    newrelic_config_t* test_config =
        newrelic_new_config(cases[i], cases[i + 1]);
    tlib_pass_if_null(cases[i + 2], test_config);

    if (test_config != NULL) {
      free(test_config);
    }
  }
}

static void test_new_config_success(void) {
  /* Test the case in which newrelic_new_config returns a well-formed
   * configuration */
  newrelic_config_t* test_config =
      newrelic_new_config(CONFIG_NAME, LICENSE_KEY);
  tlib_pass_if_not_null(
      "Configuration must be created when both parameters are well-formed",
      test_config);
  tlib_pass_if_str_equal("Configuration must have supplied application name",
                         test_config->app_name, CONFIG_NAME);
  tlib_pass_if_str_equal("Configuration must have supplied license key",
                         test_config->license_key, LICENSE_KEY);

  if (test_config != NULL) {
    free(test_config);
  }
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_new_config_null();
  test_new_config_success();
}