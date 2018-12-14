<?php

/*DESCRIPTION
We do not yet support http library version 2.
*/

/*SKIPIF
<?php

if (!extension_loaded('http')) {
    die("skip: http extension required\n");
}

if (version_compare(phpversion('http'), '2.0.0', '<')) {
    die("skip: http 2.x or newer required\n");
}
*/

/*EXPECT
X-NewRelic-ID=missing X-NewRelic-Transaction=missing X-NewRelic-App-Data=missing cat endpoint reached
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_cat_url(realpath(dirname(__FILE__)) . '/../../../include/cat_endpoint.php');

$request = new http\Client\Request("GET", $url);
$driver = "curl";
$client = new http\Client($driver);
try {
    $client->enqueue($request);
    $client->send();
    $rv = $client->getResponse();
    echo $rv->getBody();
} catch (http\Exception\RuntimeException $ex) {
    echo "exception";
    echo $ex;
}
