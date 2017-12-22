# New Relic C Agent Release Notes #

## Master Branch (In Progress/Current Working Version) ##

### New Features ###

- ...

### Bug Fixes ###

- ...

### End of Life Notices ###

### Upgrade Notices ###

* The function `bool newrelic_end_transaction(newrelic_txn_t** transaction)` has changed
  its return type from void to bool.  A value of true is returned if the transaction was
  properly sent.  An error is still logged on failure.

* The function `bool newrelic_destroy_app(newrelic_app_t** app)` has changed its return
  type from void to bool.

## 0.0.1-alpha ##

### New Features ###

- Initial Release

### Bug Fixes ###

- Initial Release, no bug fixes

### Notes ###

### Internal Changes ###
