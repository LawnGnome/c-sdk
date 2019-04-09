<?php

/*DESCRIPTION
The agent should report Database metrics for pg_query.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.stack_trace_threshold = 0
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = raw
*/

/*EXPECT
ok - pg_query successful
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/operation/Postgres/select"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/pg_user/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/pg_user/select",
      "scope":"OtherTransaction/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/all"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/allOther"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},       [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT 1 FROM pg_user",
      "Datastore/statement/Postgres/pg_user/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in pg_query called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

$pg = pg_connect($PG_CONNECTION);
$result = pg_query("SELECT 1 FROM pg_user");
$row = pg_fetch_row($result);
tap_assert($row[0] == 1, "pg_query successful");
pg_close($pg);
$pg = NULL;