<?php

/*DESCRIPTION
Tests how the agent converts custom parameter values to strings.
*/

/*EXPECT
ok - should reject zero args
ok - should reject one arg
ok - should reject more than two args
ok - should reject infinity
ok - should reject NaN
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??"
      },
      {
      },
      {
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

function test_add_custom_parameter() {
  $result = @newrelic_add_custom_parameter();
  tap_refute($result, "should reject zero args");

  $result = @newrelic_add_custom_parameter("key1");
  tap_refute($result, "should reject one arg");

  $result = @newrelic_add_custom_parameter("key2", "value", "bad");
  tap_refute($result, "should reject more than two args");

  $result = @newrelic_add_custom_parameter("key3", INF);
  tap_refute($result, "should reject infinity");

  $result = @newrelic_add_custom_parameter("key4", NAN);
  tap_refute($result, "should reject NaN");
}

test_add_custom_parameter();
