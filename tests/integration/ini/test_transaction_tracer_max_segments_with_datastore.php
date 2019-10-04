<?php

/*DESCRIPTION
When the newrelic.transaction_tracer.max_segments is set to a non-zero value,
the agent limits the number of custom segments created, but does not limit
subsequent datastore segment creation.
*/

/*INI
newrelic.transaction_tracer.max_segments=1
newrelic.transaction_tracer.threshold=0
newrelic.distributed_tracing_enabled=0
*/



/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/my_function"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/MySQL/host.name/2222"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/table/select"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/record_datastore_segment"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function",
     "scope":"OtherTransaction/php__FILE__" },               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/table/select",
      "scope": "OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

function my_function(){

}

newrelic_add_custom_tracer("my_function");

my_function();
my_function();
my_function();

newrelic_record_datastore_segment(function () {
    return 42;
}, array(
    'product'       => 'mysql',
    'collection'    => 'table',
    'operation'     => 'select',
    'host'          => 'host.name',
    'portPathOrId'  => 2222,
    'databaseName'  => 'db',
    'query'         => 'SELECT * FROM table WHERE foo = 42',
));
