<?php

/*DESCRIPTION
Test that CAT works with file_get_contents when the customer has added a header
through the default context.
*/

/*EXPECT
Customer-Header=found cat endpoint reached
Customer-Header=found cat endpoint reached
Customer-Header=found cat endpoint reached
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                           [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/432507#73695/all"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat"},
                                                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#73695/WebTransaction/Custom/cat",
      "scope":"OtherTransaction/php__FILE__"},          [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

/*
 * If a context is not provided to file_get_contents, our agent should add the
 * cross process headers to the default context and not a brand new context.
 * Therefore, this test uses the endpoint which requires the $CUSTOMER_HEADER
 * header to make it to the external application.
 */
$url = "http://" . make_cat_url(realpath(dirname(__FILE__)) . '/../../../include/cat_endpoint.php');

$http_header['http']['header'] = CUSTOMER_HEADER . ": ok";
stream_context_set_default ($http_header);

echo file_get_contents ($url);

/*
 * Repeat the call multiple times to ensure that the default context is not
 * modified.
 */
echo file_get_contents ($url);
echo file_get_contents ($url);
