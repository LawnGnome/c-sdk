<?php

/*DESCRIPTION
The agent should capture and report parse errors.
*/

/*EXPECT_REGEX
^\s*(PHP )?Parse error:\s*syntax error, unexpected '\}' in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "syntax error, unexpected '}'",
      "E_PARSE",
      {
        "stack_trace": [],
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
        "error.class": "E_PARSE",
        "error.message": "syntax error, unexpected '}'",
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

if (2 == 2) {
  lacks_semicolon()
}