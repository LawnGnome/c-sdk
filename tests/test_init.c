#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "app.h"
#include "test.h"
#include "nr_app.h"
#include "nr_agent.h"

/* Declare prototypes for mocks/real */
nrapplist_t* __wrap_nr_applist_create(void);
nrapplist_t* __real_nr_applist_create(void);
nr_status_t __wrap_nr_agent_initialize_daemon_connection_parameters(
    const char* listen_path,
    int external_port);
int __wrap_nr_agent_try_daemon_connect(int time_limit_ms);

/* Mock functions */
nrapplist_t* __wrap_nr_applist_create(void) {
  bool use_real = mock_type(bool);
  if (use_real) {
    return __real_nr_applist_create();
  } else {
    return (nrapplist_t*)mock();
  }
}

nr_status_t __wrap_nr_agent_initialize_daemon_connection_parameters(
    const char* listen_path NRUNUSED,
    int external_port NRUNUSED) {
  return (nr_status_t)mock();
}

int __wrap_nr_agent_try_daemon_connect(int time_limit_ms NRUNUSED) {
  return (int)mock();
}
/* End mock functions */

static void test_init_null_socket(void** state NRUNUSED) {
  nrapplist_t* ret = NULL;
  will_return(__wrap_nr_applist_create, true);
  ret = newrelic_init(NULL);
  assert_null(ret);
}

static void test_init_null_context(void** state NRUNUSED) {
  nrapplist_t* ret = NULL;
  will_return(__wrap_nr_applist_create, false);
  will_return(__wrap_nr_applist_create, NULL);
  ret = newrelic_init("daemon socket");
  assert_null(ret);
}

static void test_init_daemon_init_failure(void** state NRUNUSED) {
  nrapplist_t* ret = NULL;
  will_return(__wrap_nr_applist_create, true);
  will_return(__wrap_nr_agent_initialize_daemon_connection_parameters,
              NR_FAILURE);
  ret = newrelic_init("daemon socket");
  assert_null(ret);
}

static void test_init_daemon_connect_failure(void** state NRUNUSED) {
  nrapplist_t* ret = NULL;
  will_return(__wrap_nr_applist_create, true);
  will_return(__wrap_nr_agent_initialize_daemon_connection_parameters,
              NR_SUCCESS);
  will_return(__wrap_nr_agent_try_daemon_connect, 0);
  ret = newrelic_init("daemon socket");
  assert_null(ret);
}

static void test_init_daemon_connect_success(void** state NRUNUSED) {
  nrapplist_t* ret = NULL;
  will_return(__wrap_nr_applist_create, true);
  will_return(__wrap_nr_agent_initialize_daemon_connection_parameters,
              NR_SUCCESS);
  will_return(__wrap_nr_agent_try_daemon_connect, 1);
  ret = newrelic_init("daemon socket");
  assert_non_null(ret);
}

int main(void) {
  const struct CMUnitTest init_tests[] = {
      cmocka_unit_test(test_init_null_socket),
      cmocka_unit_test(test_init_null_context),
      cmocka_unit_test(test_init_daemon_init_failure),
      cmocka_unit_test(test_init_daemon_connect_failure),
      cmocka_unit_test(test_init_daemon_connect_success),
  };

  return cmocka_run_group_tests(init_tests, NULL, NULL);
}
