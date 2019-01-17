<?php

/*DESCRIPTION
Tests that LASP functionality does not interfere with normal operatoin of
newrelic.transaction_tracer.record_sql=off (i.e. no query sent in trace)
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.transaction_tracer.record_sql = "off"
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "OtherTransaction/php__FILE__",
      "<unknown>",
      [
        [
          0, {}, {},
          [
            "?? start time", "?? end time", "ROOT", "?? root attributes",
            [
              [
                "?? start time", "?? end time", "`0", "?? node attributes",
                [
                  [
                    "?? start time", "?? end time", "`1",
                    {
                      "host": "host.name",
                      "port_path_or_id": "2222",
                      "database_name": "db"
                    },
                    []
                  ]
                ]
              ]
            ]
          ],
          {
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??"
            }
          }
        ],
        [
          "OtherTransaction/php__FILE__",
          "Datastore/statement/MySQL/table/select"
        ]
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      null
    ]
  ]
]
*/

/*EXPECT
int(42)
*/

var_dump(newrelic_record_datastore_segment(function () {
  // Make sure this function takes at least 1 microsecond to ensure that a trace
  // node is generated.
  time_nanosleep(0, 1000);
  return 42;
}, array(
  'product'       => 'mysql',
  'collection'    => 'table',
  'operation'     => 'select',
  'host'          => 'host.name',
  'portPathOrId'  => 2222,
  'databaseName'  => 'db',
  'query'         => 'SELECT * FROM table WHERE foo = 42',
)));