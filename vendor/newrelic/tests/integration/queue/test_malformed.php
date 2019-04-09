<?php

/*DESCRIPTION
The agent should obey the queue time header.
*/

/*HEADERS
X_REQUEST_START=abc
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Apdex"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Apdex/Uri__FILE__"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

echo "hello world";