<?php

/*DESCRIPTION
Tests newrelic_record_custom_event().
*/

/*INI
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
[
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "alpha",
        "timestamp": "??"
      },
      {
        "beta": 1
      },
      {
      }
    ]
  ]
]
*/

newrelic_record_custom_event("alpha", array("beta"=>1));
