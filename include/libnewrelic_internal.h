#ifndef LIBNEWRELIC_INTERNAL_H
#define LIBNEWRELIC_INTERNAL_H

#include "nr_txn.h"

typedef struct _nr_app_and_info_t {
  nrapp_t* app;
  nr_app_info_t* app_info;
  newrelic_config_t* config;
  nrapplist_t* context;
} nr_app_and_info_t;

nrapplist_t* newrelic_init(const char* daemon_socket);
nr_status_t newrelic_connect_app(newrelic_app_t* app,
                                 nrapplist_t* context,
                                 unsigned short timeout_ms);
newrelic_txn_t* newrelic_start_transaction(newrelic_app_t* app,
                                           const char* name,
                                           bool is_web_transaction);

/*
 * Purpose: Returns the current stack trace as a JSON string
 *
 * Usage
 *
 * char* stacktrace_json;
 * stacktrace_json = newrelic_get_stack_trace_as_json();
 * nr_free(stacktrace_json);  //caller needs to free the string
 *
 */
char* newrelic_get_stack_trace_as_json(void);

#endif /* LIBNEWRELIC_INTERNAL_H */
