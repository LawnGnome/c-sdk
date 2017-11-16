<?php

/*DESCRIPTION
The agent SHALL NOT add an X-NewRelic-Synthetics header to external calls when
the Synthetics feature is disabled.
*/

/*INI
newrelic.synthetics.enabled = false
*/

/*
 * The synthetics header contains the following JSON.
 *   [
 *     1,
 *     432507,
 *     "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
 *     "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
 *     "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
 *   ]
 */

/*HEADERS
X-NewRelic-Synthetics=PwcbVVVRDQMHSEMQRUNFFBZDG0EQFBFPAVALVhVKRkBBSEsTQxNBEBZERRMUERofEg4LCF1bXQxJW1xZCEtSUANWFQhSUl4fWQ9TC1sLWQgOXF0LRE8aXl0JDA9aXBoLCVxbHlNUUFYdD1UPVRVZX14IVAxcDF4PCVsVPA==
*/

/*EXPECT
cat endpoint reached
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Apdex"},                                  ["??", "??", "??", "??", "??",    0]],
    [{"name":"Apdex/Uri__FILE__"},                      ["??", "??", "??", "??", "??",    0]],
    [{"name":"External/all"},                           [   1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                        [   1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                 [   1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/432507#73695/all"}, [   1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat"},
                                                        [   1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat",
      "scope":"WebTransaction/Uri__FILE__"},            [   1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                         [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                         [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},             [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},    [   1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_cat_url(realpath(dirname(__FILE__)) . '/../../../include/cat_endpoint.php');

// Context Without Options
$context = stream_context_create();
echo file_get_contents($url, false, $context);
echo $context['http']['header'];
