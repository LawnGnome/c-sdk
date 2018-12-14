<?php

/*DESCRIPTION
The agent should create a databaseDuration attribute when database queries
occur.
*/

/*SKIPIF
<?php require(dirname(__FILE__).'/../pdo/skipif_mysql.inc');
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
        "totalTime": "??",
        "databaseDuration": "??"
      },
      {
      },
      {
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/../pdo/pdo.inc');

function test_slow_sql() {
  global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

  $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD);
  $result = $conn->query('select * from tables limit 1;');
}

test_slow_sql();
