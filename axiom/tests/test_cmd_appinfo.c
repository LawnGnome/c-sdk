#include "nr_axiom.h"

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "nr_agent.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_rules.h"
#include "util_buffer.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_network.h"
#include "util_reply.h"
#include "util_strings.h"

#include "tlib_main.h"

#define test_pass_if_empty_vector(T, I) \
  tlib_pass_if_size_t_equal (__func__, 0, nr_flatbuffers_table_read_vector_len ((T), (I)))

static void
test_create_empty_query (void)
{
  nr_flatbuffers_table_t msg, app;
  nr_app_info_t info;
  nr_flatbuffer_t *query;
  int high_security;

  nr_memset (&info, 0, sizeof (info));
  query = nr_appinfo_create_query ("", &info);

  nr_flatbuffers_table_init_root (&msg, nr_flatbuffers_data (query),
    nr_flatbuffers_len (query));
  test_pass_if_empty_vector (&msg, MESSAGE_FIELD_AGENT_RUN_ID);

  nr_flatbuffers_table_read_union (&app, &msg, MESSAGE_FIELD_DATA);
  test_pass_if_empty_vector (&app, APP_FIELD_LICENSE);
  test_pass_if_empty_vector (&app, APP_FIELD_APPNAME);
  test_pass_if_empty_vector (&app, APP_FIELD_AGENT_LANGUAGE);
  test_pass_if_empty_vector (&app, APP_FIELD_AGENT_VERSION);
  test_pass_if_empty_vector (&app, APP_FIELD_REDIRECT_COLLECTOR);
  test_pass_if_empty_vector (&app, APP_FIELD_ENVIRONMENT);
  test_pass_if_empty_vector (&app, APP_FIELD_SETTINGS);
  test_pass_if_empty_vector (&app, APP_DISPLAY_HOST);
  test_pass_if_empty_vector (&app, APP_FIELD_LABELS);

  high_security = nr_flatbuffers_table_read_i8 (&app, APP_FIELD_HIGH_SECURITY, 42);
  tlib_pass_if_false (__func__, 0 == high_security, "high_security=%d", high_security);

  nr_flatbuffers_destroy (&query);
}

static void
test_create_query (void)
{
  nr_app_info_t info;
  nr_flatbuffers_table_t msg, app;
  nr_flatbuffer_t *query;
  int high_security;
  const char *settings_json = "[\"my_settings\"]";

  info.high_security = 1;
  info.license = nr_strdup ("my_license");
  info.settings = nro_create_from_json (settings_json);
  info.environment = nro_create_from_json ("{\"my_environment\":\"hi\"}");
  info.labels = nro_create_from_json ("{\"my_labels\":\"hello\"}");
  info.host_display_name = nr_strdup ("my_host_display_name");
  info.lang = nr_strdup ("my_lang");
  info.version = nr_strdup ("my_version");
  info.appname = nr_strdup ("my_appname");
  info.redirect_collector = nr_strdup ("my_redirect_collector");

  query = nr_appinfo_create_query ("12345", &info);

  nr_flatbuffers_table_init_root (&msg, nr_flatbuffers_data (query),
    nr_flatbuffers_len (query));

  nr_flatbuffers_table_read_union (&app, &msg, MESSAGE_FIELD_DATA);

  tlib_pass_if_str_equal (__func__, info.license,
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_LICENSE));
  tlib_pass_if_str_equal (__func__, info.appname,
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_APPNAME));
  tlib_pass_if_str_equal (__func__, info.host_display_name,
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_DISPLAY_HOST));
  tlib_pass_if_str_equal (__func__, info.lang,
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_AGENT_LANGUAGE));
  tlib_pass_if_str_equal (__func__, info.version,
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_AGENT_VERSION));
  tlib_pass_if_str_equal (__func__, info.redirect_collector,
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_REDIRECT_COLLECTOR));
  tlib_pass_if_str_equal (__func__, settings_json,
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_SETTINGS));
  tlib_pass_if_str_equal (__func__, "[{\"label_type\":\"my_labels\",\"label_value\":\"hello\"}]",
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_LABELS));
  tlib_pass_if_str_equal (__func__, "[[\"my_environment\",\"hi\"]]",
    (const char *)nr_flatbuffers_table_read_bytes (&app, APP_FIELD_ENVIRONMENT));

  high_security = nr_flatbuffers_table_read_i8 (&app, APP_FIELD_HIGH_SECURITY, 0);
  tlib_pass_if_true (__func__, 1 == high_security, "high_security=%d", high_security);

  nr_app_info_destroy_fields (&info);
  nr_flatbuffers_destroy (&query);
}

