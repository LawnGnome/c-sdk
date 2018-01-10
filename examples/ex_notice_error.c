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
	char * license_key = getenv("NR_LICENSE");



 	if(license_key == NULL) {
 		printf("This example program depends on environment variables NR_APP_NAME and NR_LICENSE.\n")
 		printf("Environment variable NR_LICENSE must be set to a valid New Relic license key.\n");
 		return -1;
 	}

  config = newrelic_new_config(app_name, license_key);

  strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
  strcpy(config->redirect_collector, "staging-collector.newrelic.com");
  strcpy(config->log_filename, "./c_agent.log");
  config->log_level = LOG_INFO;

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app(config, 10000);
  free(config);

  /* Start a web transaction */
  txn = newrelic_start_web_transaction(app, "ExampleWebTransaction");

  sleep(1);

  /* Record an error */
  newrelic_notice_error(txn, priority, "Meaningful error message",
                        "Error.class");

  /* End web transaction */
  newrelic_end_transaction(&txn);


  newrelic_destroy_app(&app);

  return 0;
}
