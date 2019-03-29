<?php

/*DESCRIPTION
Span events are generated and external span event fields are set correctly.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
    "events_seen": 3
  },
  [
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "timestamp": "??"
      },
      {},
      {}
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "??",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "timestamp": "??"
      },
      {},
      {}
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "External\/127.0.0.1\/all",
        "guid": "??",
        "type": "Span",
        "category": "http",
        "priority": "??",
        "sampled": true,
        "timestamp": "??",
        "parentId": "??",
        "span.kind": "client",
        "component": "Drupal"
      },
      {},
      {
        "http.url": "??",
        "http.method": "GET"
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/drupal_6_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/drupal_6_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
drupal_http_request($url);
