#C Agent

Generic library to communicate with New Relic.

## Requirements
* 64-bit Linux with
  * glibc 2.5+ with NPTL support
  * kernel version 2.6.13 or higher (2.6.26+ highly recommended)
  * libpcre 8.20+
  * libpthread

## Getting started

Instrument your code. Consider the brief program below or look at the
`examples` directory for source and Makefiles highlighting particular features.

```c
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>

#include "libnewrelic.h"

int main (void) {
  int priority = 50;
  newrelic_app_t* app = 0;
  newrelic_txn_t* txn = 0;
  newrelic_app_config_t* config = 0;
  newrelic_segment_t *segment1, *segment2;

  newrelic_configure_log("./c_agent.log", NEWRELIC_LOG_INFO);

  config = newrelic_new_config("Your Application Name", "<LICENSE_KEY_HERE>");

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

  /* Add segments */
  segment1 = newrelic_start_segment(txn, "Stuff", "Secret");
  sleep(1);
  newrelic_end_segment(txn, &segment1);

  segment2 = newrelic_start_segment(txn, "More Stuff", "Secret");
  sleep(2);
  newrelic_end_segment(txn, &segment2);

  /* End web transaction */
  newrelic_end_transaction(&txn);

  /* Start and end a non-web transaction */
  txn = newrelic_start_non_web_transaction(app,
                                           "veryImportantOtherTransaction");
  sleep(1);

  newrelic_end_transaction(&txn);

  newrelic_destroy_app(&app);

  return 0;
}
```

Compile and link your application against the static library, `libnewrelic.a`. There
are two considerations to make during the linking step. First, because
`libnewrelic.a` is offered as a static library, because it is already linked
with the `libpcre`, `libpthread`, and `lm` libraries, you must also link
against these three libraries to avoid symbol collisions in the linking step.

Second, to take full advantage of error traces at New Relic's Error Analytics
dashboard, link your application using GNU's `-rdynamic` linker flag.
Doing so means that more meaningful information appears in the stack trace
for the error recorded on a transaction using `newrelic_notice_error()`.

With these two considerations in mind, one may compile and link a simple application
like so:

```sh
gcc -o test_app test_app.c -L. -lnewrelic -lpcre -lm -pthread -rdynamic
```

Start the daemon:

```sh
./bin/daemon -f -logfile stdout -loglevel debug
```

Run your test application and check the `c-agent.log` file for output.

## Features
* Transactions
  * Transaction events
  * Web and non-web transactions
  * Segments
  * Custom attributes
  * Error instrumentation
  * Custom events
  * Custom metrics
  * Manual timing  
* Logging levels

### Configuration

The C agent's configuration is stored in a `newrelic_app_config_t`, which is created
like so.

```c
  newrelic_app_config_t* config;
  config = newrelic_new_app_config(app_name, license_key);
```

The two required values for any configuration are the application's name, or 
`app_name` and the New Relic `license_key`.  The remaining fields are optional
and users likely do not need to change the default values of the optional fields.

The configuation's optional fields include the application's `log_level` 
and `log_filename`, used to configure the appliction's logging. They also 
include `transaction_tracer` and `datastore_tracer`.  All the fields of 
`newrelic_app_config_t` are detailed in `libnewrelic.h`.

