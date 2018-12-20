#! /bin/bash
#
# Run this curl invocation on the customer's site to help diagnose connection issues.
#
# Uses curl in trace mode to attempt to contact New Relic's data center (aka "the collector"),
# using a bogus license key
#
# Constructs a POST request.
#
# Writes the tracing output into a timestamped file of the form
#   /var/log/newrelic/curl_trace.out.*
#
addr=collector.newrelic.com
addr=50.31.164.140  # nslookup of collector.newrelic.com

TIMESTAMP=$(date +%Y%m%dT%H-%M-%S)
NOW=$(date)
curl \
  --trace-time \
  --trace /var/log/newrelic/curl.${TIMESTAMP}.trace.out \
  ${addr}:/agent_listener/invoke_raw_method\?marshal_format=json\&license_key=0000000000000000000000000000000000000000\&protocol_version=12\&method=connect \
  --data "{\"bogus_timestamp\":\"${TIMESTAMP} ${NOW}\"}" \
  > /var/log/newrelic/curl.${TIMESTAMP}.out \
  2>&1
