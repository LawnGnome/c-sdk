#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <setjmp.h>
#include <cmocka.h>

#include "libnewrelic.h"
#include "app.h"
#include "test.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_time.h"

nrapp_t* __wrap_nr_agent_find_or_add_app(
    nrapplist_t* applist,
    const nr_app_info_t* info,
    nrobj_t* (*settings_callback_fn)(void));

nrapp_t* __wrap_nr_agent_find_or_add_app(nrapplist_t* applist NRUNUSED,
                                         const nr_app_info_t* info NRUNUSED,
                                         nrobj_t* (*settings_callback_fn)(void)
                                             NRUNUSED) {
  return (nrapp_t*)mock();
}

static void test_connect_app_null_inputs(void** state NRUNUSED) {
  nr_status_t result;
  result = newrelic_connect_app(NULL, NULL, 1);
  assert_true(NR_FAILURE == result);
}

static void test_connect_app_is_null(void** state NRUNUSED) {
  nr_status_t result;

  nrapplist_t* context;
  context = (nrapplist_t*)nr_zalloc(sizeof(nrapplist_t));
  result = newrelic_connect_app(NULL, context, 0);
  assert_true(NR_FAILURE == result);

  nr_free(context);
}

static void test_connect_context_is_null(void** state NRUNUSED) {
  nr_status_t result;

  newrelic_app_t* app;
  app = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));  

  result = newrelic_connect_app(app, NULL, 0);
  assert_true(NR_FAILURE == result);

  newrelic_destroy_app(&app);
}

static void test_connect_app_nrapp_is_null(void** state NRUNUSED) {
  nr_status_t result;

  newrelic_app_t* app;
  nrapplist_t* context;
  nrapp_t* nrapp = 0;
  nr_app_info_t* app_info;
  
  app = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));
  context = (nrapplist_t*)nr_zalloc(sizeof(nrapplist_t));
  app_info = (nr_app_info_t*)nr_zalloc(sizeof(nr_app_info_t));
  app->app_info = app_info;

  will_return(__wrap_nr_agent_find_or_add_app, nrapp);

  result = newrelic_connect_app(app, context, 0);
  assert_true(NR_FAILURE == result);

  newrelic_destroy_app(&app);
  nr_free(context);
}

static void test_connect_app_null_app_info(void** state NRUNUSED) {
  nr_status_t result;

  newrelic_app_t* app;
  nrapplist_t* context;
  
  app = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));
  context = (nrapplist_t*)nr_zalloc(sizeof(nrapplist_t));
  app->app_info = NULL;
  

  result = newrelic_connect_app(app, context, 0);
  assert_true(NR_FAILURE == result);

  newrelic_destroy_app(&app);
  nr_free(context);
}

static void test_connect_app_successful_connect(void** state NRUNUSED) {
  nr_status_t result;

  newrelic_app_t* app;
  nrapplist_t* context;
  nrapp_t* nrapp;
  nr_app_info_t* app_info;

  nrapp = (nrapp_t*)nr_zalloc(sizeof(nrapp_t));
  app = (newrelic_app_t*)nr_zalloc(sizeof(newrelic_app_t));
  context = (nrapplist_t*)nr_zalloc(sizeof(nrapplist_t));
  app_info = (nr_app_info_t*)nr_zalloc(sizeof(nr_app_info_t));
  app->app_info = app_info;

  will_return(__wrap_nr_agent_find_or_add_app, nrapp);

  result = newrelic_connect_app(app, context, 0);
  assert_true(NR_SUCCESS == result);

  // newrelic_destroy_app also cleans up app_info
  newrelic_destroy_app(&app);
  nr_free(context);
  nr_free(nrapp);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_connect_app_null_inputs),
      cmocka_unit_test(test_connect_app_nrapp_is_null),
      cmocka_unit_test(test_connect_app_null_app_info),
      cmocka_unit_test(test_connect_app_successful_connect),
      cmocka_unit_test(test_connect_app_is_null),
      cmocka_unit_test(test_connect_context_is_null),      
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
