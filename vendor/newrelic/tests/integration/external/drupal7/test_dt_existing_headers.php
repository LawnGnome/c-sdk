<?php

/*DESCRIPTION
Test that DT works with drupal_http_request when called with a single header.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.distributed_tracing_enabled = true
*/

/*EXPECT
newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"External/127.0.0.1/all"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all", 
      "scope":"OtherTransaction/php__FILE__"},            [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                             [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"},   [1,    0,    0,    0,    0,    0]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, 
                                                          [2, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/drupal_7_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/drupal_7_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$rv = drupal_http_request($url, array('headers' => array("kiteboarding" => "windsurfing")));
echo $rv->data;
$rv = drupal_http_request($url, array('headers' => array(CUSTOMER_HEADER => "windsurfing")));
echo $rv->data;