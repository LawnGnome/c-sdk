#include "libnewrelic.h"
#include "config.h"

#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

newrelic_config_t* newrelic_new_config(const char* app_name,
                                       const char* license_key) {
  newrelic_config_t* config;

  if (NULL == app_name) {
    nrl_error(NRL_INSTRUMENT, "app name is required");
    return NULL;
  }

  if (NR_LICENSE_SIZE != nr_strlen(license_key)) {
    nrl_error(NRL_INSTRUMENT, "invalid license key format");
    return NULL;
  }

  config = (newrelic_config_t*)nr_zalloc(sizeof(newrelic_config_t));

  nr_strxcpy(config->app_name, app_name, nr_strlen(app_name));
  nr_strxcpy(config->license_key, license_key, nr_strlen(license_key));

  return config;
}

nrtxnopt_t* newrelic_get_default_options(void) {
  nrtxnopt_t* opt = nr_zalloc(sizeof(nrtxnopt_t));

  opt->analytics_events_enabled = true;
  opt->custom_events_enabled = false;
  opt->synthetics_enabled = false;
  opt->instance_reporting_enabled = false;
  opt->database_name_reporting_enabled = false;
  opt->err_enabled = true;
  opt->request_params_enabled = false;
  opt->autorum_enabled = false;
  opt->error_events_enabled = true;
  opt->tt_enabled = false;
  opt->ep_enabled = false;
  opt->tt_recordsql = false;
  opt->tt_slowsql = false;
  opt->apdex_t = 0;
  opt->tt_threshold = 0;
  opt->ep_threshold = 0;
  opt->ss_threshold = 0;
  opt->cross_process_enabled = false;
  opt->tt_is_apdex_f = 0;

  return opt;
}
