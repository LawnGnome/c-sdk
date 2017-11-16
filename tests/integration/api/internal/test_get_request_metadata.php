<?php

/*DESCRIPTION
newrelic_get_request_metadata() should return headers for use in a request.
*/

/*EXPECT
ok - metadata is an array
ok - metadata has two elements
ok - X-NewRelic-ID is valid
ok - X-NewRelic-Transaction is valid
*/

require_once(realpath(dirname( __FILE__ )).'/../../../include/tap.php');

$metadata = newrelic_get_request_metadata();

tap_assert(is_array($metadata), 'metadata is an array');
tap_equal(2, count($metadata), 'metadata has two elements');
tap_equal(1, preg_match('#^[a-zA-Z0-9\=\+/]{16}$#', $metadata['X-NewRelic-ID']), 'X-NewRelic-ID is valid');
tap_equal(1, preg_match('#^[a-zA-Z0-9\=\+/]{76}$#', $metadata['X-NewRelic-Transaction']), 'X-NewRelic-Transaction is valid');
