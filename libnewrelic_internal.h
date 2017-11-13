#ifndef LIBNEWRELIC_INTERNAL_H
#define LIBNEWRELIC_INTERNAL_H

#include "php_agent/axiom/nr_txn.h"

typedef struct _nr_app_and_info_t {
  nrapp_t* app;
  nr_app_info_t* app_info;
  newrelic_config_t* config;
  nrapplist_t* context;
} nr_app_and_info_t;

nrtxnopt_t* newrelic_get_default_options(void);
nrapplist_t* newrelic_init(const char* daemon_socket);
nr_status_t newrelic_connect_app(newrelic_app_t* app,
                                 nrapplist_t* context,
                                 unsigned short timeout_ms);
newrelic_txn_t* newrelic_start_transaction(newrelic_app_t* app,
                                           const char* name,
                                           bool is_web_transaction);
bool newrelic_add_attribute(newrelic_txn_t* transaction,
                            const char* key,
                            nrobj_t* obj);

#endif /* LIBNEWRELIC_INTERNAL_H */