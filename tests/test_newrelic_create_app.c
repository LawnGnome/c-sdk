#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "libnewrelic_internal.h"
#include "test.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

nr_status_t __wrap_nrl_send_log_message(nrloglev_t level, const char* fmt, ...);

nrapplist_t* __wrap_newrelic_init(const char* daemon_socket);

nr_status_t __wrap_newrelic_connect_app(newrelic_app_t* app,
                                        nrapplist_t* context,
                                        unsigned short timeout_ms);
/**
 * Purpose: Mock to catch calls to the logger so we don't spew information
 * during the test runs
 */
nr_status_t __wrap_nrl_send_log_message(nrloglev_t level NRUNUSED,
                                        const char* fmt,
                                        ...) {
  printf("LOG: %s", fmt);
  return NR_SUCCESS;
}

nrapplist_t* __wrap_newrelic_init(const char* daemon_socket NRUNUSED) {
  // printf("\n\n__wrap_newrelic_init\n\n");
  return (nrapplist_t*)mock();
}

nr_status_t __wrap_newrelic_connect_app(newrelic_app_t* app NRUNUSED,
                                        nrapplist_t* context NRUNUSED,
                                        unsigned short timeout_ms NRUNUSED) {
  // printf("\n\n__wrap_newrelic_connect_app\n\n");
  return (nr_status_t)mock();
}

static int setup(void** state) {
  newrelic_config_t* config;
  config = (newrelic_config_t*)nr_zalloc(sizeof(newrelic_config_t));
  *state = config;
  return 0;  // tells cmocka setup completed, 0==OK
}

static int teardown(void** state) {
  newrelic_config_t* config;
  config = (newrelic_config_t*)*state;
  free(config);
  return 0;  // tells cmocka teardown completed, 0==OK
}

static void test_null_config(void** state NRUNUSED) {
  newrelic_config_t* config;
  newrelic_app_t* app;
  config = NULL;
  app = NULL;
  app = newrelic_create_app(config, 1000);
  assert_null(app);
}

static void test_empty_appname(void** state NRUNUSED) {
  newrelic_config_t* config;
  newrelic_app_t* app;
  config = (newrelic_config_t*)*state;
  app = NULL;

  nr_strxcpy(config->app_name, "", nr_strlen(""));

  app = newrelic_create_app(config, 1000);
  assert_null(app);
}

static void test_licence_key_lengths(void** state NRUNUSED) {
  newrelic_config_t* config;
  newrelic_app_t* app;
  config = (newrelic_config_t*)*state;

  app = NULL;
  nr_strxcpy(config->app_name, "valid app name", nr_strlen("valid app name"));

  // too short
  nr_strxcpy(config->license_key, "abc", nr_strlen("abc"));
  app = newrelic_create_app(config, 1000);
  assert_null(app);

  // too long
  nr_strxcpy(config->license_key, "toolong-toolong-toolong-toolong-toolong-42",
             nr_strlen("toolong-toolong-toolong-toolong-toolong-42"));
  app = newrelic_create_app(config, 1000);
  assert_null(app);
}

static void test_newrelic_connect_app_returns_failure(void** state NRUNUSED) {
  newrelic_config_t* config;
  newrelic_app_t* appForMock;
  newrelic_app_t* app;
  config = (newrelic_config_t*)*state;

  appForMock = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));

  will_return(__wrap_newrelic_init, appForMock);
  will_return(__wrap_newrelic_connect_app, NR_FAILURE);

  nr_strxcpy(config->app_name, "valid app name", nr_strlen("valid app name"));

  nr_strxcpy(config->license_key, "0123456789012345678901234567890123456789",
             40);

  app = newrelic_create_app(config, 1000);
  assert_null(app);

  nr_free(app);
}

static void test_newrelic_init_returns_null(void** state NRUNUSED) {
  newrelic_config_t* config;
  newrelic_app_t* app;
  config = (newrelic_config_t*)*state;

  will_return(__wrap_newrelic_init, NULL);
  app = NULL;
  nr_strxcpy(config->app_name, "valid app name", nr_strlen("valid app name"));

  nr_strxcpy(config->license_key, "0123456789012345678901234567890123456789",
             40);

  app = newrelic_create_app(config, 1000);
  assert_null(app);
}

static void test_newrelic_app_correctly_populated(void** state NRUNUSED) {
  newrelic_config_t* config;
  newrelic_app_t* appForMock;
  newrelic_app_t* app;
  config = (newrelic_config_t*)*state;

  appForMock = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));

  will_return(__wrap_newrelic_init, appForMock);
  will_return(__wrap_newrelic_connect_app, NR_SUCCESS);

  nr_strxcpy(config->app_name, "valid app name", nr_strlen("valid app name"));

  nr_strxcpy(config->license_key, "0123456789012345678901234567890123456789",
             40);

  app = newrelic_create_app(config, 1000);
  assert_non_null(app);

  nr_free(app);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_null_config),
      cmocka_unit_test(test_empty_appname),
      cmocka_unit_test(test_licence_key_lengths),
      cmocka_unit_test(test_newrelic_connect_app_returns_failure),
      cmocka_unit_test(test_newrelic_init_returns_null),
      cmocka_unit_test(test_newrelic_app_correctly_populated),
  };

  return cmocka_run_group_tests(tests, setup, teardown);
}
