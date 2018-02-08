# C Agent

Generic library to communicate with New Relic.

## Requirements
* 64-bit Linux with
  * glibc 2.5+ with NPTL support
  * kernel version 2.6.13 or higher (2.6.26+ highly recommended)
  * libpcre 8.13+
  * libpthread

## Getting started

Instrument your code. Consider the brief program below or look at the `examples` directory
for source and Makefiles highlighting particular features.

```
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>

#include "libnewrelic.h"

int main (void) {
  int priority = 50;
  newrelic_app_t *app = 0;
  newrelic_txn_t *txn = 0;
  newrelic_config_t *config = 0;

  config = newrelic_new_config("Your Application Name", "<LICENSE_KEY_HERE>");
  strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
  strcpy(config->log_filename, "./c_agent.log");
  config->log_level = LOG_INFO;

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app(config, 10000);
  free(config);

  /* Start a web transaction */
  txn = newrelic_start_web_transaction(app, "veryImportantWebTransaction");

  /* Add attributes */
  newrelic_add_attribute_int(txn, "my_custom_int", INT_MAX);
  newrelic_add_attribute_string(txn, "my_custom_string",
                                "String String String");

  sleep(1);

  /* Record an error */
  newrelic_notice_error(txn, priority, "Meaningful error message",
                        "Error.class");

  /* End web transaction */
  newrelic_end_transaction(&txn);

  /* Start and end a non-web transaction */
  txn =
      newrelic_start_non_web_transaction(app, "veryImportantOtherTransaction");
  sleep(1);

  newrelic_end_transaction(&txn);

  newrelic_destroy_app(&app);

  return 0;
}
```

Compile and link your application against the static library, `libnewrelic.a`. There
are two considerations to make during the linking step. First, because `libnewrelic.a`
is offered as a static library, because it is already linked with the `libpcre`
and `libpthread` libraries, you must also link against these two libraries to avoid
symbol collisions in the linking step.

Second, to take full advantage of error traces at New Relic's Error Analytics
dashboard, link your application using GNU's `-rdynamic` linker flag.
Doing so means that more meaningful information appears in the stack trace
for the error recorded on a transaction using `newrelic_notice_error()`.

With these two considerations in mind, one may compile and link a simple application
like so:

```
gcc -o test_app test_app.c -L. -lnewrelic -lpcre -pthread -rdynamic
```

Start the daemon:

```
./bin/daemon -f -logfile stdout -loglevel debug
```

Run your test application and check the `c-agent.log` file for output.

## Features
* Transactions
  * Transaction events
  * Web and non-web transactions
  * Custom attributes
  * Error instrumentation
* Logging levels

### Segment Instrumentation

The agent provides several API functions for creating optional *segment* instrumentation.  Segments allow you to measure the time taken by specific portions of a transaction.  The agent allows you to create two different segment types.

* External Segments
* Datastore Segments

### Datastore Segments

