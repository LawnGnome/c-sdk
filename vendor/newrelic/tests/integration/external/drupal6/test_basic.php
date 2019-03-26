<?php

/*DESCRIPTION
Test that a simple request through drupal_http_request gets instrumented.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
*/

/*EXPECT
Hello world!
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"External/127.0.0.1/all"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"}, [1,    0,    0,    0,    0,    0]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/drupal_6_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/drupal_6_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . $EXTERNAL_HOST; 
$rv = drupal_http_request($url);
echo $rv->data;

