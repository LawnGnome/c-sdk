#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "libnewrelic.h"

int main(void) {
  newrelic_app_t* app = 0;
  newrelic_txn_t* txn = 0;
  newrelic_config_t* config = 0;
  newrelic_external_segment_params_t params = {
    .procedure = "GET",
    .uri       = "https://httpbin.org/delay/1",
  };
  newrelic_external_segment_t* segment = 0;

  char* app_name = get_app_name();
  if (app_name == NULL)
    return -1;

  char* license_key = get_license_key();
  if (license_key == NULL)
    return -1;

  config = newrelic_new_config(app_name, license_key);

  customize_config(&config);

  /* Change the transaction tracer threshold to ensure a trace is generated */
  config->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  config->transaction_tracer.duration_us = 1;

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app(config, 10000);
  free(config);

  /* Start a web transaction */
  txn = newrelic_start_web_transaction(app, "ExampleWebTransaction");

  /* Fake a web request */
  segment = newrelic_start_external_segment(txn, &params);
  sleep(1);
  newrelic_end_external_segment(txn, &segment);

  /* End web transaction */
  newrelic_end_transaction(&txn);

  newrelic_destroy_app(&app);

  return 0;
}
