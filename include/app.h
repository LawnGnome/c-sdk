#ifndef LIBNEWRELIC_APP_H
#define LIBNEWRELIC_APP_H

#include "nr_app.h"

typedef struct _nr_app_and_info_t {
  nrapp_t* app;
  nr_app_info_t* app_info;
  newrelic_config_t* config;
  nrapplist_t* context;
} nr_app_and_info_t;

nrapplist_t* newrelic_init(const char* daemon_socket);
nr_status_t newrelic_connect_app(newrelic_app_t* app,
                                 nrapplist_t* context,
                                 unsigned short timeout_ms);

#endif /* LIBNEWRELIC_APP_H */
