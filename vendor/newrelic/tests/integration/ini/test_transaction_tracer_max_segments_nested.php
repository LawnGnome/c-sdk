<?php

/*DESCRIPTION
When the newrelic.transaction_tracer.max_segments is set to a non-zero value,
the agent limits the number of custom segments created, even in a
nested scenario.
*/

/*INI
newrelic.transaction_tracer.max_segments=10
newrelic.transaction_tracer.threshold=0
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/great_grandmother"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/great_grandmother",
     "scope":"OtherTransaction/php__FILE__" },          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/grandmother"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/grandmother",
     "scope":"OtherTransaction/php__FILE__" },          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function"},                     [8, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function",
     "scope":"OtherTransaction/php__FILE__" },          [8, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},   [3, "??", "??", "??", "??", "??"]]
  ]
]
*/

function my_function() {
}
function grandmother(){
    for ($i = 0; $i < 1000; $i++) {
        my_function();
    }
}
function great_grandmother(){
    grandmother();
}

newrelic_add_custom_tracer("great_grandmother");
newrelic_add_custom_tracer("grandmother");
newrelic_add_custom_tracer("my_function");

great_grandmother();
