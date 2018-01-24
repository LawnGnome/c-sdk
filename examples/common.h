/*!
 * @file common.h
 *
 * @brief Common constants and function declarations for New Relic C-Agent
 * example code.
 */
#include "libnewrelic.h"

/* Common constant values */
#define ENV_NOTICE                                                          \
  ("This example program depends on environment variables NEW_RELIC_APP_NAME " \
   "and NEW_RELIC_LICENSE_KEY.")

/* Common function declarations */
bool customize_config(newrelic_config_t** config_ptr);
char* get_app_name(void);
char* get_license_key(void);
