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
X-NewRelic-Synthetics=found cat endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found cat endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found cat endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found cat endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found cat endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found cat endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found cat endpoint reached
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Apdex"},                                  ["??", "??", "??", "??", "??",    0]],
    [{"name":"Apdex/Uri__FILE__"},                      ["??", "??", "??", "??", "??",    0]],
    [{"name":"External/all"},                           [  12, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                        [  12, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                 [  12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/432507#73695/all"}, [  12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat"},
                                                        [  12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat",
      "scope":"WebTransaction/Uri__FILE__"},            [  12, "??", "??", "??", "??", "??"]],
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

// Context Empty Array Options
$opts = array();
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context Without ['http']
$opts = array('kitesurfing' => array('windsurfing' => 'zen'));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http'] = array()
$opts = array('http' => array());
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = array()
$opts = array('http' => array('method' => 'GET', 'header' => array()));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = ''
$opts = array('http' => array('method' => 'GET', 'header' => ''));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\n"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\n"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate\r\n"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate\r\n"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = array(CUSTOMER_HEADER . ": zipzap")
$opts = array('http' => array('method' => 'GET', 'header' => array(CUSTOMER_HEADER . ": zipzap")));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = array(CUSTOMER_HEADER . ": zipzap", "icecream-two: chocolate")
$opts = array('http' => array('method' => 'GET', 'header' => array(CUSTOMER_HEADER . ": zipzap", "icecream-two: chocolate")));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);
