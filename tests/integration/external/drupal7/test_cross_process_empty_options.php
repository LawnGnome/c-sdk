<?php

/*DESCRIPTION
Test that cross app tracing works with drupal_http_request when called with an
empty options array.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
*/

/*EXPECT
cat endpoint reached
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
    [{"name":"ExternalApp/127.0.0.1/432507#73695/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"}, [1,    0,    0,    0,    0,    0]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/drupal_7_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/drupal_7_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_cat_url(realpath(dirname(__FILE__)) . '/../../../include/cat_endpoint.php');
$rv = drupal_http_request($url, array());
echo $rv->data;
