/*!
 * ex_common.h
 *
 * @brief Common constants and function declarations for New Relic C-Agent
 * example code.
 */

/* Common constant values */
#define ENV_NOTICE                                                          \
  ("This example program depends on environment variables NR_APP_NAME and " \
   "NR_LICENSE.")

/* Common function declarations */
char* get_app_name(void);
char* get_license_key(void);
