/*
 * This file contains the agent's view of the appinfo command:
 * it is used by the agent to query the daemon about the status of applications.
 */
#include "nr_axiom.h"

#include <errno.h>
#include <stddef.h>

#include "nr_agent.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_rules.h"
#include "util_buffer.h"
#include "util_errno.h"
#include "util_flatbuffers.h"
#include "util_labels.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_network.h"
#include "util_reply.h"

/*
 * TODO(willhf+msl) This (default) timeout value is pretty arbitrary.  The
 * appinfo command happens at the beginning of a request, and therefore if it
 * is too large, the delay could be noticable.  Optimally, we would fully
 * understand the effects of changing this value by testing under load.
 */
uint64_t nr_cmd_appinfo_timeout_us = 100 * NR_TIME_DIVISOR_MS;

/*
 * Send the labels to the daemon in the format expected by the
 * collector in the connect command.
 */
static uint32_t nr_appinfo_prepend_labels(const nr_app_info_t* info,
                                          nr_flatbuffer_t* fb) {
  char* json;
  nrobj_t* labels;
  uint32_t offset;

  if ((NULL == info) || (NULL == info->labels)) {
    return 0;
  }

  labels = nr_labels_connector_format(info->labels);
  json = nro_to_json(labels);
  offset = nr_flatbuffers_prepend_string(fb, json);
  nro_delete(labels);
  nr_free(json);

  return offset;
}

static uint32_t nr_appinfo_prepend_settings(const nr_app_info_t* info,
                                            nr_flatbuffer_t* fb) {
  char* json;
  uint32_t offset;

  if ((NULL == info) || (NULL == info->settings)) {
    return 0;
  }

  json = nro_to_json(info->settings);
  offset = nr_flatbuffers_prepend_string(fb, json);
  nr_free(json);

  return offset;
}

static nr_status_t convert_appenv(const char* key,
                                  const nrobj_t* val,
                                  void* ptr) {
  nrobj_t* envarray = (nrobj_t*)ptr;
  nrobj_t* entry = nro_new_array();

  nro_set_array_string(entry, 1, key);
  nro_set_array(entry, 2, val);

  nro_set_array(envarray, 0, entry);
  nro_delete(entry);

  return NR_SUCCESS;
}

/*
 * Send the environment to the daemon in the format expected by the
 * collector in the connect command.
 */
static uint32_t nr_appinfo_prepend_env(const nr_app_info_t* info,
                                       nr_flatbuffer_t* fb) {
  char* json;
  nrobj_t* env;
  uint32_t offset;

  if ((NULL == info) || (NULL == info->environment)) {
    return 0;
  }

  env = nro_new_array();
  nro_iteratehash(info->environment, convert_appenv, env);
  json = nro_to_json(env);
  offset = nr_flatbuffers_prepend_string(fb, json);
  nro_delete(env);
  nr_free(json);

  return offset;
}

nr_flatbuffer_t* nr_appinfo_create_query(const char* agent_run_id,
                                         const nr_app_info_t* info) {
  nr_flatbuffer_t* fb;
  uint32_t display_host;
  uint32_t labels;
  uint32_t settings;
  uint32_t env;
  uint32_t license;
  uint32_t appname;
  uint32_t agent_lang;
  uint32_t agent_version;
  uint32_t collector;
  uint32_t appinfo;
  uint32_t agent_run_id_offset;
  uint32_t message;

  fb = nr_flatbuffers_create(0);

  display_host = nr_flatbuffers_prepend_string(fb, info->host_display_name);
  labels = nr_appinfo_prepend_labels(info, fb);
  settings = nr_appinfo_prepend_settings(info, fb);
  env = nr_appinfo_prepend_env(info, fb);
  collector = nr_flatbuffers_prepend_string(fb, info->redirect_collector);
  agent_version = nr_flatbuffers_prepend_string(fb, info->version);
  agent_lang = nr_flatbuffers_prepend_string(fb, info->lang);
  appname = nr_flatbuffers_prepend_string(fb, info->appname);
  license = nr_flatbuffers_prepend_string(fb, info->license);

  nr_flatbuffers_object_begin(fb, APP_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_DISPLAY_HOST, display_host, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_LABELS, labels, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_SETTINGS, settings, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_ENVIRONMENT, env, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_REDIRECT_COLLECTOR,
                                        collector, 0);
  nr_flatbuffers_object_prepend_bool(fb, APP_FIELD_HIGH_SECURITY,
                                     info->high_security, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_AGENT_VERSION,
                                        agent_version, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_AGENT_LANGUAGE,
                                        agent_lang, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_APPNAME, appname, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_LICENSE, license, 0);
  appinfo = nr_flatbuffers_object_end(fb);

  if (agent_run_id && *agent_run_id) {
    agent_run_id_offset = nr_flatbuffers_prepend_string(fb, agent_run_id);
  } else {
    agent_run_id_offset = 0;
  }

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, appinfo, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_APP, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id_offset, 0);
  message = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_finish(fb, message);

  return fb;
}

