<?php

require_once(realpath(dirname(__FILE__)) . '/config.php');
require_once(realpath(dirname(__FILE__)) . '/polyfill.php');

newrelic_name_transaction("tracing");

$request_headers = array_change_key_case(getallheaders());

if (array_key_exists(strtolower(DT_NEWRELIC), $request_headers)) {
  echo DT_NEWRELIC . "=found ";
}

if (!array_key_exists(strtolower(X_NEWRELIC_ID), $request_headers)) {
    echo X_NEWRELIC_ID . "=missing ";
}
if (!array_key_exists(strtolower(X_NEWRELIC_TRANSACTION), $request_headers)) {
    echo X_NEWRELIC_TRANSACTION . "=missing ";
}

if (array_key_exists(strtolower(X_NEWRELIC_SYNTHETICS), $request_headers)) {
  echo X_NEWRELIC_SYNTHETICS . "=found ";
}

$response_header_created = false;
$response_headers = array_change_key_case(headers_list());
foreach ($response_headers as $value) {
    if (0 === stripos($value, X_NEWRELIC_APP_DATA)) {
        $response_header_created = true;
        break;
    }
}

if (array_key_exists(strtolower(CUSTOMER_HEADER), $request_headers)) {
    echo CUSTOMER_HEADER . "=found ";
}

echo "tracing endpoint reached\n";