static nr_flatbuffer_t *
create_app_reply (const char *agent_run_id, int8_t status, const char *connect_json)
{
  nr_flatbuffer_t *fb;
  uint32_t body;
  uint32_t agent_run_id_offset;
  uint32_t connect_json_offset;

  agent_run_id_offset = 0;
  connect_json_offset = 0;
  fb = nr_flatbuffers_create (0);

  if (connect_json && *connect_json) {
    connect_json_offset = nr_flatbuffers_prepend_string (fb, connect_json);
  }

  nr_flatbuffers_object_begin (fb, APP_REPLY_NUM_FIELDS);
  nr_flatbuffers_object_prepend_i8 (fb, APP_REPLY_FIELD_STATUS, status, 0);
  nr_flatbuffers_object_prepend_uoffset (fb, APP_REPLY_FIELD_CONNECT_REPLY,
    connect_json_offset, 0);
  body = nr_flatbuffers_object_end (fb);

  if (agent_run_id && *agent_run_id) {
    agent_run_id_offset = nr_flatbuffers_prepend_string (fb, agent_run_id);
  }

  nr_flatbuffers_object_begin (fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset (fb, MESSAGE_FIELD_DATA, body, 0);
  nr_flatbuffers_object_prepend_u8 (fb, MESSAGE_FIELD_DATA_TYPE,
    MESSAGE_BODY_APP_REPLY, 0);
  nr_flatbuffers_object_prepend_uoffset (fb, MESSAGE_FIELD_AGENT_RUN_ID,
    agent_run_id_offset, 0);
  nr_flatbuffers_finish (fb, nr_flatbuffers_object_end (fb));

  return fb;
}

static void
test_process_null_reply (void)
{
  nrapp_t app;
  nr_status_t st;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_UNKNOWN;

  st = nr_cmd_appinfo_process_reply (NULL, 0, &app);
  tlib_pass_if_status_failure (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_UNKNOWN);
}

static void
test_process_null_app (void)
{
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t *reply;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_UNKNOWN;

  reply = create_app_reply (NULL, APP_STATUS_UNKNOWN, NULL);
  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), NULL);
  tlib_pass_if_status_failure (__func__, st);

  nr_flatbuffers_destroy (&reply);
}

static void
test_process_missing_body (void)
{
  nr_flatbuffer_t *reply;
  nr_status_t st;
  uint32_t agent_run_id;

  reply = nr_flatbuffers_create (0);
  agent_run_id = nr_flatbuffers_prepend_string (reply, "12345");

  nr_flatbuffers_object_begin (reply, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset (reply, MESSAGE_FIELD_AGENT_RUN_ID, agent_run_id, 0);
  nr_flatbuffers_finish (reply, nr_flatbuffers_object_end (reply));

  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), NULL);
  tlib_pass_if_status_failure (__func__, st);

  nr_flatbuffers_destroy (&reply);
}

static void
test_process_wrong_body_type (void)
{
  nr_flatbuffer_t *fb;
  nr_status_t st;
  uint32_t agent_run_id;
  uint32_t body;

  fb = nr_flatbuffers_create (0);

  nr_flatbuffers_object_begin (fb, TRANSACTION_NUM_FIELDS);
  body = nr_flatbuffers_object_end (fb);

  agent_run_id = nr_flatbuffers_prepend_string (fb, "12345");
  nr_flatbuffers_object_begin (fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset (fb, MESSAGE_FIELD_AGENT_RUN_ID, agent_run_id, 0);
  nr_flatbuffers_object_prepend_uoffset (fb, MESSAGE_FIELD_DATA, body, 0);
  nr_flatbuffers_object_prepend_i8 (fb, MESSAGE_FIELD_DATA_TYPE, MESSAGE_BODY_TXN, 0);
  nr_flatbuffers_finish (fb, nr_flatbuffers_object_end (fb));

  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (fb),
    nr_flatbuffers_len (fb), NULL);
  tlib_pass_if_status_failure (__func__, st);

  nr_flatbuffers_destroy (&fb);
}

