<?php

/*DESCRIPTION
The agent should gracefully handle bad input being passed to PDO::exec().
*/

/*SKIPIF
<?php require('skipif_sqlite.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT_REGEX
^\s*(PHP )?Warning:\s*PDO::exec\(\) expects exactly 1 parameter, 0 given in .*? on line [0-9]+\s*$
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "PDO::exec() expects exactly 1 parameter, 0 given",
      "E_WARNING",
      {
        "stack_trace": [
          " in PDO::exec called at __FILE__ (??)",
          " in test_pdo called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');

function test_pdo() {
  $conn = new PDO('sqlite::memory:');
  $conn->exec();
}

test_pdo();