The agent provides two functions, `newrelic_start_datastore_segment()` and `newrelic_end_datastore_segment()` that allow you to create datastore segments.  APM uses segments recorded in this manner in the [Databases and Slow Queries](https://docs.newrelic.com/docs/apm/applications-menu/monitoring/databases-slow-queries-page) of APM.  Segments created with these functions also populate the `databaseDuration` attribute of a [New Relic Insights](https://docs.newrelic.com/docs/insights/use-insights-ui/getting-started/introduction-new-relic-insights) Transaction event.

To record a datastore segment on an active transaction, you'll need to

1. Create a `newrelic_datastore_segment_params_t` struct that describes the datastore segment

2. Start the timer with `newrelic_start_datastore_segment()`

3. Stop the timer with `newrelic_end_datastore_segment()`

Here are those three steps in code. The `txn` variable below is a transaction, created via `newrelic_start_web_transaction()` or `newrelic_start_non_web_transaction()`.  You may only record segments on active transactions.

    txn = newrelic_start_web_transaction(app, "aTransactionName");

    /* ... */

    newrelic_datastore_segment_params_t params = {
        .product = NEWRELIC_DATASTORE_MYSQL,
        .collection = "table_name",
        .operation = "select",
        .host = "db.example.com",
        .port_path_or_id = "3306",
        .database_name = "db_accounting_1",
        .query =
            "SELECT * FROM table_name WHERE foo_bar = 'Foo Bar' "};

    newrelic_datastore_segment_t* segment =
        newrelic_start_datastore_segment(txn, &params);

    // the code you want to time goes here

    newrelic_end_datastore_segment(txn, &segment);

The `newrelic_datastore_segment_params_t` struct contains a list of parameters that New Relic uses to identify your segment. New Relic also uses these values to drive its user interface in APM. Only the `.product` field is required. You can find documentation for each field in `libnewrelic.h`. You can also find a working code sample in `examples/ex_datastore.c`.

**IMPORTANT**:  In order to ensure accurate timing, datastore segments cannot be nested within other datastore segments, and cannot be nested with external segments. You **must** call `newrelic_end_datastore_segment()` before starting a new datastore or external segment. Starting a new segment before the previous segment has ended will produce undefined results, and should be avoided.

#### Slow Query Tracing for Datastore Segments

When you send New Relic datastore segments, those segments may be eligible for [Slow Query tracing](https://docs.newrelic.com/docs/apm/applications-menu/monitoring/viewing-slow-query-details).  Only SQL-like databases are eligible for slow query tracing.   If your datastore segment's `.product` is set to `Firebird`, `Informix`, `MSSQL`, `MySQL`, `Oracle`, `Postgres`, `SQLite`, or `Sybase`, the C-Agent will make your segment eligible for slow query tracing.

Both the time threshold to trigger a slow query trace and whether slow query tracing is enabled are controlled via the C-Agent's **application** configuration, specifically the `datastore_reporting.*` fields.

```
    config = newrelic_new_config("C Agent Test App", "<LICENSE_KEY_HERE>");
    /* ... */
    config->transaction_tracer.datastore_reporting.enabled = true;
    config->transaction_tracer.datastore_reporting.threshold_us = 500000;
    app = newrelic_create_app(config, 10000);
    free(config);

    /* ... */

    /* Start a web transaction */
    txn = newrelic_start_web_transaction(app, "yourTransactionName");

    /* datastore segments created during the `txn` transaction
       will be eligable for slow query traces if the segment time
       is greater than 500,000 microseconds (0.5 seconds) */

    /* ...  */
```

### Error instrumentation

The agent provides the function `newrelic_notice_error()` so that customers
may record transaction errors. Errors recorded in this manner are displayed in
[error traces](https://docs.newrelic.com/docs/apm/applications-menu/error-analytics/error-analytics-explore-events-behind-errors#traces-table)
at New Relic's Error Analytics dashboard; they are available to query through
[New Relic Insights](https://docs.newrelic.com/docs/insights/use-insights-ui/getting-started/introduction-new-relic-insights).

When recording an error using `newrelic_notice_error()`, callers must supply four
parameters to the function, as indicated in `libnewrelic.h`. Among these
parameters are `priority` and `errclass`.

The agent is capped at reporting 100 error traces per minute. Supposing that over
100 errors are noticed during a single minute, the total number of errors are
reported in New Relic metrics; only 100 would be available at the Error Analytic's
dashboard. That said, in the pool of errors collected by the agent, the `priority`
of an error indicates which errors should be saved in the event that the cap has
been exceeded. Higher values take priority over lower values.

Errors are grouped by class in New Relic's Error Analytics dashboard. With that in
mind, the `errclass` parameter gives the caller control over how to filter for
errors on the dashboard.

With a valid application, `app`, created using `newrelic_create_app()`, one can
start a transaction, record an error, and end a transaction like so:

```
 int priority = 50;
 newrelic_txn_t* txn = newrelic_start_non_web_transaction(app, transaction_name);

 ...

 if (err) {
    newrelic_notice_error(txn, priority, ""Meaningful error message", "Error.class");
 }

 ...

 newrelic_end_transaction(&txn);
```

As noted above, to take full advantage of the error trace feature available
at New Relic's Error Analytics dashboard, applications should be linked using
GNU's `-rdynamic` linker flag. For the example `ex_notice_error.c` in the
`examples` directory, using this linker flag means that symbols are available
to list the function calls in the error's backtrace, like so:


```
   ./ex_notice_error.out(newrelic_get_stack_trace_as_json+0x2e) [0x40ae6d]
   ./ex_notice_error.out(newrelic_notice_error+0x204) [0x40a679]
   ./ex_notice_error.out(record_error+0x2c) [0x409822]
   ./ex_notice_error.out(main+0xe8) [0x40990d]
   /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf0) [0x7ffade6e7830]
   ./ex_notice_error.out(_start+0x29) [0x409729]
```

The backtrace shows that the `main()` function calls `record_error()` which
calls `newrelic_notice_error()`. Without the `-rdynamic` flag, the
function symbols are not available, and so the backtrace may not be as
meaningful:

```
   ./ex_notice_error.out() [0x4037fd]
   ./ex_notice_error.out() [0x403009]
   ./ex_notice_error.out() [0x4021b2]
   ./ex_notice_error.out() [0x40229d]
   /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf0) [0x7f39062d6830]
   ./ex_notice_error.out() [0x4020b9]
```

## About

### Thread safety
If you want to do anything beyond timing (e.g. events or attributes),
transactions are *not* thread safe. Scope transactions to single threads.

### Agent-daemon communication
The agent makes blocking writes to the daemon. Unless the kernel is resource-
starved, it will handle these writes efficiently.

### Memory management
The C Agent's memory use is proportional to the amount of data sent. The libc
calls `malloc` and `free` are used extensively. The dominant memory cost is
user-provided data, including custom attributes, events, and metric names.

### Elevated privileges (sudo)
By default the logs will be saved in "/var/log/newrelic/c_agent.log" and needs
elevated privileges to execute properly. To avoid running the application with
these privileges have the log files saved in a location that can be written to
by the current user.