static void
test_process_unknown_app (void)
{
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t *reply;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_OK;

  reply = create_app_reply (NULL, APP_STATUS_UNKNOWN, NULL);
  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), &app);
  tlib_pass_if_status_success (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_UNKNOWN);

  nr_flatbuffers_destroy (&reply);
}

static void
test_process_invalid_app (void)
{
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t *reply;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_UNKNOWN;

  reply = create_app_reply (NULL, APP_STATUS_INVALID_LICENSE, NULL);
  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), &app);
  tlib_pass_if_status_success (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_INVALID);

  nr_flatbuffers_destroy (&reply);
}

static void
test_process_disconnected_app (void)
{
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t *reply;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_OK;

  reply = create_app_reply (NULL, APP_STATUS_DISCONNECTED, NULL);
  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), &app);
  tlib_pass_if_status_success (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_INVALID);

  nr_flatbuffers_destroy (&reply);
}

static void
test_process_still_valid_app (void)
{
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t *reply;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_UNKNOWN;

  reply = create_app_reply (NULL, APP_STATUS_STILL_VALID, NULL);
  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), &app);
  tlib_pass_if_status_success (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_OK);

  nr_flatbuffers_destroy (&reply);
}

static void
test_process_connected_app_missing_json (void)
{
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t *reply;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_UNKNOWN;

  reply = create_app_reply ("346595271037263", APP_STATUS_CONNECTED, NULL);
  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), &app);
  tlib_pass_if_status_failure (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_UNKNOWN);

  nr_flatbuffers_destroy (&reply);
}

static void
test_process_connected_app (void)
{
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t *reply;
  const char *connect_json;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_UNKNOWN;

  connect_json =
    "{"
      "\"agent_run_id\":\"346595271037263\","
      "\"url_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":false,"
          "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,\"replacement\":\"b\"}],"
      "\"transaction_name_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":false,"
          "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,\"replacement\":\"b\"}],"
      "\"transaction_segment_terms\":[{\"prefix\":\"Foo/Bar\",\"terms\":[\"a\",\"b\"]}]"
    "}";

  reply = create_app_reply ("346595271037263", APP_STATUS_CONNECTED, connect_json);

  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), &app);
  tlib_pass_if_status_success (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal (__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_not_null (__func__, app.connect_reply);
  tlib_pass_if_not_null (__func__, app.url_rules);
  tlib_pass_if_not_null (__func__, app.txn_rules);
  tlib_pass_if_not_null (__func__, app.segment_terms);

  /*
   * Perform same test again to make sure that populated fields are freed
   * before assignment.
   */
  app.state = NR_APP_UNKNOWN;
  st = nr_cmd_appinfo_process_reply (nr_flatbuffers_data (reply),
    nr_flatbuffers_len (reply), &app);
  tlib_pass_if_status_success (__func__, st);
  tlib_pass_if_int_equal (__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal (__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_not_null (__func__, app.connect_reply);
  tlib_pass_if_not_null (__func__, app.url_rules);
  tlib_pass_if_not_null (__func__, app.txn_rules);
  tlib_pass_if_not_null (__func__, app.segment_terms);

  nr_free (app.agent_run_id);
  nro_delete (app.connect_reply);
  nr_rules_destroy (&app.url_rules);
  nr_rules_destroy (&app.txn_rules);
  nr_segment_terms_destroy (&app.segment_terms);
  nr_flatbuffers_destroy (&reply);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void
test_main (void *vp NRUNUSED)
{
  test_create_empty_query ();
  test_create_query ();

  test_process_null_reply ();
  test_process_null_app ();
  test_process_unknown_app ();
  test_process_invalid_app ();
  test_process_disconnected_app ();
  test_process_still_valid_app ();
  test_process_connected_app_missing_json ();
  test_process_connected_app ();
  test_process_missing_body ();
  test_process_wrong_body_type ();
}
