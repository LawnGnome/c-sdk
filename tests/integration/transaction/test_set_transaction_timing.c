/*DESCRIPTION
Manually setting a transaction's timing must alter its timestamp and duration.
*/

/*CONFIG
  cfg->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  cfg->transaction_tracer.duration_us = 1;
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/Action/basic",
        "timestamp": 1550619185.0761,
        "duration": 0.00100,
        "totalTime": 0.00100,
        "error": false
      },
      {},
      {}
    ]
  ]
]
*/

#include "common.h"

RUN_NONWEB_TXN("basic") {
  newrelic_segment_t* s = newrelic_start_segment(txn, "segment", "other");
  newrelic_set_transaction_timing(txn, 1550619185076100, 1000);
  newrelic_set_segment_timing(s, 0, 500);
  newrelic_end_segment(txn, &s);
}
