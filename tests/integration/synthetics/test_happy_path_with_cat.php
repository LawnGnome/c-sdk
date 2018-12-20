<?php

/*DESCRIPTION
The agent should put the Synthetics metadata into the transaction trace
and the transaction event when a valid synthetics header is received.
Additionally, ensure a transaction trace is recorded regardless of the
trace threshold. Synthetics attributes should be added when CAT is enabled.
*/

/*
 * The synthetics header contains the following.
 *   [
 *     1,
 *     432507,
 *     "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
 *     "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
 *     "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
 *   ]
 */

/*INI
newrelic.transaction_tracer.threshold = '1h'
newrelic.special.expensive_node_min = 0
*/

/*HEADERS
X-NewRelic-Synthetics=PwcbVVVRDQMHSEMQRUNFFBZDG0EQFBFPAVALVhVKRkBBSEsTQxNBEBZERRMUERofEg4LCF1bXQxJW1xZCEtSUANWFQhSUl4fWQ9TC1sLWQgOXF0LRE8aXl0JDA9aXBoLCVxbHlNUUFYdD1UPVRVZX14IVAxcDF4PCVsVPA==
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
        "name": "WebTransaction/Uri__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "nr.guid": "??",
        "nr.apdexPerfZone": "??",
        "nr.syntheticsResourceId": "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
        "nr.syntheticsJobId": "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
        "nr.syntheticsMonitorId": "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
      },
      "?? user attributes",
      "?? agent attributes"
    ]
  ]
]
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "WebTransaction/Uri__FILE__",
      "__FILE__",
      [
        [
          0, {}, {},
          "?? trace details",
          {
            "agentAttributes": "??",
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??",
              "synthetics_resource_id": "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
              "synthetics_job_id": "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
              "synthetics_monitor_id": "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
            }
          }
        ],
        "?? string table"
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr"
    ]
  ]
]
*/

function hello() {
  echo "hello, world!<br>\n";
}

/*
 * Call at least one user function to ensure the transaction trace is
 * not empty. Otherwise, it will be discarded by the agent.
 */
hello();
