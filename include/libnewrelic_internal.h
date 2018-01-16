#ifndef LIBNEWRELIC_INTERNAL_H
#define LIBNEWRELIC_INTERNAL_H

#include "nr_txn.h"

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
