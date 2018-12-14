<?php

/*DESCRIPTION
Test that newrelic_set_appname works with two parameters.
*/

/*EXPECT
ok - newrelic_set_appname appname and license
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"see_me"},                                 [1, 1, 1, 1, 1, 1]],
    [{"name":"Supportability/api/custom_metric"},       [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/after"},   [1, 0, 0, 0, 0, 0]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$appname = ini_get("newrelic.appname");
$license = ini_get("newrelic.license");

newrelic_custom_metric('ignore_me', 1e+3);
$result = newrelic_set_appname($appname, $license);
tap_assert($result, 'newrelic_set_appname appname and license');

newrelic_custom_metric('see_me', 1e+3);
