#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>

#include "libnewrelic.h"

int main (void) {
  int priority = 50;
  newrelic_app_t *app = 0;
  newrelic_txn_t *txn = 0;
  newrelic_config_t *config = 0;
  newrelic_segment_t *segment1, *segment2;

  config = newrelic_new_config("Your Application Name", "<LICENSE_KEY_HERE>");
  strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
  strcpy(config->log_filename, "./c_agent.log");
  config->log_level = LOG_INFO;

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
  sleep (1);
  newrelic_end_segment (txn, &segment1);

  segment2 = newrelic_start_segment(txn, "More Stuff", "Secret");
  sleep (2);
  newrelic_end_segment (txn, &segment2);

  /* End web transaction */
  newrelic_end_transaction(&txn);

  /* Start and end a non-web transaction */
  txn =
      newrelic_start_non_web_transaction(app, "veryImportantOtherTransaction");
  sleep(1);

  newrelic_end_transaction(&txn);

  newrelic_destroy_app(&app);

  return 0;
}
