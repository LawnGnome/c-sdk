/*!
 * ex_common.c
 *
 * @brief Common function implementations for New Relic C-Agent example code.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ex_common.h"

/*
 * @brief Customize an agent configuration
 *
 * @param [in] config_ptr The address of an agent configuration created using
 * newrelic_create_config().
 *
 * @return false if config_ptr or *config_ptr are NULL; true otherwise.
 */
bool customize_config(newrelic_config_t** config_ptr) {
  if (config_ptr != NULL && *config_ptr != NULL) {
    newrelic_config_t* config = *config_ptr;

    strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
    strcpy(config->log_filename, "./c_agent.log");
    config->log_level = LOG_INFO;

    char* collector = getenv("NR_COLLECTOR");

    if (collector != NULL) {
      strcpy(config->redirect_collector, collector);
    } else {
      printf("Using default agent configuration for collector...\n");
    }

    return true;
  }
  return false;
}

/*!
 * @brief Get the New Relic application name from environment, NR_APP_NAME.
 *
 * @return A pointer to the environment variable NR_APP_NAME; NULL if it is not
 * defined.
 */
char* get_app_name(void) {
  char* app_name = getenv("NR_APP_NAME");

  if (app_name == NULL) {
    printf(ENV_NOTICE);
    printf(
        "\nEnvironment variable NR_APP_NAME must be set to a meaningful "
        "application name.\n");
    return NULL;
  }
}

/*!
 * @brief Get the New Relic license key from environment, NR_LICENSE.
 *
 * @return A pointer to the environment variable NR_LICENSE; NULL if it is not
 * defined.
 */
char* get_license_key(void) {
  char* license_key = getenv("NR_LICENSE");

  if (license_key == NULL) {
    printf(ENV_NOTICE);
    printf(
        "\nEnvironment variable NR_LICENSE must be set to a valid New Relic "
        "license key.\n");
    return NULL;
  }
}
