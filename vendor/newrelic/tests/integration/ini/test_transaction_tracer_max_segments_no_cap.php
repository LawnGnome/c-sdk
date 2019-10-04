<?php

/*DESCRIPTION
When the newrelic.transaction_tracer.max_segments is set to its default
value, the agent does not limit the number of custom segments created.
*/

/*INI
newrelic.transaction_tracer.max_segments=0
newrelic.transaction_tracer.threshold=0
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "OtherTransaction/php__FILE__",
      "\u003cunknown\u003e",
      [
        [
          0, {}, {}, [
            "?? start time", "?? end time", "ROOT", "?? root attributes", [
              [
                "?? start time", "?? end time", "`0", "?? node attributes", "?? node children"
              ]
            ]
	  ],
          {
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??"
            }
          }
        ],
        [
          "OtherTransaction\/php__FILE__",
          "Custom/my_function"
        ]
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      null
    ]
  ]
]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/my_function"},                     [1000, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function",
     "scope":"OtherTransaction/php__FILE__" },          [1000, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},   [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

function my_function(){

}

newrelic_add_custom_tracer("my_function");

for ($i = 0; $i < 1000; $i++) {
    my_function();
}
