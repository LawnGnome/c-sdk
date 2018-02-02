/*!
 * ex_common.c
 *
 * @brief Common function implementations for New Relic C-Agent example code.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * @brief Customize an agent configuration
 *
 * @param [in] config_ptr The address of an agent configuration created using
 * newrelic_create_config().
 *
 * @return false if config_ptr or *config_ptr are NULL; true otherwise.
 */
bool customize_config(newrelic_config_t** config_ptr) {
  if (NULL != config_ptr && NULL != *config_ptr) {
    newrelic_config_t* config = *config_ptr;

    strcpy(config->daemon_socket, "/tmp/.newrelic.sock");
    strcpy(config->log_filename, "./c_agent.log");
    config->log_level = LOG_INFO;

    char* collector = getenv("NEW_RELIC_HOST");

    if (NULL != collector) {
      strcpy(config->redirect_collector, collector);
    } else {
      printf("Using default agent configuration for collector...\n");
    }

    return true;
  }
  return false;
}

/*!
 * @brief Get the New Relic application name from environment, NEW_RELIC_APP_NAME.
 *
 * @return A pointer to the environment variable NEW_RELIC_APP_NAME; NULL if it
 * is not defined.
 */
char* get_app_name(void) {
  char* app_name = getenv("NEW_RELIC_APP_NAME");

  if (NULL == app_name) {
    printf(ENV_NOTICE);
    printf(
        "\nEnvironment variable NEW_RELIC_APP_NAME must be set to a meaningful "
        "application name.\n");
    return NULL;
  }
}

/*!
 * @brief Get the New Relic license key from environment, NEW_RELIC_LICENSE_KEY.
 *
 * @return A pointer to the environment variable NEW_RELIC_LICENSE_KEY; NULL if
 * it is not defined.
 */
char* get_license_key(void) {
  char* license_key = getenv("NEW_RELIC_LICENSE_KEY");

  if (NULL == license_key) {
    printf(ENV_NOTICE);
    printf(
        "\nEnvironment variable NEW_RELIC_LICENSE_KEY must be set to a valid New "
        "Relic license key.\n");
    return NULL;
  }
}
