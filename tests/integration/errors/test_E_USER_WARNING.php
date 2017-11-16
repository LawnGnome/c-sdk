<?php

/*DESCRIPTION
The agent should capture and report user-generated warnings along with a stack
trace.
*/

/*EXPECT_REGEX
^\s*(PHP )?Warning:\s*Sample E_USER_WARNING in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Sample E_USER_WARNING",
      "E_USER_WARNING",
      {
        "stack_trace": [
          " in trigger_error called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
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
        "error.class": "E_USER_WARNING",
        "error.message": "Sample E_USER_WARNING",
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

trigger_error("Sample E_USER_WARNING", E_USER_WARNING);
