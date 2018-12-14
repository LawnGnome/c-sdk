<?php

require_once(realpath(dirname(__FILE__)) . '/config.php');
require_once(realpath(dirname(__FILE__)) . '/polyfill.php');

newrelic_name_transaction("cat");

$request_headers = array_change_key_case(getallheaders());

if (!array_key_exists(strtolower(X_NEWRELIC_ID), $request_headers)) {
    echo X_NEWRELIC_ID . "=missing ";
}
if (!array_key_exists(strtolower(X_NEWRELIC_TRANSACTION), $request_headers)) {
    echo X_NEWRELIC_TRANSACTION . "=missing ";
}

if (array_key_exists(strtolower(X_NEWRELIC_SYNTHETICS), $request_headers)) {
  echo X_NEWRELIC_SYNTHETICS . "=found ";
}

// Flush all buffers so that the CAT response header gets written.
$numBuffers = ob_get_level();
for ($i = 0; $i < $numBuffers; $i++) {
    if (!ob_end_flush()) {
        break;
    }
}

$response_header_created = false;
$response_headers = array_change_key_case(headers_list());
foreach ($response_headers as $value) {
    if (0 === stripos($value, X_NEWRELIC_APP_DATA)) {
        $response_header_created = true;
        break;
    }
}

if (!$response_header_created) {
    echo X_NEWRELIC_APP_DATA . "=missing ";
}

if (array_key_exists(strtolower(CUSTOMER_HEADER), $request_headers)) {
    echo CUSTOMER_HEADER . "=found ";
}

echo "cat endpoint reached\n";
