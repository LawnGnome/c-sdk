<?php

/*DESCRIPTION
The agent SHALL add an X-NewRelic-Synthetics header to external calls when
the current request is a synthetics request.
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
X-NewRelic-Synthetics=found cat endpoint reached
X-NewRelic-Synthetics=found cat endpoint reached
X-NewRelic-Synthetics=found cat endpoint reached
X-NewRelic-Synthetics=found cat endpoint reached
X-NewRelic-Synthetics=found cat endpoint reached
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Apdex"},                                  ["??", "??", "??", "??", "??",    0]],
    [{"name":"Apdex/Uri__FILE__"},                      ["??", "??", "??", "??", "??",    0]],
    [{"name":"External/all"},                           [   5, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                        [   5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                 [   5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/432507#73695/all"}, [   5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat"},
                                                        [   5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat",
      "scope":"WebTransaction/Uri__FILE__"},            [   5, "??", "??", "??", "??", "??"]],
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
