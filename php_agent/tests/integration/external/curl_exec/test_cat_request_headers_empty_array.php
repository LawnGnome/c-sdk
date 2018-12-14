<?php

/*DESCRIPTION
Test that CAT works with curl_exec when curl_setopt+CURLOPT_HTTPHEADER is called
with an empty array.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT
cat endpoint reached
ok - cat successful
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
    [{"name":"External/all"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/432507#73695/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat"}, 
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = make_cat_url(realpath(dirname(__FILE__)) . '/../../../include/cat_endpoint.php');
$ch = curl_init($url);
curl_setopt($ch, CURLOPT_HTTPHEADER, array());
tap_not_equal(false, curl_exec($ch), "cat successful");
curl_close($ch);
