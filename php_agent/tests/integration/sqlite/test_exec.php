<?php

/*DESCRIPTION
The agent should report Datastore metrics for sqlite_exec(), and it should
gracefully handle the first two parameters occuring in either order.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                          [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                   [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},              [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other"},       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other",
      "scope":"OtherTransaction/php__FILE__"},          [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert",
      "scope":"OtherTransaction/php__FILE__"},          [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

function test_sqlite() {
  $conn = sqlite_open(":memory:");
  sqlite_exec($conn, "CREATE TABLE test (id INT, desc VARCHAR(10));");

  /* Now try putting the resource second. */
  sqlite_exec("INSERT INTO test VALUES (1, 'one');", $conn);
  sqlite_exec("INSERT INTO test VALUES (2, 'two');", $conn);
  sqlite_exec("INSERT INTO test VALUES (3, 'three');", $conn);

  sqlite_exec($conn, "DROP TABLE test;");
  sqlite_close($conn);
}

test_sqlite();
