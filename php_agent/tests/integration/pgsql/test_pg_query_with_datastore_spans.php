pg = NULL;
<?php

/*DESCRIPTION
The agent should report database metrics for SQLite3.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
    "events_seen": 5
  },
  [
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "nr.entryPoint": true
      },
      {},
      {}
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": ??,
        "name": "Datastore\/operation\/SQLite\/other",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "datastore",
        "parentId": "??",
        "span.kind": "client",
        "component": "SQLite"
      },
      {},
      {
        "db.statement": "CREATE TABLE test (id INT, desc VARCHAR(?));",
        "db.instance": "",
        "peer.hostname": "",
        "peer.address": ""
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Datastore\/statement\/SQLite\/test\/insert",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "datastore",
        "parentId": "??",
        "span.kind": "client",
        "component": "SQLite"
      },
      {},
      {
        "db.statement": "INSERT INTO test VALUES (?, ?);",
        "db.instance": "",
        "peer.hostname": "",
        "peer.address": ""
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Datastore\/statement\/SQLite\/test\/select",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "datastore",
        "parentId": "??",
        "span.kind": "client",
        "component": "SQLite"
      },
      {},
      {
        "db.statement": "SELECT * FROM test;",
        "db.instance": "",
        "peer.hostname": "",
        "peer.address": ""
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": 1.786400,
        "name": "Datastore\/operation\/SQLite\/other",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "datastore",
        "parentId": "??",
        "span.kind": "client",
        "component": "SQLite"
      },
      {},
      {
        "db.statement": "DROP TABLE test;",
        "db.instance": "",
        "peer.hostname": "",
        "peer.address": ""
      }
    ]
  ]
]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/SQLite/all"},                   [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},              [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                          [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other"},       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other",
      "scope":"OtherTransaction/php__FILE__"},          [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/select"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert",
      "scope":"OtherTransaction/php__FILE__"},          [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

function test_sqlite3() {
  $conn = new SQLite3(":memory:");
  $conn->exec("CREATE TABLE test (id INT, desc VARCHAR(10));");

  $conn->exec("INSERT INTO test VALUES (1, 'one');");

  $result = $conn->query("SELECT * FROM test;");

  while ($row = $result->fetchArray(SQLITE3_ASSOC)) {
    print_r($row);
  }

  $conn->exec("DROP TABLE test;");
}

test_sqlite3();