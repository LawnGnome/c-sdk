#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "libnewrelic.h"
#include "libnewrelic_internal.h"
#include "app.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "nr_agent.h"
#include "nr_app.h"
#include "nr_attributes.h"
#include "nr_axiom.h"
#include "nr_commands.h"
#include "nr_txn.h"
#include "util_object.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_sleep.h"
#include "version.h"

#define NUM_BACKTRACE_FRAMES 100

newrelic_txn_t* newrelic_start_transaction(newrelic_app_t* app,
                                           const char* name,
                                           bool is_web_transaction) {
  newrelic_txn_t* transaction = NULL;
  nrtxnopt_t* options = NULL;
  nr_attribute_config_t* attribute_config = NULL;

  if (NULL == app) {
    nrl_error(NRL_INSTRUMENT,
              "unable to start transaction with a NULL application");
    return NULL;
  }

  options = newrelic_get_default_options();
  transaction = nr_txn_begin(app->app, options, attribute_config);

  if (NULL == name) {
    name = "NULL";
  }

  nr_txn_set_path(NULL, transaction, name, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

  nr_attribute_config_destroy(&attribute_config);
  nr_free(options);

  if (is_web_transaction) {
    nr_txn_set_as_web_transaction(transaction, 0);
    nrl_verbose(NRL_INSTRUMENT, "starting web transaction \"%s\"", name);
  } else {
    nr_txn_set_as_background_job(transaction, 0);
    nrl_verbose(NRL_INSTRUMENT, "starting non-web transaction \"%s\"", name);
  }

  return transaction;
}

char* newrelic_get_stack_trace_as_json(void) {
#ifdef HAVE_BACKTRACE
  size_t size_of_backtrace_array;
  void* backtrace_pointers[NUM_BACKTRACE_FRAMES];
  char* stacktrace_json;
  nrobj_t* arr_backtrace;
  char** backtrace_lines;

  // grab the backtrace lines
  size_of_backtrace_array = backtrace(backtrace_pointers, NUM_BACKTRACE_FRAMES);
  backtrace_lines =
      backtrace_symbols(backtrace_pointers, size_of_backtrace_array);

  // for each line in the backtrace, add to our nro_array
  arr_backtrace = nro_new_array();
  for (size_t i = 0; i < size_of_backtrace_array; i++) {
    nro_set_array_string(arr_backtrace, 0, backtrace_lines[i]);
  }

  // serialize the nro_array as json
  stacktrace_json = nro_to_json(arr_backtrace);

  // free up what we don't need
  nr_free(backtrace_lines);
  nro_delete(arr_backtrace);
  return stacktrace_json;
#else
  return nr_strdup("[\"No backtrace on this platform.\"]");
#endif

}
