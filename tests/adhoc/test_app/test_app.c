#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libnewrelic.h"

int main(void) {
  int priority = 50;
  newrelic_app_t* app = 0;
  newrelic_txn_t* txn = 0;
  newrelic_config_t* config = 0;
  newrelic_segment_t* segment1 = 0;
  newrelic_segment_t* segment2 = 0;
  const char* version = newrelic_version();
  char* buffer;

//  snprintf(buffer, 25, "C-Agent Test App %s", version);
  asprintf(&buffer, "C-Agent Test App %s", version);

  /* Staging account 432507 */
  config = newrelic_new_config(buffer,
                               "07a2ad66c637a29c3982469a3fe8d1982d002c4a");
  strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
  strcpy(config->redirect_collector, "staging-collector.newrelic.com");
  strcpy(config->log_filename, "./c_agent.log");
  config->log_level = LOG_INFO;
  config->transaction_tracer.threshold = NEWRELIC_THRESHOLD_IS_OVER_DURATION;
  config->transaction_tracer.duration_us = 1;

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app(config, 10000);
  free(config);

  /* Start a web transaction */
  txn = newrelic_start_web_transaction(app, "veryImportantWebTransaction");

  /* Add attributes */
  newrelic_add_attribute_int(txn, "my_custom_int", INT_MAX);
  newrelic_add_attribute_string(txn, "my_custom_string",
                                "String String String");

  sleep(1);

  /* Record an error */
  newrelic_notice_error(txn, priority, "Meaningful error message",
                        "Error.class");

  /* Add segments */
  segment1 = newrelic_start_segment(txn, "Stuff", "Secret");
  sleep(1);
  newrelic_end_segment(txn, &segment1);

  segment2 = newrelic_start_segment(txn, "More Stuff", "Secret");
  sleep(1);
  segment1 = newrelic_start_segment(txn, "Nested Stuff", "Secret");
  sleep(1);
  newrelic_end_segment(txn, &segment1);
  sleep(1);
  newrelic_end_segment(txn, &segment2);

  /* End web transaction */
  newrelic_end_transaction(&txn);

  /* Start and end a non-web transaction */
  txn = newrelic_start_non_web_transaction(app,
                                           "veryImportantOtherTransaction");
  sleep(1);

  newrelic_end_transaction(&txn);

  newrelic_destroy_app(&app);

  free(buffer);

  return 0;
}
