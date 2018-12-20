<?php

/*DESCRIPTION
A simple DT request.
 */

/*INI
newrelic.distributed_tracing_enabled = true
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT
newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
ok - simple dt request
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_TRACED_ERRORS
null
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all", 
      "scope":"OtherTransaction/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, 
                                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, 
                                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, 
                                                             [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$ch = curl_init($url);
tap_not_equal(false, curl_exec($ch), "simple dt request");
curl_close($ch);
