<?php
/*DESCRIPTION
Tests that PHP sends error messages when LASP configuration
indicates allow_raw_exception_messages:{enabled:true}, and agent
is configured to allow error messages.
*/

/*INI
newrelic.allow_raw_exception_messages=1
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "Exception",
        "error.message": "Uncaught exception 'Exception' with message 'Hey look, an error' in __FILE__:??",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??"
      },
      {},
      {}
    ]
  ]
]
*/
throw new Exception("Hey look, an error");