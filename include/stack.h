#ifndef LIBNEWRELIC_STACK_H
#define LIBNEWRELIC_STACK_H

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

#endif /* LIBNEWRELIC_STACK_H */
