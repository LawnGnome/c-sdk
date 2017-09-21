## C Agent

Generic library to communicate with New Relic.

### Requirements
* 64-bit Linux with
    * glibc 2.5+ with NPTL support; or musl libc version 1.1 or higher
    * kernel version 2.6.13 or higher (2.6.26+ highly recommended)

### Getting started

Instrument your code:

```
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>

#include "libnewrelic.h"

int main (void) {
  newrelic_app_t *app = 0;
  newrelic_txn_t *txn = 0;
  newrelic_config_t *config = 0;

  config = newrelic_new_config ("C Agent Test App", "<LICENSE_KEY_HERE>");
  strcpy (config->daemon_socket, "/tmp/.newrelic.sock");
  strcpy (config->log_filename, "./c_agent.log");
  config->log_level = LOG_INFO;

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app (config, 10000);
  free (config);

  /* Start a web transaction */
  txn = newrelic_start_web_transaction (app, "veryImportantTransaction");

  /* Add attributes and check/alert if they fail */
  if (!newrelic_transaction_add_attribute_int (txn, "CustomInt", INT_MAX))
    printf ("Failed to add custom Int\n");
  if (!newrelic_transaction_add_attribute_long (txn, "CustomLong", LONG_MAX))
    printf ("Failed to add custom Long\n");

  sleep (0.5);


  /* End web transaction */
  newrelic_end_transaction (&txn);

  /* Start a non-web transaction */
  txn = newrelic_start_non_web_transaction (app, "veryImportantTransactionPart2");

  /* Add attributes and check/alert if they fail */
  if (!newrelic_transaction_add_attribute_double (txn, "CustomDbl", DBL_MAX))
    printf ("Failed to add custom Double\n");
  if (!newrelic_transaction_add_attribute_string (txn, "CustomStr", "String String String"))
    printf ("Failed to add custom String\n");

  sleep (0.5);

  /* End non-web transaction */
  newrelic_end_transaction (&txn);

  newrelic_destroy_app (&app);

  return 0;
}
```


Link your app against the library. For example:

```
gcc -o test_app test_app.c -L. -lnewrelic -lpcre -pthread
```

Start the daemon:

```
../bin/daemon -f -logfile stdout -loglevel debug
```

### Features
* [x] Named transactions
* [x] Web and non-web transactions events
* [X] Transaction event attributes
* [x] Logging levels

### About

#### Thread safety
If you want to do anything beyond timing (e.g. events or attributes), transactions are *not* thread safe. Scope transactions to single threads.

#### Agent-daemon communication
The agent makes blocking writes to the daemon. Unless the kernel is resource-starved, it will handle these writes efficiently.

#### Memory management
The C Agent's memory use is proportional to the amount of data sent. The libc allocator `malloc` (and `free`) is used extensively. The dominant memory cost is user-provided data, including custom attributes, events, and metric names.

#### Elevated privileges (sudo)
By default the logs will be saved in "/var/log/newrelic/c_agent.log" and needs elevated privileges to execute properly. To avoid running the application with these privileges have the log files saved in a location that can be written to by the current user.