The `transaction_tracer` field configures the behavior of the C agent's 
transaction tracer using a `newrelic_transaction_tracer_config_t`.  At New Relic, 
a transaction trace gives a detailed snapshot of a single transaction in your 
application. A transaction trace records the available function calls, database 
calls, and external calls. The `transaction_tracer` configuration describes how 
the C agent chooses transactions for reporting to New Relic.  This includes
the threshold over which a transaction becomes traced, whether slow sql 
queries are reported, the format of reported sql, and the threshold for
slow sql queries.  By default, the configuration returned by 
`newrelic_new_app_config()` enables transaction traces, with the threshold 
set to `NEWRELIC_THRESHOLD_IS_APDEX_FAILING`. This default threshold is based on 
[Apdex](https://docs.newrelic.com/docs/apm/new-relic-apm/apdex/apdex-measure-user-satisfaction),
an industry standard for measuring user satisfaction. All the fields of 
`newrelic_transaction_tracer_config_t` are detailed in `libnewrelic.h`.


The `datastore_tracer` field configures how datastore segments are recorded
in a transaction, whether or not the C agent reports database host names,
database ports, and database names. By default, the configuration returned 
by `newrelic_new_app_config()` configures datastore segments with `instance_reporting` 
and `database_name_reporting` both enabled. All the fields of 
`newrelic_datastore_segment_config_t` are detailed in `libnewrelic.h`.

### Segment Instrumentation

The agent provides several API functions for creating optional *segment*
instrumentation.  Segments allow you to measure the time taken by specific
portions of a transaction.  The agent allows you to create two different
segment types.

* External Segments
* Datastore Segments

### External Segments

The agent provides two functions, `newrelic_start_external_segment()` and
`newrelic_end_external_segment()` that allows end-user-programmers to create
external segments. External segments appear in the transaction "Breakdown
table" and in the "External services" page in APM.

* [More info on External Services page](https://docs.newrelic.com/docs/apm/applications-menu/monitoring/external-services-page)

Timing an external segment transaction is a three-step process.

1. Create a `newrelic_external_segment_params_t` struct that describes the external
   segment

2. Start the timer with `newrelic_start_external_segment()`

3. Stop the timer with `newrelic_end_external_segment()`

Here are those three steps in code.  The `txn` variable below is a
transaction, created via `newrelic_start_web_transaction()` or
`newrelic_start_non_web_transaction()`. Segments may only be recorded on
active transactions.

```c
    newrelic_external_segment_params_t params = {
        .procedure = "GET",
        .uri       = "https://httpbin.org/delay/1",
    };

    newrelic_external_segment_t* segment = newrelic_start_external_segment(txn, &params);

    // The external call to be timed goes here

    newrelic_end_external_segment(txn, &segment);
```

The `newrelic_external_segment_params_t` struct contains a list of parameters
that New Relic will use to identify a segment. These parameters also drive
the user interface in APM. Only the `.uri` field is required. Documentation
for each field can be found in `libnewrelic.h`. You can also find a working
code sample in `examples/ex_external.c`.

**IMPORTANT**:  In order to ensure accurate timing, external segments cannot
be nested within other external segments, and cannot be nested with datastore
segments. You **must** call `newrelic_end_external_segment()` before starting
a new external segment with `newrelic_start_external_segment()`. Starting a
new segment before the previous segment has ended will produce undefined
results, and should be avoided.

### Datastore Segments

The agent provides two functions, `newrelic_start_datastore_segment()` and
`newrelic_end_datastore_segment()` that allow you to create datastore
segments.  APM uses segments recorded in this manner in the
[Databases and Slow Queries](https://docs.newrelic.com/docs/apm/applications-menu/monitoring/databases-slow-queries-page)
of APM.  Segments created with these functions
also populate the `databaseDuration` attribute of a
[New Relic Insights](https://docs.newrelic.com/docs/insights/use-insights-ui/getting-started/introduction-new-relic-insights)
Transaction event.

To record a datastore segment on an active transaction, you'll need to

1. Create a `newrelic_datastore_segment_params_t` struct that describes the
   datastore segment

2. Start the timer with `newrelic_start_datastore_segment()`

3. Stop the timer with `newrelic_end_datastore_segment()`

Here are those three steps in code. The `txn` variable below is a
transaction, created via `newrelic_start_web_transaction()` or
`newrelic_start_non_web_transaction()`.  You may only record segments on
active transactions.

```c
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
```

The `newrelic_datastore_segment_params_t` struct contains a list of
parameters that New Relic uses to identify your segment. New Relic also uses
these values to drive its user interface in APM. Only the `.product` field is
required. You can find documentation for each field in `libnewrelic.h`. You
can also find a working code sample in `examples/ex_datastore.c`.

**IMPORTANT**:  In order to ensure accurate timing, datastore segments cannot
be nested within other datastore segments, and cannot be nested with external
segments. You **must** call `newrelic_end_datastore_segment()` before
starting a new datastore or external segment. Starting a new segment before
the previous segment has ended will produce undefined results, and should be
avoided.


#### Slow Query Tracing for Datastore Segments

When you send New Relic datastore segments, those segments may be eligible
for
[Slow Query tracing](https://docs.newrelic.com/docs/apm/applications-menu/monitoring/viewing-slow-query-details).
Only SQL-like databases are eligible for slow query tracing.   If your
datastore segment's `.product` is set to `Firebird`, `Informix`, `MSSQL`,
`MySQL`, `Oracle`, `Postgres`, `SQLite`, or `Sybase`, the C-Agent will make
your segment eligible for slow query tracing.

Both the time threshold to trigger a slow query trace and whether slow query
tracing is enabled are controlled via the C-Agent's **application**
configuration, specifically the `datastore_reporting.*` fields.

```c
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

```c
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

### Creating Custom Events

The agent provides a Custom Events API that allows users to send custom events to
New Relic Insights.  To send an event, start a transaction and use the
`newrelic_create_custom_event` and `newrelic_record_custom_event` functions

```c
    // txn is a newrelic_txn_t*, created via newrelic_start_web_transaction
    // see above examples for more information

    newrelic_custom_event_t* custom_event=0;
    custom_event = newrelic_create_custom_event("aTypeForYourEvent");
    newrelic_record_custom_event(txn, &custom_event);
```

You can also add `int`, `long`, `double`, and `char*` (string) attributes to your event
via the `newrelic_custom_event_add_*` family of functions.

```c
    newrelic_custom_event_t* custom_event=0;
    custom_event = newrelic_create_custom_event("aTypeForYourEvent");

    newrelic_custom_event_add_attribute_int(custom_event, "keya", 42);
    newrelic_custom_event_add_attribute_long(custom_event, "keyb", 84);
    newrelic_custom_event_add_attribute_double(custom_event, "keyc", 42.42);
    newrelic_custom_event_add_attribute_string(custom_event, "keyd", "A string");

    newrelic_record_custom_event(txn, &custom_event);
```

Don't forget to review the
[Insights custom data requirements and limits](https://docs.newrelic.com/docs/insights/insights-data-sources/custom-data/insights-custom-data-requirements-limits)
for guidance on what are and aren't allowed values inside of a custom event.

#### Memory Lifecycle for Custom Events

Under normal circumstances, the `newrelic_record_custom_event` function will
free the memory allocated when you called `newrelic_create_custom_event`.
If you you end up creating a custom event that you do NOT need to record,
you may use the `newrelic_discard_custom_event` function to free the allocated
memory in order to avoid leaks in your program.

```c
    newrelic_custom_event_t* custom_event=0;
    custom_event = newrelic_create_custom_event("aTypeForYourEvent");

    // circumstances

    newrelic_discard_custom_event(&custom_event);
```

### Custom Metrics

The agent provides the function `newrelic_record_custom_metric`, which allows users to
record custom timing metrics. To create a custom metric, just provide a name/identifier
and an amount of time in milliseconds to the function, (along with the active
transaction).

```c
    // txn is a newrelic_txn_t*, created via newrelic_start_web_transaction

    // Record a metric value of 100ms in the transaction txn
    newrelic_record_custom_metric(txn, "Custom/YourMetric/Label", 100);
```

**Important**: Start all metric names with `Custom/`; for example,
`Custom/MyMetric/My_label`. The `Custom/` prefix is required for all custom metrics.

To learn more about collecting custom metrics, including naming strategies to
avoid metric grouping issues (also calls MGIs) read the
[Collect Custom Metrics](https://docs.newrelic.com/docs/agents/manage-apm-agents/agent-data/collect-custom-metrics)
documentation.

## Manual timing

The C agent provides a pair of API calls with which users may manually change
transaction timing and individual segment timing. Though the C agent is 
incredibly effective at automatically timing transactions and segments, 
providing users with this kind of timing allows them to achieve consistent 
timing values across the New Relic Platform and their own internal monitoring
systems.

Users may manually change timing for active transactions using 
`newrelic_set_transaction_timing()`, as shown in the example below.

```c
  txn = newrelic_start_web_transaction(app, "ExampleWebTransaction");
  newrelic_set_transaction_timing(txn, now_us() + 50, 500000);
  newrelic_end_transaction(&txn);
```

It is important to note than a transaction must be active in order to change its
timing. Attempting to change timing for a transaction after it has been ended 
results in undefined behavior. 

Similarly, users may manually change timing for active segments using 
`newrelic_set_segment_timing()`, as shown in the example below.

```c 
  seg_c = newrelic_start_segment(txn, "C", "Secret");

  /* Manually change seg_c so that starts 10 us after the start 
   * of the transaction and lasts half a second. */
  newrelic_set_segment_timing(seg_c, 10, 500000);
  newrelic_end_segment(txn, &seg_c);
```

Again, it is important to note that a segment must be active in order to change
its timing. Attempting to change timing for a segment after it has been ended 
results in undefined behavior.

Users may want to coordinate their use of these two API calls. Using just one
or the other may result in inconsistent transaction summary values at the New 
Relic user interface. In the example below, a transaction is manually changed
with a duration of only 500 milliseconds, even though its custom segment 
lasted a full second.

```c
  txn = newrelic_start_web_transaction(app, "ExampleWebTransaction");

  seg = newrelic_start_segment(txn, "Custom Segment A", "Category");
  sleep(1);
  newrelic_end_segment(txn, &seg);

  /* Manually change the transaction timing so that the start time is
   * 50 us from now and the duration is 500 ms */ 
  newrelic_set_transaction_timing(txn, now_us() + 50, 500000);
  newrelic_end_transaction(&txn);
```

At the New Relic user interface showing the transaction trace, 
"Custom Segment A" appears to have taken 1000 ms (correct) while the top-level
segment for the web transaction, "ExampleWebTransaction"  appears to have
taken -500 ms (incorrect). Even so, because of the call to 
`newrelic_set_transaction_timing()` the response time shown for the 
transaction is 500 ms. Essentially, the user interface takes the necessary
steps to make the segment times add up, even when calls to 
`newrelic_set_transaction_timing()` or `newrelic_set_segment_timing()` 
break the mathematics.

It is recommended that users coordinate their calls to 
`newrelic_set_segment_timing()` and `newrelic_set_transaction_timing()`
so that the timing values are consistent. In the example below, the call
to `newrelic_set_segment_timing()` sets the segment to a duration of 500 ms
while the call to `newrelic_set_transaction_timing()` sets the transaction
duration to 2 seconds.

```c
  txn = newrelic_start_web_transaction(app, "ExampleWebTransaction");

  seg = newrelic_start_segment(txn, "Custom Segment A", "Category");
  sleep(1);

  /* Manually change the segment timing so that it starts at the
   * same time as the transaction and the duration is 500 ms */
  newrelic_set_segment_timing(seg, 0, 500000);
  newrelic_end_segment(txn, &seg);

  /* Manually change the transaction timing so that the start time is
   * now and the duration is 500 ms */ 
  newrelic_set_transaction_timing(txn, now_us(), 2000000);
  newrelic_end_transaction(&txn);
```  

You can find working examples of transaction and segment timing calls 
in `examples/ex_timing.c` and `examples/ex_segment.c`.

All told, this pair of API calls offers users a powerful means to customize
transaction and segment timing values according to their systems' needs. But
with great power comes great responsibility.  Misuse of these calls can create
summary values that are inconsistent at the New Relic user interface.

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

### Logging
By default, logs are output to standard error. You may change this by calling
`newrelic_configure_log()`.
