<?php

/*DESCRIPTION
A simple cross process request.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT
tracing endpoint reached
ok - simple cat request
*/

/*EXPECT_RESPONSE_HEADERS
X-NewRelic-App-Data=??
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
    [{"name":"ExternalApp/127.0.0.1/432507#4741547/all"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#4741547/WebTransaction/Custom/tracing"},
                                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#4741547/WebTransaction/Custom/tracing",
      "scope":"OtherTransaction/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},       [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$ch = curl_init($url);
tap_not_equal(false, curl_exec($ch), "simple cat request");
curl_close($ch);