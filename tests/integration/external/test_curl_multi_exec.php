<?php

/*DESCRIPTION
Test that the agent instruments curl_multi_exec.
*/

/*EXPECT
Hello world!
Hello world!
Hello world!
Hello world!
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                          ["??", "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                     ["??", "??", "??", "??", "??", "??"]],
    [{"name":"External/curl_multi_exec/all"},          ["??", "??", "??", "??", "??", "??"]],
    [{"name":"External/curl_multi_exec/all",
      "scope":"OtherTransaction/php__FILE__"},         ["??", "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$url = "http://" . $EXTERNAL_HOST;

$ch1 = curl_init();
curl_setopt($ch1, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);

$ch2 = curl_init();
curl_setopt($ch2, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch2, CURLOPT_RETURNTRANSFER, 1);

$ch3 = curl_init();
curl_setopt($ch3, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch3, CURLOPT_RETURNTRANSFER, 1);

$ch4 = curl_init();
curl_setopt($ch4, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch4, CURLOPT_RETURNTRANSFER, 1);

$mh = curl_multi_init();

curl_multi_add_handle($mh, $ch1);
curl_multi_add_handle($mh, $ch2);
curl_multi_add_handle($mh, $ch3);
curl_multi_add_handle($mh, $ch4);

$active = null;

do {
  curl_multi_exec($mh, $active);
} while ($active > 0);

echo curl_multi_getcontent($ch1) . "\n";
echo curl_multi_getcontent($ch2) . "\n";
echo curl_multi_getcontent($ch3) . "\n";
echo curl_multi_getcontent($ch4) . "\n";

curl_multi_remove_handle($mh, $ch1);
curl_multi_remove_handle($mh, $ch2);
curl_multi_remove_handle($mh, $ch3);
curl_multi_remove_handle($mh, $ch4);
curl_multi_close($mh);

