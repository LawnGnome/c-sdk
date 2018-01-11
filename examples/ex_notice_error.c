#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ex_common.h"
#include "libnewrelic.h"

void record_error(newrelic_txn_t* txn){
  int priority = 50;
  newrelic_notice_error(txn, priority, "Meaningful error message",
                        "Error.class.medium");
  return;
}


int main(void) {
  newrelic_app_t* app = 0;
  newrelic_txn_t* txn = 0;
  newrelic_config_t* config = 0;

	char * app_name = get_app_name();
	if (app_name == NULL) return -1;

	char * license_key = get_license_key();
	if (license_key == NULL) return -1;

  config = newrelic_new_config(app_name, license_key);

	customize_config(&config);

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app(config, 10000);
  free(config);

  /* Start a web transaction */
  txn = newrelic_start_web_transaction(app, "ExampleWebTransaction");

  newrelic_add_attribute_int(txn, "Custom_int", INT_MAX);

  sleep(5);

  /* Record an error.
   * Note the nested call to newrelic_notice_error() so that something
   * interesting appears in the backtrace.
   */
  record_error(txn);

  sleep(5);

  /* End web transaction */
  newrelic_end_transaction(&txn);

  newrelic_destroy_app(&app);

  return 0;
}
