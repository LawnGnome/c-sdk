#ifndef LIBNEWRELIC_INTERNAL_H
#define LIBNEWRELIC_INTERNAL_H

#include "nr_txn.h"
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
