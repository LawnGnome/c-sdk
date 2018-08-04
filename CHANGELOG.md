# New Relic C Agent Release Notes #

## Master Branch (In Progress/Current Working Version) ##

### New Features ###

### Bug Fixes ###

### End of Life Notices ###

### Upgrade Notices ###

## 0.0.5 ##

### Upgrade Notices ###

- To increase security, TLS will now always be used in communication between 
the C Agent and New Relic servers.  This change should be invisible to
customers since the C Agent did not previously offer any way to disable TLS.

## 0.0.4 ##

### New Features ###

- Added support for creating datastore segments using the new
  `newrelic_start_datastore_segment()` and `newrelic_end_datastore_segment()`
  functions. See `libnewrelic.h`, `GUIDE.md` and `examples/ex_datastore.c` for usage
  information.
   
- Added support for creating external segments using the new
  `newrelic_start_external_segment()` and `newrelic_end_external_segment()`
  functions. See `libnewrelic.h`, `GUIDE.md` and `examples/ex_external.c` for usage
  information.

- Added configuration options to control transaction trace generation.

## 0.0.3-alpha ##

### New Features ###

- Customers may now use `newrelic_notice_error()` to record transaction errors that
are not automatically handled by the agent. Errors recorded in this manner are displayed in
[error traces](https://docs.newrelic.com/docs/apm/applications-menu/error-analytics/error-analytics-explore-events-behind-errors#traces-table)
and are available to query through
[New Relic Insights](https://docs.newrelic.com/docs/insights/use-insights-ui/getting-started/introduction-new-relic-insights).
See `libnewrelic.h` for usage information.

### Bug Fixes ###

- At times, when the daemon removed an application after a 10-minute timeout, the agent
daemon exited in failure. This has been fixed.

### End of Life Notices ###

### Upgrade Notices ###

* The function `bool newrelic_end_transaction(newrelic_txn_t** transaction)` has changed
  its return type from void to bool. A value of true is returned if the transaction was
  properly sent. An error is still logged on failure.

* The function `bool newrelic_destroy_app(newrelic_app_t** app)` has changed its return
  type from void to bool.

## 0.0.1-alpha ##

### New Features ###

- Initial Release

### Bug Fixes ###

- Initial Release, no bug fixes

### Notes ###

### Internal Changes ###