int nr_command_is_flatbuffer_invalid(nr_flatbuffer_t* msg, size_t msglen) {
  size_t offset = nr_flatbuffers_read_uoffset(nr_flatbuffers_data(msg), 0);

  if (msglen - MIN_FLATBUFFER_SIZE <= offset) {
    nrl_verbosedebug(NRL_DAEMON, "offset is too large, len=%zu", offset);
    return 1;
  }

  return 0;
}

nr_status_t nr_cmd_appinfo_process_reply(const uint8_t* data,
                                         int len,
                                         nrapp_t* app) {
  nr_flatbuffers_table_t msg;
  nr_flatbuffers_table_t reply;
  int data_type;
  int status;
  int reply_len;
  const char* reply_json;

  if ((NULL == data) || (0 == len)) {
    return NR_FAILURE;
  }
  if (NULL == app) {
    return NR_FAILURE;
  }

  nr_flatbuffers_table_init_root(&msg, data, len);

  data_type = nr_flatbuffers_table_read_u8(&msg, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  if (MESSAGE_BODY_APP_REPLY != data_type) {
    nrl_error(NRL_ACCT, "unexpected message type, data_type=%d", data_type);
    return NR_FAILURE;
  }

  if (0 == nr_flatbuffers_table_read_union(&reply, &msg, MESSAGE_FIELD_DATA)) {
    nrl_error(NRL_ACCT, "APPINFO reply missing a body");
    return NR_FAILURE;
  }

  status = nr_flatbuffers_table_read_i8(&reply, APP_REPLY_FIELD_STATUS,
                                        APP_STATUS_UNKNOWN);

  switch (status) {
    case APP_STATUS_UNKNOWN:
      app->state = NR_APP_UNKNOWN;
      nrl_debug(NRL_ACCT, "APPINFO reply unknown app=" NRP_FMT,
                NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    case APP_STATUS_DISCONNECTED:
      app->state = NR_APP_INVALID;
      nrl_info(NRL_ACCT, "APPINFO reply disconnected app=" NRP_FMT,
               NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    case APP_STATUS_INVALID_LICENSE:
      app->state = NR_APP_INVALID;
      nrl_error(NRL_ACCT,
                "APPINFO reply invalid license app=" NRP_FMT
                " please check your license "
                "key and restart your web server.",
                NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    case APP_STATUS_CONNECTED:
      /* Don't return here, instead break and continue below. */
      nrl_debug(NRL_ACCT, "APPINFO reply connected");
      break;
    case APP_STATUS_STILL_VALID:
      app->state = NR_APP_OK;
      nrl_debug(NRL_ACCT, "APPINFO reply agent run id still valid app='%.*s'",
                NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    default:
      nrl_error(NRL_ACCT, "APPINFO reply has unknown status status=%d", status);
      return NR_FAILURE;
  }

  /*
   * Connected: Full app reply
   */
  reply_len = (int)nr_flatbuffers_table_read_vector_len(
      &reply, APP_REPLY_FIELD_CONNECT_REPLY);
  reply_json = (const char*)nr_flatbuffers_table_read_bytes(
      &reply, APP_REPLY_FIELD_CONNECT_REPLY);

  nro_delete(app->connect_reply);
  app->connect_reply = nro_create_from_json_unterminated(reply_json, reply_len);

  if (NULL == app->connect_reply) {
    nrl_error(NRL_ACCT, "APPINFO reply bad connect reply: len=%d json=%p",
              reply_len, reply_json);
    return NR_FAILURE;
  }

  nr_free(app->agent_run_id);
  app->agent_run_id = nr_strdup(
      nro_get_hash_string(app->connect_reply, "agent_run_id", NULL));
  app->state = NR_APP_OK;
  nr_rules_destroy(&app->url_rules);
  app->url_rules = nr_rules_create_from_obj(
      nro_get_hash_array(app->connect_reply, "url_rules", 0));
  nr_rules_destroy(&app->txn_rules);
  app->txn_rules = nr_rules_create_from_obj(
      nro_get_hash_array(app->connect_reply, "transaction_name_rules", 0));
  nr_segment_terms_destroy(&app->segment_terms);
  app->segment_terms = nr_segment_terms_create_from_obj(
      nro_get_hash_array(app->connect_reply, "transaction_segment_terms", 0));

  nrl_debug(NRL_ACCT, "APPINFO reply full app='%.*s' agent_run_id=%s",
            NRP_APPNAME(app->info.appname), app->agent_run_id);

  return NR_SUCCESS;
}

/* Hook for stubbing APPINFO messages during testing. */
nr_status_t (*nr_cmd_appinfo_hook)(int daemon_fd, nrapp_t* app) = NULL;

nr_status_t nr_cmd_appinfo_tx(int daemon_fd, nrapp_t* app) {
  nr_flatbuffer_t* query;
  nrbuf_t* buf = NULL;
  nrtime_t deadline;
  nr_status_t st;
  size_t querylen;

  if (nr_cmd_appinfo_hook) {
    return nr_cmd_appinfo_hook(daemon_fd, app);
  }

  if (NULL == app) {
    /* TODO(msl): Why success and not failure? */
    return NR_SUCCESS;
  }
  if (daemon_fd < 0) {
    /* TODO(msl): Why success and not failure? */
    return NR_SUCCESS;
  }

  app->state = NR_APP_UNKNOWN;
  nrl_verbosedebug(NRL_DAEMON, "querying app=" NRP_FMT " from parent=%d",
                   NRP_APPNAME(app->info.appname), daemon_fd);

  query = nr_appinfo_create_query(app->agent_run_id, &app->info);
  querylen = nr_flatbuffers_len(query);

  nrl_verbosedebug(NRL_DAEMON, "sending appinfo message, len=%zu", querylen);

  if (nr_command_is_flatbuffer_invalid(query, querylen)) {
    nr_flatbuffers_destroy(&query);
    /* TODO(msl): Why success and not failure? */
    return NR_SUCCESS;
  }

  deadline = nr_get_time() + nr_cmd_appinfo_timeout_us;

  nr_agent_lock_daemon_mutex();
  {
    st = nr_write_message(daemon_fd, nr_flatbuffers_data(query), querylen,
                          deadline);
    if (NR_SUCCESS == st) {
      buf = nr_network_receive(daemon_fd, deadline);
    }
  }
  nr_agent_unlock_daemon_mutex();

  nr_flatbuffers_destroy(&query);
  st = nr_cmd_appinfo_process_reply((const uint8_t*)nr_buffer_cptr(buf),
                                    nr_buffer_len(buf), app);
  nr_buffer_destroy(&buf);

  if (NR_SUCCESS != st) {
    app->state = NR_APP_UNKNOWN;
    nrl_error(NRL_DAEMON, "APPINFO failure: len=%zu errno=%s", querylen,
              nr_errno(errno));
    nr_agent_close_daemon_connection();
  }

  return st;
}
