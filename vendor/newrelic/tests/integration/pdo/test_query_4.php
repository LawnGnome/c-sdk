<?php

/*DESCRIPTION
The agent should record database metrics for the FETCH_INTO variant of
PDO::query().
*/

/*SKIPIF
<?php require('skipif_sqlite.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - create table
ok - insert row
ok - fetch row into object
ok - drop table
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                          [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                   [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},              [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other"},       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/select"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other",
      "scope":"OtherTransaction/php__FILE__"},          [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(dirname(__FILE__).'/../../include/tap.php');

class Row {
  public $id;
  public $desc;
}

function test_pdo_query() {
  $conn = new PDO('sqlite::memory:');
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, desc VARCHAR(10));"), 'create table');
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (1, 'one');"), 'insert row');

  $expected = new Row();
  $expected->id = '1';
  $expected->desc = 'one';

  $actual = new Row();
  $conn->query('SELECT * FROM test;', PDO::FETCH_INTO, $actual)->fetch();
  tap_assert($expected == $actual, 'fetch row into object');   // FIXME: can't use ===, why?

  tap_equal(1, $conn->exec("DROP TABLE test;"), 'drop table');
}

test_pdo_query();