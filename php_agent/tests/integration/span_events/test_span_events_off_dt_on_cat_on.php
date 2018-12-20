<?php
/*DESCRIPTION
Span events should not be sent when distributed tracing is
enabled, span events are disabled and cat is enabled.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled=0
newrelic.cross_application_tracer.enabled = true
*/

/*EXPECT_SPAN_EVENTS
null
*/

/*EXPECT
Hello
*/

newrelic_add_custom_tracer('main');
function main()
{
  echo 'Hello';
}
main();

