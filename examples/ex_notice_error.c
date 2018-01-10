#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libnewrelic.h"
#include "ex_common.h"

int main(void) {
  int priority = 50;
  newrelic_app_t* app = 0;
  newrelic_txn_t* txn = 0;
  newrelic_config_t* config = 0;

	char * app_name = get_app_name();
	if (app_name == NULL) return -1;

	char * license_key = get_license_key();
	if (license_key == NULL) return -1;

  config = newrelic_new_config(app_name, license_key);

  strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
  strcpy(config->redirect_collector, "staging-collector.newrelic.com");
  strcpy(config->log_filename, "./c_agent.log");
  config->log_level = LOG_VERBOSE;

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app(config, 10000);
  free(config);

#ifdef butt
  /* Start a web transaction */
  txn = newrelic_start_web_transaction(app, "ExampleWebTransaction");

  sleep(5);

  /* Add an attribute */
  newrelic_add_attribute_int(txn, "custom_int", INT_MAX);

  /* Record an error */
  newrelic_notice_error(txn, priority, "Meaningful error message",
                        "Error.class.low");

  sleep(5);

  /* End web transaction */
  newrelic_end_transaction(&txn);
#endif

 /* Start and end a non-web transaction */
  txn =
      newrelic_start_non_web_transaction(app, "veryImportantOtherTransaction");
  sleep(1);

  newrelic_end_transaction(&txn);


  newrelic_destroy_app(&app);

  return 0;
}
