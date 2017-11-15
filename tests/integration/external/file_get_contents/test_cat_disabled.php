<?php

/*DESCRIPTION
The agent SHALL NOT add X-NewRelic-ID and X-NewRelic-Transaction headers
to external calls when cross application tracing is disabled.
*/

/*INI
newrelic.cross_application_tracer.enabled = false
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
    [{"name":"External/all"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_cat_url(realpath(dirname(__FILE__)) . '/../../../include/cat_endpoint.php');

// Context Without Options
$context = stream_context_create();
echo file_get_contents($url, false, $context);
echo $context['http']['header'];
