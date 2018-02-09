/*!
 * @file app.h
 *
 * @brief Type definitions, constants, and function declarations necessary to
 * support application initialization and connection for the C agent.
 */
#ifndef LIBNEWRELIC_APP_H
#define LIBNEWRELIC_APP_H

#include "nr_app.h"

/*! @brief The internal type used to represent application information. */
typedef struct _nr_app_and_info_t {
  /* A variety of application state. */
  nrapp_t* app;

  /*! The New Relic APPINFO; includes local configuration sent up to New Relic
   * upon connection. */
  nr_app_info_t* app_info;

  /*! C Agent configuration options. */
  newrelic_config_t* config;

  /*! The list of applications. */
  nrapplist_t* context;
} nr_app_and_info_t;

/*!
 * @brief Initialize a New Relic application by trying to connect to the daemon.
 *
 * @param [in] daemon_socket A string representing the underlying communication
 * mechanism for the agent daemon, e.g. "/tmp/.newrelic.sock".
 *
 * @return A list of applications.
 */
nrapplist_t* newrelic_init(const char* daemon_socket);

/*!
 * @brief Connect application to New Relic.
 *
 * @param [in] app Application information.
 * @param [in] context A list of applications.
 * @param [in] timeout_ms The amount of time, in milliseconds, for the
 * application to wait on the daemon to connect to New Relic.
 *
 * @return NR_SUCCESS if successful; NR_FAILURE if any parameters are invalid or
 * the daemon was unable to connect.  A message at level LOG_ERROR will be
 * logged if connection fails.
 *
 */
nr_status_t newrelic_connect_app(newrelic_app_t* app,
                                 nrapplist_t* context,
                                 unsigned short timeout_ms);

#endif /* LIBNEWRELIC_APP_H */
