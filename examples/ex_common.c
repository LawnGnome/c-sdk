/*!
 * ex_common.c
 *
 * @brief Common function implementations for New Relic C-Agent example code.
 *
 */
#include <stdio.h>
#include <stdlib.h>

#include "ex_common.h"

/*!
 * @brief Get the New Relic application name from environment, NR_APP_NAME.
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