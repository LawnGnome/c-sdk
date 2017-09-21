#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>

#include "libnewrelic.h"

int main (void) {
  newrelic_app_t *app = 0;
  newrelic_txn_t *txn = 0;
  newrelic_config_t *config = 0;

  /* Staging account 432507 */
  config = newrelic_new_config ("C-Agent Test App", "07a2ad66c637a29c3982469a3fe8d1982d002c4a");
  strcpy (config->daemon_socket, "/tmp/.newrelic.sock");
  strcpy (config->redirect_collector, "staging-collector.newrelic.com");
  strcpy (config->log_filename, "./c_agent.log");
  config->log_level = LOG_INFO;

  /* Wait up to 10 seconds for the agent to connect to the daemon */
  app = newrelic_create_app (config, 10000);
  free (config);

  /* Start a web transaction */
  txn = newrelic_start_web_transaction (app, "veryImportantTransaction");

  /* Add attributes and check/alert if they fail */
  if (!newrelic_transaction_add_attribute_int (txn, "CustomInt", INT_MAX))
    printf ("Failed to add custom Int\n");
  if (!newrelic_transaction_add_attribute_long (txn, "CustomLong", LONG_MAX))
    printf ("Failed to add custom Long\n");

  sleep (1);


  /* End web transaction */
  newrelic_end_transaction (&txn);

  /* Start a non-web transaction */
  txn = newrelic_start_non_web_transaction (app, "veryImportantTransactionPart2");

  /* Add attributes and check/alert if they fail */
  if (!newrelic_transaction_add_attribute_double (txn, "CustomDbl", DBL_MAX))
    printf ("Failed to add custom Double\n");
  if (!newrelic_transaction_add_attribute_string (txn, "CustomStr", "String String String"))
    printf ("Failed to add custom String\n");

  sleep (1);

  /* End non-web transaction */
  newrelic_end_transaction (&txn);

  newrelic_destroy_app (&app);

  return 0;
}
