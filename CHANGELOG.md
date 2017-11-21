# New Relic C Agent Release Notes #

## Master ##

### End of Life Notices ###

### New Features ###

### Upgrade Notices ###

* The function `bool newrelic_end_transaction(newrelic_txn_t** transaction)` has changed
  its return type from void to bool.  A value of true is returned if the transaction was
  properly sent.  An error is still logged on failure.

### Notes ###

### Bug Fixes ###
 
### Internal Changes ###
