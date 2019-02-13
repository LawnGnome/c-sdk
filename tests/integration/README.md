# C Agent integration tests

Basically C Agent integration tests can be written the same way as PHP Agent
integration tests. All the same comment keyword blocks are supported. The only
exception to consider is that for C Agent tests the `INI` block will be
ignored; instead the `CONFIG` block is relevant.

## `CONFIG`

The `CONFIG` comment block must contain valid C code that manipulates a
`newrelic_app_config_t* cfg` data structure. Here's an example:

```c
/*CONFIG
  cfg->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  cfg->transaction_tracer.duration_us = 1;
*/
```

This code is injected in the test executable as it is given, no preprocessing,
sandboxing or additional checks are done.

## Write tests using predefined helper macros

C Agent integration tests can include the file `common.h`. This file provides a
`main` function as well as some macros that should avoid repetitive boilerplate 
code in tests.

The following macros are provided:

* `RUN_APP()`: Initializes a `newrelic_app_t*` and provides it as `app` in the
  code block following the macro.
* `RUN_WEB_TXN(M_txnname)`: In addition to providing `app` as does `RUN_APP()`,
  this also provides an initialized `newrelic_txn_t *` web transaction named
`txn`. The string given as `M_txnname` is the transaction name.
* `RUN_NONWEB_TXN(M_txnname)`: In addition to providing `app` as does `RUN_APP()`,
  this also provides an initialized `newrelic_txn_t *` non-web transaction named
`txn`. The string given as `M_txnname` is the transaction name.

In each test file only one of those macros can be used.

By default an agent log file `./c_agent.log` is created. This path can be 
changed by setting the environment variable `NEW_RELIC_LOG_FILE`.

## Write tests by providing your own `main` function

Tests can also be written providing a custom `main` function. In this case
`common.h` must not be included.

When using this approach, the following compile time defines are important:

* `NEW_RELIC_CONFIG`: The contents of the `CONFIG` comment block.
* `NEW_RELIC_DAEMON_TESTNAME`: The name of the test. This has to be used as
  application name, otherwise the integration runner cannot properly assign
  harvests to test cases.

In addition, the integration runner sets the following environment variables at
test runtime:

* `NEW_RELIC_LICENSE_KEY`: The license key used by the integration runner.
* `NEW_RELIC_DAEMON_SOCKET`: The socket on which the integration runner
  accepts connections.
