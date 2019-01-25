<?php

/*DESCRIPTION
Test that CAT works with file_get_contents without a context.
*/

/*EXPECT
tracing endpoint reached
tracing endpoint reached
tracing endpoint reached
tracing endpoint reached
tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                             [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                   [5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/432507#4741547/all"}, [5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#4741547/WebTransaction/Custom/tracing"},
                                                          [5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#4741547/WebTransaction/Custom/tracing",
      "scope":"OtherTransaction/php__FILE__"},            [5, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

/*
 * Seeking (offset) is not supported with remote files, so testing if this
 * parameter is maintained is not important. Similarly, I don't think
 * use_include_path has any effect on remote files.
 */

// only URL
echo file_get_contents ($url);

// no context
echo file_get_contents ($url, false);

// NULL context
echo file_get_contents ($url, false, NULL);

// NULL context with offset and maxlen
echo file_get_contents ($url, false, NULL, 0, 50000);

// small maxlen
echo file_get_contents ($url, false, NULL, 0, 128);
