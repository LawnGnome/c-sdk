#include "nr_axiom.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "nr_attributes.h"
#include "nr_attributes_private.h"
#include "nr_analytics_events.h"
#include "nr_errors.h"
#include "nr_header.h"
#include "nr_header_private.h"
#include "nr_rules.h"
#include "nr_slowsqls.h"
#include "nr_txn.h"
#include "nr_txn_private.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_metrics_private.h"
#include "util_random.h"
#include "util_strings.h"
#include "util_text.h"
#include "util_url.h"

#include "tlib_main.h"
#include "test_node_helpers.h"

typedef struct _test_txn_state_t {
  nrapp_t *txns_app;
} test_txn_state_t;

static char *
stack_dump_callback (void)
{
  return nr_strdup ("[\"Zip called on line 123 of script.php\","
    "\"Zap called on line 456 of hack.php\"]");
}

#define TEST_DAEMON_ID 1357

nrapp_t *
nr_app_verify_id (nrapplist_t *applist NRUNUSED, const char *agent_run_id NRUNUSED)
{
  nr_status_t rv;
  test_txn_state_t *p = (test_txn_state_t *)tlib_getspecific ();

  if (0 == p->txns_app) {
    return 0;
  }

  rv = nrt_mutex_lock (&p->txns_app->app_lock);
  tlib_pass_if_true ("app locked", NR_SUCCESS == rv, "rv=%d", (int)rv);
  return p->txns_app;
}

#define test_freeze_name(...) test_freeze_name_fn (__VA_ARGS__,__FILE__,__LINE__)

static void
test_freeze_name_fn (const char *testname, nr_path_type_t path_type, int background, const char *path,
                     const char *rules, const char *segment_terms, const char *expected_name, const char *file, int line)
{
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;
  nrapp_t appv;
  nrapp_t *app = &appv;
  test_txn_state_t *p = (test_txn_state_t *)tlib_getspecific ();

  nrt_mutex_init (&app->app_lock, 0);
  txn->app_connect_reply = 0;
  p->txns_app = app;

  txn->status.ignore = 0;
  txn->name = 0;
  txn->options.apdex_t = 0;
  txn->options.tt_is_apdex_f = 0;
  txn->options.tt_threshold = 0;

  txn->status.path_is_frozen = 0;
  txn->status.path_type = path_type;
  txn->status.background = background;
  txn->path = nr_strdup (path);

  if (rules) {
    nrobj_t *ob = nro_create_from_json (rules);

    app->url_rules = nr_rules_create_from_obj (nro_get_hash_array (ob, "url_rules", 0));
    app->txn_rules = nr_rules_create_from_obj (nro_get_hash_array (ob, "txn_rules", 0));
    nro_delete (ob);
  } else {
    app->url_rules = 0;
    app->txn_rules = 0;
  }

  if (segment_terms) {
    nrobj_t *st_obj = nro_create_from_json (segment_terms);

    app->segment_terms = nr_segment_terms_create_from_obj (st_obj);

    nro_delete (st_obj);
  } else {
    app->segment_terms = 0;
  }

  rv = nr_txn_freeze_name_update_apdex (txn);

  /* Txn path should be frozen no matter the return value. */
  test_pass_if_true (testname, (0 != txn->status.path_is_frozen), "txn->status.path_is_frozen=%d", txn->status.path_is_frozen);

  /*
   * Since there are no key transactions (0 == txn->app_connect_reply), apdex
   * and threshold should be unchanged.
   */
  test_pass_if_true (testname, (0 == txn->options.tt_threshold) && (0 == txn->options.apdex_t),
    "txn->options.tt_threshold=" NR_TIME_FMT " txn->options.apdex_t=" NR_TIME_FMT, txn->options.tt_threshold, txn->options.apdex_t);

  if (0 == expected_name) {
    test_pass_if_true (testname, NR_FAILURE == rv, "rv=%d", (int)rv);
  } else {
    test_pass_if_true (testname, NR_SUCCESS == rv, "rv=%d", (int)rv);
    test_pass_if_true (testname, 0 == nr_strcmp (expected_name, txn->name),
        "expected_name=%s actual_name=%s", expected_name, NRSAFESTR (txn->name));
  }

  nr_free (txn->path);
  nr_free (txn->name);
  nr_rules_destroy (&app->url_rules);
  nr_rules_destroy (&app->txn_rules);
  nr_segment_terms_destroy (&app->segment_terms);
  nrt_mutex_destroy (&app->app_lock);
}

#define test_key_txns(...) test_key_txns_fn (__VA_ARGS__,__FILE__,__LINE__)

static void
test_key_txns_fn (
  const char   *testname,
  const char   *path,
  int           is_apdex_f,
  nrtime_t      expected_apdex_t,
  nrtime_t      expected_tt_threshold,
  const char   *rules,
  const char   *segment_terms,
  nrobj_t      *key_txns,
  const char   *file,
  int           line)
{
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;
  nrapp_t appv;
  nrapp_t *app = &appv;
  test_txn_state_t *p = (test_txn_state_t *)tlib_getspecific ();

  nrt_mutex_init (&app->app_lock, 0);
  txn->app_connect_reply = nro_new_hash ();
  nro_set_hash (txn->app_connect_reply, "web_transactions_apdex", key_txns);
  p->txns_app = app;

  txn->status.ignore = 0;
  txn->name = 0;
  txn->options.apdex_t = 0;
  txn->options.tt_threshold = 0;

  txn->options.tt_is_apdex_f = is_apdex_f;
  txn->status.path_is_frozen = 0;
  txn->status.path_type = NR_PATH_TYPE_URI;
  txn->status.background = 0;
  txn->path = nr_strdup (path);

  if (rules) {
    nrobj_t *ob = nro_create_from_json (rules);

    app->url_rules = nr_rules_create_from_obj (nro_get_hash_array (ob, "url_rules", 0));
    app->txn_rules = nr_rules_create_from_obj (nro_get_hash_array (ob, "txn_rules", 0));
    nro_delete (ob);
  } else {
    app->url_rules = 0;
    app->txn_rules = 0;
  }

  if (segment_terms) {
    nrobj_t *st_obj = nro_create_from_json (segment_terms);

    app->segment_terms = nr_segment_terms_create_from_obj (st_obj);

    nro_delete (st_obj);
  } else {
    app->segment_terms = 0;
  }

  rv = nr_txn_freeze_name_update_apdex (txn);

  test_pass_if_true (testname, NR_SUCCESS == rv, "rv=%d", (int)rv);
  test_pass_if_true (testname, expected_apdex_t == txn->options.apdex_t,
    "expected_apdex_t=" NR_TIME_FMT " txn->options.apdex_t=" NR_TIME_FMT, expected_apdex_t, txn->options.apdex_t);
  test_pass_if_true (testname, expected_tt_threshold == txn->options.tt_threshold,
    "expected_tt_threshold=" NR_TIME_FMT " txn->options.tt_threshold=" NR_TIME_FMT, expected_tt_threshold, txn->options.tt_threshold);

  nro_delete (txn->app_connect_reply);
  nr_free (txn->name);
  nr_free (txn->path);
  nr_rules_destroy (&app->url_rules);
  nr_rules_destroy (&app->txn_rules);
  nr_segment_terms_destroy (&app->segment_terms);
  nrt_mutex_destroy (&app->app_lock);
}

static void
test_txn_cmp_options (void) {

  nrtxnopt_t o1 = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  nrtxnopt_t o2 = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  bool rv = false;

  rv = nr_txn_cmp_options (NULL, NULL);
  tlib_pass_if_true ("NULL pointers are equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options (&o1, &o1);
  tlib_pass_if_true ("Equal pointers are equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options (&o1, &o2);
  tlib_pass_if_true ("Equal fields are equal", rv, "rv=%d", (int)rv);


  o2.custom_events_enabled = 0;

  rv = nr_txn_cmp_options (NULL, &o1);
  tlib_pass_if_false ("NULL and other are not equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options (&o1, NULL);
  tlib_pass_if_false ("Other and null are not equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options (&o1, &o2);
  tlib_pass_if_false ("Inequal fields are not equal", rv, "rv=%d", (int)rv);

}


const char test_rules[] = "{\"url_rules\":[{\"match_expression\":\"what\",        \"replacement\":\"txn\"}," \
                                          "{\"match_expression\":\"ignore_path\", \"ignore\":true}]," \
                           "\"txn_rules\":[{\"match_expression\":\"ignore_txn\",  \"ignore\":true}," \
                                          "{\"match_expression\":\"rename_txn\",  \"replacement\":\"ok\"}]}";

const char test_segment_terms[] = "["
                                    "{\"prefix\":\"WebTransaction/Custom\",\"terms\":[\"white\",\"list\"]}"
                                  "]";

static void
test_freeze_name_update_apdex (void)
{
  test_txn_state_t *p = (test_txn_state_t *)tlib_getspecific ();

  /*
   * Test : Bad input to nr_txn_freeze_name_update_apdex
   */
  {
    nr_status_t rv;
    nrtxn_t txnv;
    nrtxn_t *txn = &txnv;
    nrapp_t appv;
    nrapp_t *app = &appv;

    nrt_mutex_init (&app->app_lock, 0);    txn->path = 0;
    txn->status.ignore = 0;
    txn->name = 0;
    txn->status.background = 0;
    txn->status.path_is_frozen = 0;
    txn->status.path_type = NR_PATH_TYPE_URI;
    txn->app_connect_reply = 0;
    app->url_rules = 0;
    app->txn_rules = 0;
    app->segment_terms = 0;
    p->txns_app = app;

    rv = nr_txn_freeze_name_update_apdex (0);
    tlib_pass_if_true ("no txn", NR_FAILURE == rv, "rv=%d", (int)rv);

    p->txns_app = 0;
    rv = nr_txn_freeze_name_update_apdex (txn);
    tlib_pass_if_true ("no app", NR_FAILURE == rv, "rv=%d", (int)rv);
    p->txns_app = app;

    txn->status.ignore = 1;
    rv = nr_txn_freeze_name_update_apdex (txn);
    tlib_pass_if_true ("ignore txn", NR_FAILURE == rv, "rv=%d", (int)rv);
    txn->status.ignore = 0;

    txn->status.path_is_frozen = 1;
    txn->status.path_type = NR_PATH_TYPE_URI;
    rv = nr_txn_freeze_name_update_apdex (txn);
    tlib_pass_if_true ("already frozen", (NR_SUCCESS == rv) && (0 == txn->name), "rv=%d txn->name=%p", (int)rv, txn->name);
    txn->status.path_is_frozen = 0;
    txn->status.path_type = NR_PATH_TYPE_URI;

    rv = nr_txn_freeze_name_update_apdex (txn);
    tlib_pass_if_true ("no path", (NR_SUCCESS == rv) && (0 == nr_strcmp (txn->name, "WebTransaction/Uri/unknown")),
      "rv=%d txn->name=%s", (int)rv, NRSAFESTR (txn->name));

    nr_free (txn->name);
    nrt_mutex_destroy (&app->app_lock);
  }

  /*
   * Transaction Naming Tests
   *
   * url_rules should only be applied to URI non-background txns and CUSTOM
   * non-background txns.
   */

  /*
   * Test : URI Web Transaction Naming
   */
  test_freeze_name ("URI WT",                                 NR_PATH_TYPE_URI, 0, "/zap.php",          0,          0, "WebTransaction/Uri/zap.php");
  test_freeze_name ("URI WT no slash",                        NR_PATH_TYPE_URI, 0, "zap.php",           0,          0, "WebTransaction/Uri/zap.php");
  test_freeze_name ("URI WT url_rule change",                 NR_PATH_TYPE_URI, 0, "/what.php",         test_rules, 0, "WebTransaction/Uri/txn.php");
  test_freeze_name ("URI WT url_rule ignore",                 NR_PATH_TYPE_URI, 0, "/ignore_path.php",  test_rules, 0, 0);
  test_freeze_name ("URI WT url_rule and txn_rule change",    NR_PATH_TYPE_URI, 0, "/rename_what.php",  test_rules, 0, "WebTransaction/Uri/ok.php");
  test_freeze_name ("URI WT url_rule change txn_rule ignore", NR_PATH_TYPE_URI, 0, "/ignore_what.php",  test_rules, 0, 0);

  /*
   * Test : URI Background Naming
   */
  test_freeze_name ("URI BG",                           NR_PATH_TYPE_URI, 1, "/zap.php",          0,          0, "OtherTransaction/php/zap.php");
  test_freeze_name ("URI BG no slash",                  NR_PATH_TYPE_URI, 1, "zap.php",           0,          0, "OtherTransaction/php/zap.php");
  test_freeze_name ("URI BG url_rule no change",        NR_PATH_TYPE_URI, 1, "/what.php",         test_rules, 0, "OtherTransaction/php/what.php");
  test_freeze_name ("URI BG url_rule no ignore",        NR_PATH_TYPE_URI, 1, "/ignore_path.php",  test_rules, 0, "OtherTransaction/php/ignore_path.php");
  test_freeze_name ("URI BG txn_rule change",           NR_PATH_TYPE_URI, 1, "/rename_txn.php",   test_rules, 0, "OtherTransaction/php/ok.php");
  test_freeze_name ("URI BG txn_rule ignore",           NR_PATH_TYPE_URI, 1, "/ignore_txn.php",   test_rules, 0, 0);

  /*
   * Test : ACTION Web Transaction Naming
   */
  test_freeze_name ("ACTION WT",                        NR_PATH_TYPE_ACTION, 0, "/zap.php",          0,          0, "WebTransaction/Action/zap.php");
  test_freeze_name ("ACTION WT no slash",               NR_PATH_TYPE_ACTION, 0, "zap.php",           0,          0, "WebTransaction/Action/zap.php");
  test_freeze_name ("ACTION WT url_rule no change",     NR_PATH_TYPE_ACTION, 0, "/what.php",         test_rules, 0, "WebTransaction/Action/what.php");
  test_freeze_name ("ACTION WT url_rule no ignore",     NR_PATH_TYPE_ACTION, 0, "/ignore_path.php",  test_rules, 0, "WebTransaction/Action/ignore_path.php");
  test_freeze_name ("ACTION WT txn_rule change",        NR_PATH_TYPE_ACTION, 0, "/rename_txn.php",   test_rules, 0, "WebTransaction/Action/ok.php");
  test_freeze_name ("ACTION WT txn_rule ignore",        NR_PATH_TYPE_ACTION, 0, "/ignore_txn.php",   test_rules, 0, 0);

  /*
   * Test : ACTION Background Naming
   */
  test_freeze_name ("ACTION BG",                        NR_PATH_TYPE_ACTION, 1, "/zap.php",          0,          0, "OtherTransaction/Action/zap.php");
  test_freeze_name ("ACTION BG no slash",               NR_PATH_TYPE_ACTION, 1, "zap.php",           0,          0, "OtherTransaction/Action/zap.php");
  test_freeze_name ("ACTION BG url_rule no change",     NR_PATH_TYPE_ACTION, 1, "/what.php",         test_rules, 0, "OtherTransaction/Action/what.php");
  test_freeze_name ("ACTION BG url_rule no ignore",     NR_PATH_TYPE_ACTION, 1, "/ignore_path.php",  test_rules, 0, "OtherTransaction/Action/ignore_path.php");
  test_freeze_name ("ACTION BG txn_rule change",        NR_PATH_TYPE_ACTION, 1, "/rename_txn.php",   test_rules, 0, "OtherTransaction/Action/ok.php");
  test_freeze_name ("ACTION BG txn_rule ignore",        NR_PATH_TYPE_ACTION, 1, "/ignore_txn.php",   test_rules, 0, 0);

  /*
   * Test : FUNCTION Web Transaction Naming
   */
  test_freeze_name ("FUNCTION WT",                      NR_PATH_TYPE_FUNCTION, 0, "/zap.php",          0,          0, "WebTransaction/Function/zap.php");
  test_freeze_name ("FUNCTION WT no slash",             NR_PATH_TYPE_FUNCTION, 0, "zap.php",           0,          0, "WebTransaction/Function/zap.php");
  test_freeze_name ("FUNCTION WT url_rule no change",   NR_PATH_TYPE_FUNCTION, 0, "/what.php",         test_rules, 0, "WebTransaction/Function/what.php");
  test_freeze_name ("FUNCTION WT url_rule no ignore",   NR_PATH_TYPE_FUNCTION, 0, "/ignore_path.php",  test_rules, 0, "WebTransaction/Function/ignore_path.php");
  test_freeze_name ("FUNCTION WT txn_rule change",      NR_PATH_TYPE_FUNCTION, 0, "/rename_txn.php",   test_rules, 0, "WebTransaction/Function/ok.php");
  test_freeze_name ("FUNCTION WT txn_rule ignore",      NR_PATH_TYPE_FUNCTION, 0, "/ignore_txn.php",   test_rules, 0, 0);

  /*
   * Test : FUNCTION Background Naming
   */
  test_freeze_name ("FUNCTION BG",                      NR_PATH_TYPE_FUNCTION, 1, "/zap.php",          0,          0, "OtherTransaction/Function/zap.php");
  test_freeze_name ("FUNCTION BG no slash",             NR_PATH_TYPE_FUNCTION, 1, "zap.php",           0,          0, "OtherTransaction/Function/zap.php");
  test_freeze_name ("FUNCTION BG url_rule no change",   NR_PATH_TYPE_FUNCTION, 1, "/what.php",         test_rules, 0, "OtherTransaction/Function/what.php");
  test_freeze_name ("FUNCTION BG url_rule no ignore",   NR_PATH_TYPE_FUNCTION, 1, "/ignore_path.php",  test_rules, 0, "OtherTransaction/Function/ignore_path.php");
  test_freeze_name ("FUNCTION BG txn_rule change",      NR_PATH_TYPE_FUNCTION, 1, "/rename_txn.php",   test_rules, 0, "OtherTransaction/Function/ok.php");
  test_freeze_name ("FUNCTION BG txn_rule ignore",      NR_PATH_TYPE_FUNCTION, 1, "/ignore_txn.php",   test_rules, 0, 0);

  /*
   * Test : CUSTOM Web Transaction Naming
   */
  test_freeze_name ("CUSTOM WT",                                 NR_PATH_TYPE_CUSTOM, 0, "/zap.php",          0,          0, "WebTransaction/Custom/zap.php");
  test_freeze_name ("CUSTOM WT no slash",                        NR_PATH_TYPE_CUSTOM, 0, "zap.php",           0,          0, "WebTransaction/Custom/zap.php");
  test_freeze_name ("CUSTOM WT url_rule change",                 NR_PATH_TYPE_CUSTOM, 0, "/what.php",         test_rules, 0, "WebTransaction/Custom/txn.php");
  test_freeze_name ("CUSTOM WT url_rule ignore",                 NR_PATH_TYPE_CUSTOM, 0, "/ignore_path.php",  test_rules, 0, 0);
  test_freeze_name ("CUSTOM WT url_rule and txn_rule change",    NR_PATH_TYPE_CUSTOM, 0, "/rename_what.php",  test_rules, 0, "WebTransaction/Custom/ok.php");
  test_freeze_name ("CUSTOM WT url_rule change txn_rule ignore", NR_PATH_TYPE_CUSTOM, 0, "/ignore_what.php",  test_rules, 0, 0);

  /*
   * Test : CUSTOM Background Naming
   */
  test_freeze_name ("CUSTOM BG",                      NR_PATH_TYPE_CUSTOM, 1, "/zap.php",          0,          0, "OtherTransaction/Custom/zap.php");
  test_freeze_name ("CUSTOM BG no slash",             NR_PATH_TYPE_CUSTOM, 1, "zap.php",           0,          0, "OtherTransaction/Custom/zap.php");
  test_freeze_name ("CUSTOM BG url_rule no change",   NR_PATH_TYPE_CUSTOM, 1, "/what.php",         test_rules, 0, "OtherTransaction/Custom/what.php");
  test_freeze_name ("CUSTOM BG url_rule no ignore",   NR_PATH_TYPE_CUSTOM, 1, "/ignore_path.php",  test_rules, 0, "OtherTransaction/Custom/ignore_path.php");
  test_freeze_name ("CUSTOM BG txn_rule change",      NR_PATH_TYPE_CUSTOM, 1, "/rename_txn.php",   test_rules, 0, "OtherTransaction/Custom/ok.php");
  test_freeze_name ("CUSTOM BG txn_rule ignore",      NR_PATH_TYPE_CUSTOM, 1, "/ignore_txn.php",   test_rules, 0, 0);

  /*
   * Test : UNKNOWN Web Transaction Naming
   */
  test_freeze_name ("UNKNOWN WT",                      NR_PATH_TYPE_UNKNOWN, 0, "/zap.php",          0,          0, "WebTransaction/Uri/<unknown>");
  test_freeze_name ("UNKNOWN WT no slash",             NR_PATH_TYPE_UNKNOWN, 0, "zap.php",           0,          0, "WebTransaction/Uri/<unknown>");
  test_freeze_name ("UNKNOWN WT url_rule no change",   NR_PATH_TYPE_UNKNOWN, 0, "/what.php",         test_rules, 0, "WebTransaction/Uri/<unknown>");
  test_freeze_name ("UNKNOWN WT url_rule no ignore",   NR_PATH_TYPE_UNKNOWN, 0, "/ignore_path.php",  test_rules, 0, "WebTransaction/Uri/<unknown>");
  test_freeze_name ("UNKNOWN WT txn_rule no change",   NR_PATH_TYPE_UNKNOWN, 0, "/rename_txn.php",   test_rules, 0, "WebTransaction/Uri/<unknown>");
  test_freeze_name ("UNKNOWN WT txn_rule no ignore",   NR_PATH_TYPE_UNKNOWN, 0, "/ignore_txn.php",   test_rules, 0, "WebTransaction/Uri/<unknown>");

  /*
   * Test : UNKNOWN Background Naming
   */
  test_freeze_name ("UNKNOWN BG",                      NR_PATH_TYPE_UNKNOWN, 1, "/zap.php",          0,          0, "OtherTransaction/php/<unknown>");
  test_freeze_name ("UNKNOWN BG no slash",             NR_PATH_TYPE_UNKNOWN, 1, "zap.php",           0,          0, "OtherTransaction/php/<unknown>");
  test_freeze_name ("UNKNOWN BG url_rule no change",   NR_PATH_TYPE_UNKNOWN, 1, "/what.php",         test_rules, 0, "OtherTransaction/php/<unknown>");
  test_freeze_name ("UNKNOWN BG url_rule no ignore",   NR_PATH_TYPE_UNKNOWN, 1, "/ignore_path.php",  test_rules, 0, "OtherTransaction/php/<unknown>");
  test_freeze_name ("UNKNOWN BG txn_rule no change",   NR_PATH_TYPE_UNKNOWN, 1, "/rename_txn.php",   test_rules, 0, "OtherTransaction/php/<unknown>");
  test_freeze_name ("UNKNOWN BG txn_rule no ignore",   NR_PATH_TYPE_UNKNOWN, 1, "/ignore_txn.php",   test_rules, 0, "OtherTransaction/php/<unknown>");

  /*
   * Test : Segment term application
   */
  test_freeze_name ("Prefix does not match",            NR_PATH_TYPE_ACTION, 0, "/zap.php",     0, test_segment_terms, "WebTransaction/Action/zap.php");
  test_freeze_name ("Prefix matches; all whitelisted",  NR_PATH_TYPE_CUSTOM, 0, "/white/list",  0, test_segment_terms, "WebTransaction/Custom/white/list");
  test_freeze_name ("Prefix matches; none whitelisted", NR_PATH_TYPE_CUSTOM, 0, "/black/foo",   0, test_segment_terms, "WebTransaction/Custom/*");
  test_freeze_name ("Prefix matches; some whitelisted", NR_PATH_TYPE_CUSTOM, 0, "/black/list",  0, test_segment_terms, "WebTransaction/Custom/*/list");

  /*
   * Test : Key Transactions
   */
  {
    nrobj_t *key_txns = nro_create_from_json (
      "{\"WebTransaction\\/Uri\\/key\":0.1,"
       "\"WebTransaction\\/Uri\\/ok\":0.1,"
       "\"WebTransaction\\/Uri\\/key_int\":2,"
       "\"WebTransaction\\/Uri\\/key_negative\":-0.1}");

    /*             Test Name                         path              is_apdex_f   apdex_t   tt_threshold */
    test_key_txns ("not key txn",                    "/not",           1,           0,        0,      test_rules, 0, key_txns);
    test_key_txns ("key txn",                        "/key",           0,           100000,   0,      test_rules, 0, key_txns);
    test_key_txns ("key txn is_apdex_f",             "/key",           1,           100000,   400000, test_rules, 0, key_txns);
    test_key_txns ("key txn after rules",            "/rename_what",   0,           100000,   0,      test_rules, 0, key_txns);
    test_key_txns ("key txn after rules is_apdex_f", "/rename_what",   1,           100000,   400000, test_rules, 0, key_txns);
    test_key_txns ("key txn apdex int",              "/key_int",       0,           2000000,  0,      test_rules, 0, key_txns);
    test_key_txns ("key txn apdex negative",         "/key_negative",  0,           0,        0,      test_rules, 0, key_txns);

    nro_delete (key_txns);
  }
}

#define test_txn_metric_created(...) test_txn_metric_created_fn (__VA_ARGS__,__FILE__,__LINE__)

static void
test_txn_metric_created_fn (const char *testname, nrmtable_t *table, uint32_t flags,
                        const char *name, nrtime_t count, nrtime_t total, nrtime_t exclusive,
                        nrtime_t min, nrtime_t max, nrtime_t sumsquares, const char *file, int line)
{
  const nrmetric_t *m = nrm_find (table, name);
  const char *nm = nrm_get_name (table, m);

  test_pass_if_true (testname, 0 != m, "m=%p", m);
  test_pass_if_true (testname, 0 == nr_strcmp (nm, name), "nm=%s name=%s", nm, name);

  test_pass_if_true (testname, flags == m->flags, "name=%s flags=%u m->flags=%u", name, flags, m->flags);
  test_pass_if_true (testname, nrm_count (m) == count, "name=%s nrm_count (m)=" NR_TIME_FMT " count=" NR_TIME_FMT, name, nrm_count (m), count);
  test_pass_if_true (testname, nrm_total (m) == total, "name=%s nrm_total (m)=" NR_TIME_FMT " total=" NR_TIME_FMT, name, nrm_total (m), total);
  test_pass_if_true (testname, nrm_exclusive (m) == exclusive, "name=%s nrm_exclusive (m)=" NR_TIME_FMT " exclusive=" NR_TIME_FMT, name, nrm_exclusive (m), exclusive);
  test_pass_if_true (testname, nrm_min (m) == min, "name=%s nrm_min (m)=" NR_TIME_FMT " min=" NR_TIME_FMT, name, nrm_min (m), min);
  test_pass_if_true (testname, nrm_max (m) == max, "name=%s nrm_max (m)=" NR_TIME_FMT " max=" NR_TIME_FMT, name, nrm_max (m), max);
  test_pass_if_true (testname, nrm_sumsquares (m) == sumsquares, "name=%s nrm_sumsquares (m)=" NR_TIME_FMT " sumsquares=" NR_TIME_FMT, name, nrm_sumsquares (m), sumsquares);
}

#define test_apdex_metric_created(...) test_apdex_metric_created_fn (__VA_ARGS__,__FILE__,__LINE__)

static void
test_apdex_metric_created_fn (const char *testname, nrmtable_t *table, uint32_t flags,
                              const char *name, nrtime_t satisfying, nrtime_t tolerating,
                              nrtime_t failing, nrtime_t min, nrtime_t max, const char *file, int line)
{
  const nrmetric_t *m = nrm_find (table, name);
  const char *nm = nrm_get_name (table, m);

  test_pass_if_true (testname, 0 != m, "m=%p", m);
  test_pass_if_true (testname, 0 == nr_strcmp (nm, name), "nm=%s name=%s", nm, name);

  if (m) {
    test_pass_if_true (testname, (flags|MET_IS_APDEX) == m->flags, "flags=%u m->flags=%u MET_IS_APDEX=%u", flags, m->flags, MET_IS_APDEX);
    test_pass_if_true (testname, nrm_satisfying (m) == satisfying, "nrm_satisfying (m)=" NR_TIME_FMT " satisfying=" NR_TIME_FMT, nrm_satisfying (m), satisfying);
    test_pass_if_true (testname, nrm_tolerating (m) == tolerating, "nrm_tolerating (m)=" NR_TIME_FMT " tolerating=" NR_TIME_FMT, nrm_tolerating (m), tolerating);
    test_pass_if_true (testname, nrm_failing (m) == failing, "nrm_failing (m)=" NR_TIME_FMT " failing=" NR_TIME_FMT, nrm_failing (m), failing);
    test_pass_if_true (testname, nrm_min (m) == min, "nrm_min (m)=" NR_TIME_FMT " min=" NR_TIME_FMT, nrm_min (m), min);
    test_pass_if_true (testname, nrm_max (m) == max, "nrm_max (m)=" NR_TIME_FMT " max=" NR_TIME_FMT, nrm_max (m), max);
    test_pass_if_true (testname, 0 == nrm_sumsquares (m), "nrm_sumsquares (m)=" NR_TIME_FMT, nrm_sumsquares (m));
  }
}

#define test_apdex_metrics(...) test_apdex_metrics_fn (__VA_ARGS__,__FILE__,__LINE__)

static void
test_apdex_metrics_fn (
  const char *txn_name,
  int has_error,
  nrtime_t duration,
  nrtime_t apdex_t,
  const char *mname,
  nrtime_t satisfying,
  nrtime_t tolerating,
  nrtime_t failing,
  const char *file,
  int line)
{
  int table_size;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->unscoped_metrics = nrm_table_create (0);
  txn->name = nr_strdup (txn_name);
  txn->options.apdex_t = apdex_t;
  txn->error = NULL;

  if (has_error) {
    int priority = 5;

    txn->error = nr_error_create (priority, "my/msg", "my/class", "[\"my\\/stacktrace\"]",
                                  nr_get_time ());
  }

  nr_txn_create_apdex_metrics (txn, duration);

  /*
   * Test : 'Apdex' metric created and is correct.
   */
  test_apdex_metric_created_fn (txn_name, txn->unscoped_metrics, MET_FORCED, "Apdex", satisfying,
                                tolerating, failing, apdex_t, apdex_t, file, line);

  /*
   * Test : Specific apdex metric created and correct, and table size.
   */
  table_size = nrm_table_size (txn->unscoped_metrics);
  if (mname) {
    test_apdex_metric_created_fn (txn_name, txn->unscoped_metrics, 0, mname, satisfying, tolerating,
                                  failing, apdex_t, apdex_t, file, line);

    test_pass_if_true (txn_name, 2 == table_size, "table_size=%d", table_size);
  } else {
    test_pass_if_true (txn_name, 1 == table_size, "table_size=%d", table_size);
  }

  nr_free (txn->name);
  nrm_table_destroy (&txn->unscoped_metrics);
  nr_error_destroy (&txn->error);
}

static void
test_create_apdex_metrics (void)
{
  /* Should not blow up on NULL input */
  nr_txn_create_apdex_metrics (0, 0);

  /*                                                        has error
   *                                                        *  duration
   * Test : Apdex value is properly calculated.             *  *   apdex                         Satisfying
   *                                                        *  *   *                             *  Tolerating
   *                  txn_name                              *  *   *  Specific Metric Name       *  *  Failing    */
  test_apdex_metrics (NULL,                                 0, 2,  4, NULL,                      1, 0, 0);
  test_apdex_metrics ("nope",                               0, 2,  4, NULL,                      1, 0, 0);
  test_apdex_metrics ("OtherTransaction/php/path.php",      0, 2,  4, "Apdex/php/path.php",      1, 0, 0);
  test_apdex_metrics ("WebTransaction/Uri/path.php",        0, 2,  4, "Apdex/Uri/path.php",      1, 0, 0);
  test_apdex_metrics ("OtherTransaction/Action/path.php",   0, 5,  4, "Apdex/Action/path.php",   0, 1, 0);
  test_apdex_metrics ("WebTransaction/Action/path.php",     0, 17, 4, "Apdex/Action/path.php",   0, 0, 1);
  test_apdex_metrics ("OtherTransaction/Function/path.php", 1, 1,  4, "Apdex/Function/path.php", 0, 0, 1);
  test_apdex_metrics ("WebTransaction/Function/path.php",   0, 2,  4, "Apdex/Function/path.php", 1, 0, 0);
  test_apdex_metrics ("OtherTransaction/Custom/path.php",   0, 2,  4, "Apdex/Custom/path.php",   1, 0, 0);
  test_apdex_metrics ("OtherTransaction/php/<unknown>",     0, 2,  4, "Apdex/php/<unknown>",     1, 0, 0);
  test_apdex_metrics ("WebTransaction/Uri/<unknown>",       0, 2,  4, "Apdex/Uri/<unknown>",     1, 0, 0);
}

static void
test_create_error_metrics (void)
{
  int table_size;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->status.background = 0;
  txn->trace_strings = 0;
  txn->unscoped_metrics = 0;

  /*
   * Test : Bad Params.  Should not blow up.
   */
  nr_txn_create_error_metrics (0, 0);
  nr_txn_create_error_metrics (0, "WebTransaction/Action/not_words");
  nr_txn_create_error_metrics (txn, 0);
  nr_txn_create_error_metrics (txn, "");
  nr_txn_create_error_metrics (txn, "WebTransaction/Action/not_words"); /* No metric table */

  /*
   * Test : Web Transaction
   */
  txn->trace_strings = nr_string_pool_create ();
  txn->unscoped_metrics = nrm_table_create (2);

  nr_txn_create_error_metrics (txn, "WebTransaction/Action/not_words");

  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("three error metrics created", 3 == table_size, "table_size=%d", table_size);
  test_txn_metric_created ("rollup", txn->unscoped_metrics, MET_FORCED, "Errors/all", 1, 0, 0, 0, 0, 0);
  test_txn_metric_created ("web rollup", txn->unscoped_metrics, MET_FORCED, "Errors/allWeb", 1, 0, 0, 0, 0, 0);
  test_txn_metric_created ("specific", txn->unscoped_metrics, MET_FORCED, "Errors/WebTransaction/Action/not_words", 1, 0, 0, 0, 0, 0);

  /*
   * Test : Background Task
   */
  nr_string_pool_destroy (&txn->trace_strings);
  nrm_table_destroy (&txn->unscoped_metrics);
  txn->trace_strings = nr_string_pool_create ();
  txn->unscoped_metrics = nrm_table_create (2);

  txn->status.background = 1;
  nr_txn_create_error_metrics (txn, "OtherTransaction/Custom/zap");

  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("three error metrics created", 3 == table_size, "table_size=%d", table_size);
  test_txn_metric_created ("rollup", txn->unscoped_metrics, MET_FORCED, "Errors/all", 1, 0, 0, 0, 0, 0);
  test_txn_metric_created ("background rollup", txn->unscoped_metrics, MET_FORCED, "Errors/allOther", 1, 0, 0, 0, 0, 0);
  test_txn_metric_created ("specific", txn->unscoped_metrics, MET_FORCED, "Errors/OtherTransaction/Custom/zap", 1, 0, 0, 0, 0, 0);

  nr_string_pool_destroy (&txn->trace_strings);
  nrm_table_destroy (&txn->unscoped_metrics);
}

static void
test_create_duration_metrics (void)
{
  int table_size;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  nr_memset ((void *) txn, 0, sizeof (txnv));

  txn->status.background = 0;
  txn->trace_strings = 0;
  txn->unscoped_metrics = 0;
  txn->root_kids_duration = 111;

  /*
   * Test : Bad Params.  Should not blow up.
   */
  nr_txn_create_duration_metrics (0, 0, 99);
  nr_txn_create_duration_metrics (0, "WebTransaction/Action/not_words", 99);
  nr_txn_create_duration_metrics (txn, 0, 99);
  nr_txn_create_duration_metrics (txn, "", 99);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 99); /* No metric table */

  /*
   * Test : Web Transaction
   */
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "WebTransaction",                           1, 999, 888, 999, 999, 998001);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "HttpDispatcher",                           1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "WebTransaction/Action/not_words",          1, 999, 888, 999, 999, 998001);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime",                  1, 999, 999, 999, 999, 998001);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime/Action/not_words", 1, 999, 999, 999, 999, 998001);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_int_equal ("number of metrics created", 5, table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Test : Web Transaction No Exclusive
   */
  txn->root_kids_duration = 999 + 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransaction",                           1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "HttpDispatcher",                           1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransaction/Action/not_words",          1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime",                  1, 999, 999, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime/Action/not_words", 1, 999, 999, 999, 999, 998001);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_int_equal ("number of metrics created", 5, table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Test : Web Transaction (no slash)
   */
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "NoSlash", 999);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransaction",          1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "HttpDispatcher",          1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "NoSlash",                 1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime", 1, 999, 999, 999, 999, 998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "NoSlashTotalTime",        1, 999, 999, 999, 999, 998001);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_int_equal ("number of metrics created", 5, table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Background Task
   */
  txn->root_kids_duration = 111;
  txn->status.background = 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("background", txn->unscoped_metrics, MET_FORCED, "OtherTransaction/all",            1, 999, 888, 999, 999, 998001);
  test_txn_metric_created ("background", txn->unscoped_metrics, MET_FORCED, "WebTransaction/Action/not_words", 1, 999, 888, 999, 999, 998001);
  test_txn_metric_created ("background", txn->unscoped_metrics, MET_FORCED, "OtherTransactionTotalTime",       1, 999, 999, 999, 999, 998001);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_int_equal ("number of metrics created", 4, table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Background Task No Exclusive
   */
  txn->root_kids_duration = 999 + 1;
  txn->status.background = 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("background no exclusive", txn->unscoped_metrics, MET_FORCED, "OtherTransaction/all",            1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("background no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransaction/Action/not_words", 1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("background no exclusive", txn->unscoped_metrics, MET_FORCED, "OtherTransactionTotalTime",       1, 999, 999, 999, 999, 998001);

  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_int_equal ("number of metrics created", 4, table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Background Task (no slash)
   */
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "NoSlash", 999);
  test_txn_metric_created ("background no slash", txn->unscoped_metrics, MET_FORCED, "OtherTransaction/all",      1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("background no slash", txn->unscoped_metrics, MET_FORCED, "NoSlash",                   1, 999,   0, 999, 999, 998001);
  test_txn_metric_created ("background no slash", txn->unscoped_metrics, MET_FORCED, "NoSlashTotalTime",          1, 999, 999, 999, 999, 998001);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_int_equal ("four duration metrics created", 4, table_size);
  nrm_table_destroy (&txn->unscoped_metrics);
}

static void
test_create_duration_metrics_async (void)
{
  int table_size;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->async_duration = 222;
  txn->status.background = 0;
  txn->trace_strings = 0;
  txn->unscoped_metrics = 0;
  txn->root_kids_duration = 111;
  txn->intrinsics = 0;

  /*
   * Test : Bad Params.  Should not blow up.
   */
  nr_txn_create_duration_metrics (0, 0, 99);
  nr_txn_create_duration_metrics (0, "WebTransaction/Action/not_words", 99);
  nr_txn_create_duration_metrics (txn, 0, 99);
  nr_txn_create_duration_metrics (txn, "", 99);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 99); /* No metric table */

  /*
   * Test : Web Transaction
   */
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "WebTransaction",                           1, 999,  888,  999,  999,  998001);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "HttpDispatcher",                           1, 999,    0,  999,  999,  998001);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime",                  1, 1221, 1221, 1221, 1221, 1490841);
  test_txn_metric_created ("web txn", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime/Action/not_words", 1, 1221, 1221, 1221, 1221, 1490841);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("number of metrics created", 5 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Test : Web Transaction No Exclusive
   */
  txn->root_kids_duration = 999 + 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransaction",                           1,  999,    0,  999,  999,  998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "HttpDispatcher",                           1,  999,    0,  999,  999,  998001);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime",                  1, 1221, 1221, 1221, 1221, 1490841);
  test_txn_metric_created ("web txn no exclusive", txn->unscoped_metrics, MET_FORCED, "WebTransactionTotalTime/Action/not_words", 1, 1221, 1221, 1221, 1221, 1490841);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("number of metrics created", 5 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Background Task
   */
  txn->root_kids_duration = 111;
  txn->status.background = 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("background", txn->unscoped_metrics, MET_FORCED, "OtherTransaction/all",      1,  999,  888,  999,  999,  998001);
  test_txn_metric_created ("background", txn->unscoped_metrics, MET_FORCED, "OtherTransactionTotalTime", 1, 1221, 1221, 1221, 1221, 1490841);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("number of metrics created", 4 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Background Task No Exclusive
   */
  txn->root_kids_duration = 999 + 1;
  txn->status.background = 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_duration_metrics (txn, "WebTransaction/Action/not_words", 999);
  test_txn_metric_created ("background no exclusive", txn->unscoped_metrics, MET_FORCED, "OtherTransaction/all",      1,  999,    0,  999,  999,  998001);
  test_txn_metric_created ("background no exclusive", txn->unscoped_metrics, MET_FORCED, "OtherTransactionTotalTime", 1, 1221, 1221, 1221, 1221, 1490841);
  tlib_pass_if_true ("number of metrics created", 4 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);
}

static void
test_create_queue_metric (void)
{

  int table_size;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->unscoped_metrics = 0;
  txn->root.start_time.when = 444;
  txn->status.http_x_start = 333;
  txn->status.background = 0;

  /*
   * Test : Bad Params.  Should not blow up.
   */
  nr_txn_create_queue_metric (0);
  nr_txn_create_queue_metric (txn); /* No metric table */

  /*
   * Test : Non-Zero Queue Time
   */
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_queue_metric (txn);
  test_txn_metric_created ("non-zero queue time", txn->unscoped_metrics, MET_FORCED, "WebFrontend/QueueTime", 1, 111, 111, 111, 111, 12321);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("non-zero queue time", 1 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Test : Background tasks should not have queue metrics.
   */
  txn->status.background = 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_queue_metric (txn);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("no queue metrics for background", 0 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);
  txn->status.background = 0;

  /*
   * Test : No queue start addded.
   */
  txn->status.http_x_start = 0;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_queue_metric (txn);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("no queue start", 0 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);

  /*
   * Test : Start time before queue start.
   */
  txn->status.http_x_start = txn->root.start_time.when + 1;
  txn->unscoped_metrics = nrm_table_create (2);
  nr_txn_create_queue_metric (txn);
  test_txn_metric_created ("txn start before queue start", txn->unscoped_metrics, MET_FORCED, "WebFrontend/QueueTime", 1, 0, 0, 0, 0, 0);
  table_size = nrm_table_size (txn->unscoped_metrics);
  tlib_pass_if_true ("txn start before queue start", 1 == table_size, "table_size=%d", table_size);
  nrm_table_destroy (&txn->unscoped_metrics);
}

static void
test_set_path (void)
{
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->path = 0;
  txn->status.path_is_frozen = 0;
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  rv = nr_txn_set_path (0, 0, 0, NR_PATH_TYPE_UNKNOWN, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path null params", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path null params", 0 == txn->path, "txn->path=%p", txn->path);

  rv = nr_txn_set_path (0, 0, "path_uri", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path null txn", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path null txn", 0 == txn->path, "txn->path=%p", txn->path);

  rv = nr_txn_set_path (0, txn, 0, NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path null path", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path null path", 0 == txn->path, "txn->path=%p", txn->path);

  rv = nr_txn_set_path (0, txn, "", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path empty path", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path empty path", 0 == txn->path, "txn->path=%p", txn->path);

  rv = nr_txn_set_path (0, txn, "path_uri", NR_PATH_TYPE_UNKNOWN, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path zero ptype", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path zero ptype", 0 == txn->path, "txn->path=%p", txn->path);

  rv = nr_txn_set_path (0, txn, "path_uri", NR_PATH_TYPE_UNKNOWN, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path negative ptype", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path negative ptype", 0 == txn->path, "txn->path=%p", txn->path);

  txn->status.path_is_frozen = 1;
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;
  rv = nr_txn_set_path (0, txn, "path_uri", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path frozen", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path frozen", 0 == txn->path, "txn->path=%p", txn->path);
  txn->status.path_is_frozen = 0;
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  rv = nr_txn_set_path (0, txn, "path_uri000", NR_PATH_TYPE_URI, NR_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);
  rv = nr_txn_set_path (0, txn, "path_uri", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path succeeds", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path sets path and ptype", NR_PATH_TYPE_URI == txn->status.path_type,
    "txn->status.path_type=%d", (int) txn->status.path_type);
  tlib_pass_if_true ("nr_txn_set_path sets path and ptype", 0 == nr_strcmp (txn->path, "path_uri000"),
    "txn->path=%s", NRSAFESTR (txn->path));
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  rv = nr_txn_set_path (0, txn, "path_uri", NR_PATH_TYPE_URI, NR_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);
  rv = nr_txn_set_path (0, txn, "path_uri0000", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("nr_txn_set_path succeeds", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("nr_txn_set_path sets path and ptype", NR_PATH_TYPE_URI == txn->status.path_type,
    "txn->status.path_type=%d", (int) txn->status.path_type);
  tlib_pass_if_true ("nr_txn_set_path sets path and ptype", 0 == nr_strcmp (txn->path, "path_uri"),
    "txn->path=%s", NRSAFESTR (txn->path));

  rv = nr_txn_set_path (0, txn, "path_custom", NR_PATH_TYPE_CUSTOM, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("higher priority name", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("higher priority name", NR_PATH_TYPE_CUSTOM == txn->status.path_type,
    "txn->status.path_type=%d", (int) txn->status.path_type);
  tlib_pass_if_true ("higher priority name", 0 == nr_strcmp ("path_custom", txn->path),
    "txn->path=%s", NRSAFESTR (txn->path));

  rv = nr_txn_set_path (0, txn, "path_uri", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("lower priority name ignored", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("lower priority name ignored", NR_PATH_TYPE_CUSTOM == txn->status.path_type,
    "txn->status.path_type=%d", (int) txn->status.path_type);
  tlib_pass_if_true ("lower priority name ignored", 0 == nr_strcmp ("path_custom", txn->path),
    "txn->path=%s", NRSAFESTR (txn->path));

  nr_free (txn->path);
}

static void
test_set_request_uri (void)
{
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->request_uri = 0;

  nr_txn_set_request_uri (0, 0);
  tlib_pass_if_true ("null params", 0 == txn->request_uri, "txn->request_uri=%p", txn->request_uri);

  nr_txn_set_request_uri (0, "the_uri");
  tlib_pass_if_true ("null txn", 0 == txn->request_uri, "txn->request_uri=%p", txn->request_uri);

  nr_txn_set_request_uri (txn, 0);
  tlib_pass_if_true ("null uri", 0 == txn->request_uri, "txn->request_uri=%p", txn->request_uri);

  nr_txn_set_request_uri (txn, "");
  tlib_pass_if_true ("empty uri", 0 == txn->request_uri, "txn->request_uri=%p", txn->request_uri);

  nr_txn_set_request_uri (txn, "the_uri");
  tlib_pass_if_true ("succeeds", 0 == nr_strcmp ("the_uri", txn->request_uri),
    "txn->request_uri=%s", NRSAFESTR (txn->request_uri));

  nr_txn_set_request_uri (txn, "alpha?zip=zap");
  tlib_pass_if_true ("params removed ?", 0 == nr_strcmp ("alpha", txn->request_uri),
    "txn->request_uri=%s", NRSAFESTR (txn->request_uri));

  nr_txn_set_request_uri (txn, "beta;zip=zap");
  tlib_pass_if_true ("params removed ;", 0 == nr_strcmp ("beta", txn->request_uri),
    "txn->request_uri=%s", NRSAFESTR (txn->request_uri));

  nr_txn_set_request_uri (txn, "gamma#zip=zap");
  tlib_pass_if_true ("params removed #", 0 == nr_strcmp ("gamma", txn->request_uri),
    "txn->request_uri=%s", NRSAFESTR (txn->request_uri));

  nr_free (txn->request_uri);
}

static void
test_record_error_worthy (void)
{
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->error = 0;
  txn->options.err_enabled = 1;
  txn->status.recording = 1;

  rv = nr_txn_record_error_worthy (0, 1);
  tlib_pass_if_true ("nr_txn_record_error_worthy null txn", NR_FAILURE == rv, "rv=%d", (int)rv);

  txn->options.err_enabled = 0;
  rv = nr_txn_record_error_worthy (txn, 1);
  tlib_pass_if_true ("nr_txn_record_error_worthy no err_enabled", NR_FAILURE == rv, "rv=%d", (int)rv);
  txn->options.err_enabled = 1;

  txn->status.recording = 0;
  rv = nr_txn_record_error_worthy (txn, 1);
  tlib_pass_if_true ("nr_txn_record_error_worthy no recording", NR_FAILURE == rv, "rv=%d", (int)rv);
  txn->status.recording = 1;

  /* No previous error */
  rv = nr_txn_record_error_worthy (txn, 1);
  tlib_pass_if_true ("nr_txn_record_error_worthy succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);

  /* Previous error exists */
  txn->error = nr_error_create (1, "msg", "class", "[]", nr_get_time ());

  rv = nr_txn_record_error_worthy (txn, 0);
  tlib_pass_if_true ("nr_txn_record_error_worthy lower priority", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_txn_record_error_worthy (txn, 2);
  tlib_pass_if_true ("nr_txn_record_error_worthy succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);

  nr_error_destroy (&txn->error);
}

static void
test_record_error (void)
{
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->options.err_enabled = 1;
  txn->status.recording = 1;
  txn->error = 0;
  txn->high_security = 0;

  /*
   * Nothing to test after these calls since no txn is provided.
   * However, we want to ensure that the stack parameter is freed.
   */
  nr_txn_record_error (0, 0, 0, 0, 0);
  nr_txn_record_error (0, 2, "msg", "class", "[\"A\",\"B\"]");

  txn->options.err_enabled = 0;
  nr_txn_record_error (txn, 2, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true ("nr_txn_record_error no err_enabled", 0 == txn->error, "txn->error=%p", txn->error);
  txn->options.err_enabled = 1;

  txn->status.recording = 0;
  nr_txn_record_error (txn, 2, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true ("nr_txn_record_error no recording", 0 == txn->error, "txn->error=%p", txn->error);
  txn->status.recording = 1;

  nr_txn_record_error (txn, 2, 0, "class", "[\"A\",\"B\"]");
  tlib_pass_if_true ("nr_txn_record_error no errmsg", 0 == txn->error, "txn->error=%p", txn->error);

  nr_txn_record_error (txn, 2, "msg", 0, "[\"A\",\"B\"]");
  tlib_pass_if_true ("nr_txn_record_error no class", 0 == txn->error, "txn->error=%p", txn->error);

  nr_txn_record_error (txn, 2, "", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true ("nr_txn_record_error empty errmsg", 0 == txn->error, "txn->error=%p", txn->error);

  nr_txn_record_error (txn, 2, "msg", "", "[\"A\",\"B\"]");
  tlib_pass_if_true ("nr_txn_record_error empty class", 0 == txn->error, "txn->error=%p", txn->error);

  nr_txn_record_error (txn, 2, "msg", "class", 0);
  tlib_pass_if_true ("nr_txn_record_error no stack", 0 == txn->error, "txn->error=%p", txn->error);

  /* Success when no previous error */
  nr_txn_record_error (txn, 2, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true ("no previous error", 0 != txn->error, "txn->error=%p", txn->error);
  tlib_pass_if_true ("no previous error", 2 == nr_error_priority (txn->error),
    "nr_error_priority (txn->error)=%d", nr_error_priority (txn->error));
  tlib_pass_if_true ("no previous error",
    0 == nr_strcmp ("msg", nr_error_get_message (txn->error)),
    "nr_error_get_message (txn->error)=%s",
    NRSAFESTR (nr_error_get_message (txn->error)));

  /* Failure with lower priority error than existing */
  nr_txn_record_error (txn, 1, "newmsg", "newclass", "[]");
  tlib_pass_if_true ("lower priority", 0 != txn->error, "txn->error=%p", txn->error);
  tlib_pass_if_true ("lower priority", 2 == nr_error_priority (txn->error),
    "nr_error_priority (txn->error)=%d", nr_error_priority (txn->error));
  tlib_pass_if_true ("lower priority",
    0 == nr_strcmp ("msg", nr_error_get_message (txn->error)),
    "nr_error_get_message (txn->error)=%s",
    NRSAFESTR (nr_error_get_message (txn->error)));

  /* Replace error when higher priority than existing */
  nr_txn_record_error (txn, 3, "newmsg", "newclass", "[\"C\",\"D\"]");
  tlib_pass_if_true ("higher priority", 0 != txn->error, "txn->error=%p", txn->error);
  tlib_pass_if_true ("higher priority", 3 == nr_error_priority (txn->error),
    "nr_error_priority (txn->error)=%d", nr_error_priority (txn->error));
  tlib_pass_if_true ("higher priority",
    0 == nr_strcmp ("newmsg", nr_error_get_message (txn->error)),
    "nr_error_get_message (txn->error)=%s",
    NRSAFESTR (nr_error_get_message (txn->error)));

  txn->high_security = 1;
  nr_txn_record_error (txn, 4, "don't show me", "high_security", "[\"C\",\"D\"]");
  tlib_pass_if_true ("high security error message stripped", 0 != txn->error, "txn->error=%p", txn->error);
  tlib_pass_if_true ("high security error message stripped", 4 == nr_error_priority (txn->error),
    "nr_error_priority (txn->error)=%d", nr_error_priority (txn->error));
  tlib_pass_if_true ("high security error message stripped",
    0 == nr_strcmp (NR_TXN_HIGH_SECURITY_ERROR_MESSAGE, nr_error_get_message (txn->error)),
    "nr_error_get_message (txn->error)=%s",
    NRSAFESTR (nr_error_get_message (txn->error)));
  txn->high_security = 0;

  nr_error_destroy (&txn->error);
}

static void
populate_test_node (int i, nrtxnnode_t *node)
{
  nrobj_t *stack;

  switch (i % 6) {
    case 0:
      stack = nro_new_array ();
      nro_set_array_string (stack, 0, "Zap called on line 14 of script.php");
      nro_set_array_string (stack, 0, "Zip called on line 14 of hack.php");

      node->data_hash = nro_new_hash ();
      nro_set_hash (node->data_hash, "backtrace", stack);
      nro_delete (stack);
      break;
    case 1:
    case 2:
    case 3:
    case 4:
      node->data_hash = nro_new_hash ();
      nro_set_hash_string (node->data_hash, "uri", "domain.com/path/to/glory");
      break;
    case 5:
      break;
  }
}

/*
 * Returns error codes -1, -2, -3, or 0 on success.
 */
static int
test_pq_integrity (nrtxn_t *txn)
{
  int i;
  int nindex_sum = 0;

  if ((0 == txn) || (txn->nodes_used < 0) || (txn->nodes_used > NR_TXN_NODE_LIMIT)) {
    return -1;
  }

  /*
   * Note : Nodes in txn->pq begin at index one.
   */
  for (i = 1; i <= txn->nodes_used; i++) {
    /*
     * Test : pq has correct nindex fields.
     *
     * Instead of sorting the pq, or checking this with an n^2 loop, we add the
     * nindex of each pq node and check that the sum is correct.
     */
    nindex_sum += (int)(txn->pq[i] - txn->nodes);

    /*
     * Test : Each pq element has lower priority than its parent
     */
    if (i > 1) {
      int r1 = nr_txn_pq_data_compare_wrapper (txn->pq[i], txn->pq[i / 2]);
      int r2 = nr_txn_pq_data_compare_wrapper (txn->pq[i / 2], txn->pq[i]);

      if ((0 != r1) || (0 == r2)) {
        return -2;
      }
    }
  }

  if (nindex_sum != ((txn->nodes_used * (txn->nodes_used - 1)) / 2)) {
    return -3;
  }

  return 0;
}

static void
test_save_if_slow_enough (void)
{
  int has_integrity;
  int i;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;
  nrtxnnode_t *rnp;

  nr_memset (txn, 0, sizeof (*txn));

  txn->nodes_used = 0;
  txn->options.tt_enabled = 1;
  txn->status.recording = 1;

  rnp = nr_txn_save_if_slow_enough (0, 0);
  tlib_pass_if_true ("nr_txn_save_if_slow_enough null params", 0 == rnp,
    "rnp=%p", rnp);

  /*
   * Fill up the pq
   */
  for (i = 0; i < NR_TXN_NODE_LIMIT; i++) {

    rnp = nr_txn_save_if_slow_enough (txn, i + 5);

    tlib_pass_if_true ("node saved", (0 != rnp) && (txn->nodes_used == (i + 1)),
      "rnp=%p i=%d txn->nodes_used=%d", rnp, i, txn->nodes_used);

    /*
     * Inserted nodes are populated in order to test that nr_txn_node_dispose_fields
     * is called.
     */
    populate_test_node (i, rnp);
  }

  rnp = nr_txn_save_if_slow_enough (txn, 4);
  tlib_pass_if_true ("faster node not saved", 0 == rnp, "rnp=%p", rnp);

  has_integrity = test_pq_integrity (txn);
  tlib_pass_if_true ("pq has integrity after node not saved", 0 == has_integrity, "has_integrity=%d", has_integrity);

  /*
   * Replace existing nodes with slower nodes
   */
  for (i = 0; i < NR_TXN_NODE_LIMIT; i++) {
    rnp = nr_txn_save_if_slow_enough (txn, i + NR_TXN_NODE_LIMIT + 5);

    tlib_pass_if_true ("node saved", 0 != rnp, "rnp=%p", rnp);
    tlib_pass_if_true ("node saved", NR_TXN_NODE_LIMIT == txn->nodes_used, "txn->nodes_used=%d", txn->nodes_used);

    /*
     * Inserted nodes are populated in order to test that nr_txn_node_dispose_fields
     * is called.
     */
    populate_test_node (i, rnp);
  }

  rnp = nr_txn_save_if_slow_enough (txn, 4 + NR_TXN_NODE_LIMIT);
  tlib_pass_if_true ("faster node not saved", 0 == rnp, "rnp=%p", rnp);

  has_integrity = test_pq_integrity (txn);
  tlib_pass_if_true ("pq has integrity after node not saved", 0 == has_integrity, "has_integrity=%d", has_integrity);

  /*
   * Replace with slower in decreasing order.  The decreasing order is important
   * since it tests inserting a node that isn't the very slowest.
   */
  for (i = NR_TXN_NODE_LIMIT - 1;  i >= 0; i--) {
    rnp = nr_txn_save_if_slow_enough (txn, (2 * NR_TXN_NODE_LIMIT) + i + 5);

    tlib_pass_if_true ("slower node saved", 0 != rnp, "rnp=%p", rnp);
    tlib_pass_if_true ("slower node saved", NR_TXN_NODE_LIMIT == txn->nodes_used,
      "txn->nodes_used=%d", txn->nodes_used);

    /*
     * Inserted nodes are populated in order to test that nr_txn_node_dispose_fields
     * is called.
     */
    populate_test_node (i, rnp);
  }

  has_integrity = test_pq_integrity (txn);
  tlib_pass_if_int_equal ("pq has integrity after decreasing order replacement", 0, has_integrity);

  for (i = 0; i < txn->nodes_used; i++) {
    nr_txn_node_dispose_fields (txn->nodes + i);
  }
}

static void
test_save_trace_node (void)
{
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;
  nrobj_t *data_hash = nro_new_hash ();

  nro_set_hash_string (data_hash, "alpha", "beta");
  nr_memset (txn, 0, sizeof (*txn));

  txn->last_added = 0;
  txn->nodes_used = 0;
  txn->options.tt_enabled = 1;
  txn->status.recording = 1;
  txn->trace_strings = nr_string_pool_create ();

  start.stamp = 100;
  start.when = 100;
  stop.stamp = start.stamp + 100;
  stop.when = start.when + 100;

  /* Don't blow up */
  nr_txn_save_trace_node (0, &start, &stop, "myname", NULL, data_hash);

  nr_txn_save_trace_node (txn, 0, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("null start", 0, txn->nodes_used);
  tlib_pass_if_null ("null start", txn->last_added);

  nr_txn_save_trace_node (txn, &start, 0, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("null stop", 0, txn->nodes_used);
  tlib_pass_if_null ("null stop", txn->last_added);

  nr_txn_save_trace_node (txn, &start, &stop, 0, NULL, data_hash);
  tlib_pass_if_int_equal ("null name", 0, txn->nodes_used);
  tlib_pass_if_null ("null name", txn->last_added);

  txn->status.recording = 0;
  nr_txn_save_trace_node (txn, &start, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("not recording", 0, txn->nodes_used);
  tlib_pass_if_null ("not recording", txn->last_added);
  txn->status.recording = 1;

  txn->options.tt_enabled = 0;
  nr_txn_save_trace_node (txn, &start, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("tt not enabled", 0, txn->nodes_used);
  tlib_pass_if_null ("tt not enabled", txn->last_added);
  txn->options.tt_enabled = 1;

  stop.when = start.when - 1;
  nr_txn_save_trace_node (txn, &start, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("negative duration", 0, txn->nodes_used);
  tlib_pass_if_null ("negative duration", txn->last_added);
  stop.when = start.when + 100;

  stop.stamp = start.stamp;
  nr_txn_save_trace_node (txn, &start, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("matching stamps", 0, txn->nodes_used);
  tlib_pass_if_null ("matching stamps", txn->last_added);
  stop.stamp = start.stamp + 100;

  stop.stamp = start.stamp - 1;
  nr_txn_save_trace_node (txn, &start, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("stamps in wrong order", 0, txn->nodes_used);
  tlib_pass_if_null ("stamps in wrong order", txn->last_added);
  stop.stamp = start.stamp + 100;

  stop.when = start.when;
  nr_txn_save_trace_node (txn, &start, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("zero duration", 1, txn->nodes_used);
  tlib_pass_if_not_null ("zero duration", txn->last_added);
  stop.when = start.when + 100;

  nr_txn_save_trace_node (txn, &start, &stop, "myname", NULL, data_hash);
  tlib_pass_if_int_equal ("success, no context", 2, txn->nodes_used);
  tlib_pass_if_not_null ("success, no context", txn->last_added);
  tlib_pass_if_int_equal ("success, no context", 0, txn->last_added->async_context);

  nr_txn_save_trace_node (txn, &start, &stop, "myname", "mycontext", data_hash);
  tlib_pass_if_int_equal ("success, with context", 3, txn->nodes_used);
  tlib_pass_if_not_null ("success, with context", txn->last_added);
  tlib_pass_if_int_equal ("success, with context", 2, txn->last_added->async_context);

  nr_string_pool_destroy (&txn->trace_strings);
  nr_txn_node_dispose_fields (txn->nodes + 0);
  nr_txn_node_dispose_fields (txn->nodes + 1);
  nr_txn_node_dispose_fields (txn->nodes + 2);
  nro_delete (data_hash);
}

static void
test_save_trace_node_async (void)
{
  nrtxntime_t start;
  nrtxntime_t stop;
  nrtxn_t *txn = (nrtxn_t *) nr_zalloc (sizeof (nrtxn_t));
  nrobj_t *data_hash = nro_new_hash ();

  nro_set_hash_string (data_hash, "alpha", "beta");
  nr_memset (txn, 0, sizeof (*txn));

  txn->last_added = 0;
  txn->nodes_used = 0;
  txn->options.tt_enabled = 1;
  txn->status.recording = 1;
  txn->trace_strings = nr_string_pool_create ();

  start.stamp = 100;
  start.when = 100;
  stop.stamp = start.stamp + 100;
  stop.when = start.when + 100;

  nr_txn_save_trace_node (txn, &start, &stop, "myname", "foo", data_hash);
  tlib_pass_if_true ("node saved success", 1 == txn->nodes_used, "txn->nodes_used=%d", txn->nodes_used);
  tlib_pass_if_true ("node saved success", 0 != txn->last_added, "txn->last_added=%p", txn->last_added);
  tlib_pass_if_true ("node has async_context", nr_string_add (txn->trace_strings, "foo") == txn->last_added->async_context, "async_context=%d", txn->last_added->async_context);

  nro_delete (data_hash);
  nr_txn_destroy (&txn);
}

#define test_created_txn(...) test_created_txn_fn (__VA_ARGS__,__FILE__,__LINE__)

static void
test_created_txn_fn (const char *testname, nrtxn_t *rv, nrtxnopt_t *correct, const char *file, int line)
{
  nrtxnopt_t *opts = &rv->options;

  /*
   * Test : GUID Created
   */
  test_pass_if_true (testname, 0 != rv->guid, "rv->guid=%p", rv->guid);
  test_pass_if_true (testname, NR_GUID_SIZE == nr_strlen (rv->guid), "nr_strlen (rv->guid)=%d", nr_strlen (rv->guid));

  /*
   * Test : root times and stamp
   */
  test_pass_if_true (testname, 0 == rv->root.start_time.stamp, "rv->root.start_time.stamp=%d", rv->root.start_time.stamp);
  test_pass_if_true (testname, 0 != rv->root.start_time.when, "rv->root.start_time.when=" NR_TIME_FMT, rv->root.start_time.when);
  test_pass_if_true (testname, 1 == rv->stamp, "rv->stamp=%d", rv->stamp);
  test_pass_if_true (testname, 0 == rv->root.async_context, "rv->root.async_context=%d", rv->root.async_context);

  /*
   * Test : Structures allocated
   */
  test_pass_if_true (testname, 0 != rv->trace_strings, "rv->trace_strings=%p", rv->trace_strings);
  test_pass_if_true (testname, 0 != rv->scoped_metrics, "rv->scoped_metrics=%p", rv->scoped_metrics);
  test_pass_if_true (testname, 0 != rv->unscoped_metrics, "rv->unscoped_metrics=%p", rv->unscoped_metrics);
  test_pass_if_true (testname, 0 != rv->intrinsics, "rv->intrinsics=%p", rv->intrinsics);
  test_pass_if_true (testname, 0 != rv->attributes, "rv->attributes=%p", rv->attributes);

  /*
   * Test : Kids Duration
   */
  test_pass_if_true (testname, 0 == rv->root_kids_duration, "rv->root_kids_duration=" NR_TIME_FMT, rv->root_kids_duration);
  test_pass_if_true (testname, &rv->root_kids_duration == rv->cur_kids_duration,
    "&rv->root_kids_duration=%p rv->cur_kids_duration=%p", &rv->root_kids_duration, rv->cur_kids_duration);

  /*
   * Test : Status
   */
  test_pass_if_true (testname, 0 == rv->status.ignore_apdex, "rv->status.ignore_apdex=%d", rv->status.ignore_apdex);
  test_pass_if_true (testname, rv->options.request_params_enabled == rv->options.request_params_enabled,
    "rv->options.request_params_enabled=%d rv->options.request_params_enabled=%d", rv->options.request_params_enabled, rv->options.request_params_enabled);
  test_pass_if_true (testname, 1 == rv->status.recording, "rv->status.recording=%d", rv->status.recording);

  if (rv->options.cross_process_enabled) {
  test_pass_if_true (testname, NR_STATUS_CROSS_PROCESS_START == rv->status.cross_process,
    "rv->status.cross_process=%d", (int)rv->status.cross_process);
  } else {
  test_pass_if_true (testname, NR_STATUS_CROSS_PROCESS_DISABLED == rv->status.cross_process,
    "rv->status.cross_process=%d", (int)rv->status.cross_process);
  }

  /*
   * Test : Transaction type bits
   */
  tlib_pass_if_uint_equal (testname, 0, rv->type);

  /*
   * Test : Options
   */
  test_pass_if_true (testname, opts->custom_events_enabled == correct->custom_events_enabled,
    "opts->custom_events_enabled=%d correct->custom_events_enabled=%d", opts->custom_events_enabled, correct->custom_events_enabled);
  test_pass_if_true (testname, opts->error_events_enabled == correct->error_events_enabled,
    "opts->error_events_enabled=%d correct->error_events_enabled=%d", opts->error_events_enabled, correct->error_events_enabled);
  test_pass_if_true (testname, opts->synthetics_enabled == correct->synthetics_enabled,
    "opts->synthetics_enabled=%d correct->synthetics_enabled=%d", opts->synthetics_enabled, correct->synthetics_enabled);
  test_pass_if_true (testname, opts->err_enabled == correct->err_enabled,
    "opts->err_enabled=%d correct->err_enabled=%d", opts->err_enabled, correct->err_enabled);
  test_pass_if_true (testname, opts->request_params_enabled == correct->request_params_enabled,
    "opts->request_params_enabled=%d correct->request_params_enabled=%d", opts->request_params_enabled, correct->request_params_enabled);
  test_pass_if_true (testname, opts->autorum_enabled == correct->autorum_enabled,
    "opts->autorum_enabled=%d correct->autorum_enabled=%d", opts->autorum_enabled, correct->autorum_enabled);
  test_pass_if_true (testname, opts->tt_enabled == correct->tt_enabled,
    "opts->tt_enabled=%d correct->tt_enabled=%d", opts->tt_enabled, correct->tt_enabled);
  test_pass_if_true (testname, opts->ep_enabled == correct->ep_enabled,
    "opts->ep_enabled=%d correct->ep_enabled=%d", opts->ep_enabled, correct->ep_enabled);
  test_pass_if_true (testname, opts->tt_recordsql == correct->tt_recordsql,
    "opts->tt_recordsql=%d correct->tt_recordsql=%d", (int)opts->tt_recordsql, (int)correct->tt_recordsql);
  test_pass_if_true (testname, opts->tt_slowsql == correct->tt_slowsql,
    "opts->tt_slowsql=%d correct->tt_slowsql=%d", opts->tt_slowsql, correct->tt_slowsql);
  test_pass_if_true (testname, opts->apdex_t == correct->apdex_t,
    "opts->apdex_t=" NR_TIME_FMT " correct->apdex_t=" NR_TIME_FMT, opts->apdex_t, correct->apdex_t);
  test_pass_if_true (testname, opts->tt_threshold == correct->tt_threshold,
    "opts->tt_threshold=" NR_TIME_FMT " correct->tt_threshold=" NR_TIME_FMT, opts->tt_threshold, correct->tt_threshold);
  test_pass_if_true (testname, opts->tt_is_apdex_f == correct->tt_is_apdex_f,
    "opts->tt_is_apdex_f=%d correct->tt_is_apdex_f=%d", opts->tt_is_apdex_f, correct->tt_is_apdex_f);
  test_pass_if_true (testname, opts->ep_threshold == correct->ep_threshold,
    "opts->ep_threshold=" NR_TIME_FMT " correct->ep_threshold=" NR_TIME_FMT, opts->ep_threshold, correct->ep_threshold);
  test_pass_if_true (testname, opts->ss_threshold == correct->ss_threshold,
    "opts->ss_threshold=" NR_TIME_FMT " correct->ss_threshold=" NR_TIME_FMT, opts->ss_threshold, correct->ss_threshold);
  test_pass_if_true (testname, opts->cross_process_enabled == correct->cross_process_enabled,
    "opts->cross_process_enabled=%d correct->cross_process_enabled=%d", opts->cross_process_enabled, correct->cross_process_enabled);
}

static void
test_begin_bad_params (void)
{
  nrapp_t app;
  nrtxnopt_t opts;
  nrtxn_t *txn;
  nr_attribute_config_t *config;

  config = nr_attribute_config_create ();

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_OK;
  nr_memset (&opts, 0, sizeof (opts));

  txn = nr_txn_begin (0, 0, config);
  tlib_pass_if_true ("null params", 0 == txn, "txn=%p", txn);

  txn = nr_txn_begin (0, &opts, config);
  tlib_pass_if_true ("null app", 0 == txn, "txn=%p", txn);

  app.state = NR_APP_INVALID;
  txn = nr_txn_begin (&app, &opts, config);
  tlib_pass_if_true ("invalid app", 0 == txn, "txn=%p", txn);
  app.state = NR_APP_OK;

  txn = nr_txn_begin (&app, NULL, config);
  tlib_pass_if_true ("NULL options", 0 == txn, "txn=%p", txn);

  txn = nr_txn_begin (&app, &opts, config);
  tlib_pass_if_true ("tests valid", 0 != txn, "txn=%p", txn);

  nr_txn_destroy (&txn);
  nr_attribute_config_destroy (&config);
}

static void
test_begin (void)
{
  nrtxn_t *rv;
  nrtxnopt_t optsv;
  nrtxnopt_t *opts = &optsv;
  nrtxnopt_t correct;
  nrapp_t appv;
  nrapp_t *app = &appv;
  nr_attribute_config_t *attribute_config;
  char *json;

  attribute_config = nr_attribute_config_create ();

  opts->custom_events_enabled = 109;
  opts->error_events_enabled = 27;
  opts->synthetics_enabled = 110;
  opts->analytics_events_enabled = 108;
  opts->err_enabled = 2;
  opts->request_params_enabled = 3;
  opts->autorum_enabled = 5;
  opts->tt_enabled = 7;
  opts->ep_enabled = 8;
  opts->tt_recordsql = NR_SQL_OBFUSCATED;
  opts->tt_slowsql = 10;
  opts->apdex_t = 11;       /* Should be unused */
  opts->tt_threshold = 12;
  opts->tt_is_apdex_f = 13;
  opts->ep_threshold = 14;
  opts->ss_threshold = 15;
  opts->cross_process_enabled = 22;

  app->rnd = nr_random_create ();
  nr_random_seed (app->rnd, 345345);
  app->info.high_security = 0;
  app->connect_reply = nro_new_hash ();
  nro_set_hash_boolean (app->connect_reply, "collect_errors", 1);
  nro_set_hash_boolean (app->connect_reply, "collect_traces", 1);
  nro_set_hash_double (app->connect_reply, "apdex_t", 0.6);
  nro_set_hash_string (app->connect_reply, "js_agent_file", "js-agent.newrelic.com\\/nr-213.min.js");
  app->state = NR_APP_OK;

  app->agent_run_id = nr_strdup ("12345678");
  app->info.appname = nr_strdup ("App Name;Foo;Bar");
  app->info.license = nr_strdup ("1234567890123456789012345678901234567890");
  app->info.host_display_name = nr_strdup ("foo_host");

  /*
   * Test : Options provided.
   */
  correct.custom_events_enabled = 109;
  correct.error_events_enabled = 27;
  correct.synthetics_enabled = 110;
  correct.err_enabled = 2;
  correct.request_params_enabled = 3;
  correct.autorum_enabled = 5;
  correct.analytics_events_enabled = 108;
  correct.tt_enabled = 7;
  correct.ep_enabled = 8;
  correct.tt_recordsql = NR_SQL_OBFUSCATED;
  correct.tt_slowsql = 10;
  correct.apdex_t = 600 * NR_TIME_DIVISOR_MS; /* From app */
  correct.tt_threshold = 4 * correct.apdex_t;
  correct.tt_is_apdex_f = 13;
  correct.ep_threshold = 14;
  correct.ss_threshold = 15;
  correct.cross_process_enabled = 22;

  rv = nr_txn_begin (app, opts, attribute_config);
  test_created_txn ("options provided", rv, &correct);
  json = nr_attributes_debug_json (rv->attributes);
  tlib_pass_if_str_equal ("display host attribute created", json,
    "{\"user\":[],\"agent\":["
    "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"host.displayName\",\"value\":\"foo_host\"}]}");
  nr_free (json);
  nr_txn_destroy (&rv);

  /*
   * Test : Options provided.  tt_is_apdex_f = 0
   */
  opts->tt_is_apdex_f = 0;
  correct.tt_threshold = 12;
  correct.tt_is_apdex_f = 0;

  rv = nr_txn_begin (app, opts, attribute_config);
  test_created_txn ("tt is not apdex_f", rv, &correct);
  nr_txn_destroy (&rv);

  /*
   * Test : App turns off traces
   */
  nro_set_hash_boolean (app->connect_reply, "collect_traces", 0);
  correct.tt_enabled = 0;
  correct.ep_enabled = 0;
  correct.tt_slowsql = 0;
  rv = nr_txn_begin (app, opts, attribute_config);
  test_created_txn ("app turns off traces", rv, &correct);
  nr_txn_destroy (&rv);

  /*
   * Test : App turns off errors
   */
  nro_set_hash_boolean (app->connect_reply, "collect_errors", 0);
  correct.err_enabled = 0;
  rv = nr_txn_begin (app, opts, attribute_config);
  test_created_txn ("app turns off errors", rv, &correct);
  nr_txn_destroy (&rv);

  /*
   * Test : App turns off analytics events
   */
  nro_set_hash_boolean (app->connect_reply, "collect_analytics_events", 0);
  correct.analytics_events_enabled = 0;
  rv = nr_txn_begin (app, opts, attribute_config);
  test_created_txn ("app turns off analytics events", rv, &correct);
  nr_txn_destroy (&rv);

  /*
   * Test : App turns off custom events.
   */
  nro_set_hash_boolean (app->connect_reply, "collect_custom_events", 0);
  correct.custom_events_enabled = 0;
  rv = nr_txn_begin (app, opts, attribute_config);
  test_created_txn ("app turns off custom events", rv, &correct);
  nr_txn_destroy (&rv);

  /*
   * Test : App turns off error events.
   */
  nro_set_hash_boolean (app->connect_reply, "collect_error_events", 0);
  correct.error_events_enabled = 0;
  rv = nr_txn_begin (app, opts, attribute_config);
  test_created_txn ("app turns off error events", rv, &correct);
  nr_txn_destroy (&rv);

  /*
   * Test : High security off
   */
  app->info.high_security = 0;
  rv = nr_txn_begin (app, opts, attribute_config);
  tlib_pass_if_int_equal ("high security off", 0, rv->high_security);
  nr_txn_destroy (&rv);

  /*
   * Test : High Security On
   */
  app->info.high_security = 1;
  rv = nr_txn_begin (app, opts, attribute_config);
  tlib_pass_if_int_equal ("app local high security copied to txn", 1, rv->high_security);
  nr_txn_destroy (&rv);
  app->info.high_security = 0;

  /*
   * Test : CPU usage populated on create
   */
  rv = nr_txn_begin (app, opts, attribute_config);
  /*
   * It is tempting to think that the process has already
   * incurred some user and system time at the start.
   * This may not be true if getrusage() is lieing to us,
   * or if the amount of time that has run is less than
   * the clock threshhold, or there are VM/NTP time issues, etc.
   *
   * However, since we haven't stopped the txn yet, the END
   * usage should definately be 0.
   */
  tlib_pass_if_true ("user_cpu[1]", 0 == rv->user_cpu[NR_CPU_USAGE_END], "user_cpu[1]=" NR_TIME_FMT, rv->user_cpu[NR_CPU_USAGE_END]);
  tlib_pass_if_true ("sys_cpu[1]", 0 == rv->sys_cpu[NR_CPU_USAGE_END], "sys_cpu[1]=" NR_TIME_FMT, rv->sys_cpu[NR_CPU_USAGE_END]);
  nr_txn_destroy (&rv);

  /*
   * Test : App name is populated in the new transaction.
   */
  rv = nr_txn_begin (app, opts, attribute_config);
  tlib_pass_if_str_equal ("primary_app_name", "App Name", rv->primary_app_name);
  nr_txn_destroy (&rv);

  nr_txn_destroy (&rv);
  nr_free (app->agent_run_id);
  nr_free (app->info.appname);
  nr_free (app->info.license);
  nr_free (app->info.host_display_name);
  nro_delete (app->connect_reply);
  nr_attribute_config_destroy (&attribute_config);
  nr_random_destroy (&app->rnd);
}

static int
metric_exists (nrmtable_t *metrics, const char *name)
{
  nrmetric_t *m = nrm_find (metrics, name);

  if ((0 == m) || (INT_MAX == nrm_min (m))) {
    return 0;
  }

  return 1;
}

static nrtxn_t *
create_full_txn_and_reset (nrapp_t *app)
{
  nrtxn_t *txn;

  /*
   * Create the Transaction
   */
  txn = nr_txn_begin (app, &nr_txn_test_options, 0);
  tlib_pass_if_not_null ("nr_txn_begin succeeds", txn);
  if (0 == txn) {
    return txn;
  }

  txn->root.start_time.when -= 5 * (txn->options.tt_threshold + txn->options.ep_threshold + txn->options.ss_threshold);
  txn->status.http_x_start = txn->root.start_time.when - 100;
  txn->high_security = 0;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;

  /*
   * Add an Error
   */
  nr_txn_record_error (txn, 1, "my_errmsg", "my_errclass", "[\"Zink called on line 123 of script.php\","
    "\"Zonk called on line 456 of hack.php\"]");
  tlib_pass_if_true ("error added", 0 != txn->error, "txn->error=%p", txn->error);

  /*
   * Add some TT Nodes.  A significant amount of testing node destruction is
   * done above in test_save_if_slow_enough.
   */
  {
    nr_node_datastore_params_t params = {
      .start        = {.when = txn->root.start_time.when + 1 * NR_TIME_DIVISOR, .stamp = 1},
      .stop         = {.when = txn->root.start_time.when + 2 * NR_TIME_DIVISOR, .stamp = 2},
      .datastore    = {.type = NR_DATASTORE_MYSQL},
      .sql          = {.sql = "SELECT * FROM TABLE;"},
      .callbacks    = {.backtrace = &stack_dump_callback,}
    };

    nr_txn_end_node_datastore (txn, &params, NULL);
  }

  {
    nr_node_datastore_params_t params = {
      .start        = {.when = txn->root.start_time.when + 3 * NR_TIME_DIVISOR, .stamp = 3},
      .stop         = {.when = txn->root.start_time.when + 4 * NR_TIME_DIVISOR, .stamp = 4},
      .datastore    = {.type = NR_DATASTORE_MONGODB},
      .collection   = "collection",
      .operation    = "operation",
    };

    nr_txn_end_node_datastore (txn, &params, NULL);
  }

  {
    nr_node_datastore_params_t params = {
      .start        = {.when = txn->root.start_time.when + 5 * NR_TIME_DIVISOR, .stamp = 5},
      .stop         = {.when = txn->root.start_time.when + 6 * NR_TIME_DIVISOR, .stamp = 6},
      .datastore    = {.type = NR_DATASTORE_MEMCACHE},
      .operation    = "insert",
    };

    nr_txn_end_node_datastore (txn, &params, NULL);
  }

  {
    nr_node_external_params_t params = {
      .start  = {.when = txn->root.start_time.when + 7 * NR_TIME_DIVISOR, .stamp = 7},
      .stop   = {.when = txn->root.start_time.when + 8 * NR_TIME_DIVISOR, .stamp = 8},
      .url    = "newrelic.com",
      .urllen = sizeof ("newrelic.com") - 1,
    };

    nr_txn_end_node_external (txn, &params);
  }

  tlib_pass_if_true ("four nodes added", 4 == txn->nodes_used, "txn->nodes_used=%d", txn->nodes_used);

  /*
   * Set the Path
   */
  nr_txn_set_path (0, txn, "zap.php", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true ("path set", 0 == nr_strcmp ("zap.php", txn->path), "txn->path=%s", NRSAFESTR (txn->path));

  return txn;
}

#define test_end_testcase(...) test_end_testcase_fn (__VA_ARGS__,__FILE__,__LINE__)

static void
test_end_testcase_fn (const char *testname,
                      const nrtxn_t *txn,
                      int expected_apdex_metrics,
                      int expected_error_metrics,
                      int expected_queuetime_metric,
                      const char *file,
                      int line)
{
  int txndata_apdex_metrics = 0;
  int txndata_error_metrics = 0;
  int txndata_queuetime_metric = 0;
  nrtime_t txndata_root_stop_time_when = 0;

  test_pass_if_true (testname, 0 != txn, "txn=%p", txn);

  if (0 == txn) {
    return;
  }

  test_pass_if_true (testname, 0 == txn->status.recording,
    "txn->status.recording=%d", txn->status.recording);

  txndata_apdex_metrics = metric_exists (txn->unscoped_metrics, "Apdex");
  txndata_error_metrics = metric_exists (txn->unscoped_metrics, "Errors/all");
  txndata_queuetime_metric = metric_exists (txn->unscoped_metrics, "WebFrontend/QueueTime");
  txndata_root_stop_time_when = txn->root.stop_time.when;

  if (txn->unscoped_metrics) {
    int metric_exists_code;

    /*
     * Test : Duration Metric Created
     */
    if (1 == txn->status.background) {
      metric_exists_code = metric_exists (txn->unscoped_metrics, "OtherTransaction/all");
    } else {
      metric_exists_code = metric_exists (txn->unscoped_metrics, "WebTransaction");
    }

    test_pass_if_true (testname, 1 == metric_exists_code , "metric_exists_code=%d txn->status.background=%d", metric_exists_code, txn->status.background);
  }

  test_pass_if_true (testname, txndata_apdex_metrics == expected_apdex_metrics,
    "txndata_apdex_metrics=%d expected_apdex_metrics=%d",
    txndata_apdex_metrics, expected_apdex_metrics);
  test_pass_if_true (testname, txndata_error_metrics == expected_error_metrics,
    "txndata_error_metrics=%d expected_error_metrics=%d",
    txndata_error_metrics, expected_error_metrics);
  test_pass_if_true (testname, txndata_queuetime_metric == expected_queuetime_metric,
    "txndata_queuetime_metric=%d expected_queuetime_metric=%d",
    txndata_queuetime_metric, expected_queuetime_metric);
  test_pass_if_true (testname, 0 != txndata_root_stop_time_when,
    "txndata_root_stop_time_when=" NR_TIME_FMT,
    txndata_root_stop_time_when);
}

static void
test_end (void)
{
  nrtxn_t *txn = 0;
  nrapp_t appv;
  nrapp_t *app = &appv;
  nrobj_t *rules_ob;
  test_txn_state_t *p = (test_txn_state_t *)tlib_getspecific ();

  app->rnd = nr_random_create ();
  nr_random_seed (app->rnd, 345345);
  app->info.high_security = 0;
  app->state = NR_APP_OK;
  nrt_mutex_init (&app->app_lock, 0);
  rules_ob = nro_create_from_json (test_rules);
  app->url_rules = nr_rules_create_from_obj (nro_get_hash_array (rules_ob, "url_rules", 0));
  app->txn_rules = nr_rules_create_from_obj (nro_get_hash_array (rules_ob, "txn_rules", 0));
  nro_delete (rules_ob);
  app->segment_terms = 0;
  app->connect_reply = nro_new_hash ();
  nro_set_hash_boolean (app->connect_reply, "collect_traces", 1);
  nro_set_hash_boolean (app->connect_reply, "collect_errors", 1);
  nro_set_hash_double (app->connect_reply, "apdex_t", 0.5);
  app->agent_run_id = nr_strdup ("12345678");
  app->info.appname = nr_strdup ("App Name;Foo;Bar");
  app->info.license = nr_strdup ("1234567890123456789012345678901234567890");
  app->info.host_display_name = nr_strdup ("foo_host");
  p->txns_app = app;

  /*
   * Test : Bad Parameters
   */
  nr_txn_end (0); /* Don't blow up */

  /*
   * Test : Ignore transaction situations
   */
  txn = create_full_txn_and_reset (app);
  txn->status.ignore = 1;
  nr_txn_end (txn);
  tlib_pass_if_true ("txn->status.ignore", 0 == txn->status.recording, "txn->status.recording=%d", txn->status.recording);
  nr_txn_destroy (&txn);

  txn = create_full_txn_and_reset (app);
  nr_txn_set_path (0, txn, "/ignore_path.php", NR_PATH_TYPE_CUSTOM, NR_NOT_OK_TO_OVERWRITE);
  nr_txn_end (txn);
  tlib_pass_if_true ("ignored by rules", 0 == txn->status.recording, "txn->status.recording=%d", txn->status.recording);
  tlib_pass_if_true ("ignored by rules", 1 == txn->status.ignore, "txn->status.ignore=%d", txn->status.ignore);
  nr_txn_destroy (&txn);

  /*
   * Test : Complete Transaction sent to cmd_txndata
   */
  txn = create_full_txn_and_reset (app);
  nr_txn_end (txn);
  test_end_testcase ("full txn to cmd_txndata", txn, 1 /* apdex */, 1 /* error*/, 1 /* queue */);
  nr_txn_destroy (&txn);

  /*
   * Test : Synthetics transaction
   */
  txn = create_full_txn_and_reset (app);
  txn->synthetics = nr_synthetics_create ("[1,100,\"a\",\"b\",\"c\"]");
  nr_txn_end (txn);
  test_end_testcase ("full txn to cmd_txndata", txn, 1 /* apdex */, 1 /* error*/, 1 /* queue */);
  tlib_pass_if_str_equal ("synthetics intrinsics", "a", nro_get_hash_string (txn->intrinsics, "synthetics_resource_id", NULL));
  nr_txn_destroy (&txn);

  /*
   * Test : No error metrics when no error
   */
  txn = create_full_txn_and_reset (app);
  nr_error_destroy (&txn->error);
  nr_txn_end (txn);
  test_end_testcase ("no error", txn, 1 /* apdex */, 0 /* error*/, 1 /* queue */);
  nr_txn_destroy (&txn);

  /*
   * Test : Background taks means no apdex metrics and no queuetime metric
   */
  txn = create_full_txn_and_reset (app);
  txn->status.background = 1;
  nr_txn_end (txn);
  test_end_testcase ("background task", txn, 0 /* apdex */, 1 /* error*/, 0 /* queue */);
  nr_txn_destroy (&txn);

  /*
   * Test : Ignore Apdex
   */
  txn = create_full_txn_and_reset (app);
  txn->status.ignore_apdex = 1;
  nr_txn_end (txn);
  test_end_testcase ("ignore apdex", txn, 0 /* apdex */, 1 /* error*/, 1  /* queue */ );
  nr_txn_destroy (&txn);

  /*
   * Test : No Queue Time
   */
  txn = create_full_txn_and_reset (app);
  txn->status.http_x_start = 0;
  nr_txn_end (txn);
  test_end_testcase ("no queue time", txn, 1 /* apdex */, 1 /* error*/, 0 /* queue */);
  nr_txn_destroy (&txn);

  /*
   * Test : Stop time in future
   */
  txn = create_full_txn_and_reset (app);
  txn->root.start_time.when = nr_get_time () + 999999;
  nr_txn_end (txn);
  test_end_testcase ("stop time in future", txn, 1 /* apdex */, 1 /* error*/, 1 /* queue */);
  nr_txn_destroy (&txn);

  /*
   * Test : Txn Already Halted
   */
  txn = create_full_txn_and_reset (app);
  nr_txn_end (txn);
  nr_txn_end (txn);
  test_end_testcase ("halted", txn, 1 /* apdex */, 1 /* error*/, 1 /* queue */);
  nr_txn_destroy (&txn);

  /*
   * Test : Missing Path
   */
  txn = create_full_txn_and_reset (app);
  nr_free (txn->name);
  nr_txn_end (txn);
  test_end_testcase ("missing path", txn, 1 /* apdex */, 1 /* error*/, 1 /* queue */);
  nr_txn_destroy (&txn);

  /*
   * Test : No Metric Table
   */
  txn = create_full_txn_and_reset (app);
  nrm_table_destroy (&txn->unscoped_metrics);
  nr_txn_end (txn);
  test_end_testcase ("no metric table", txn, 0 /* apdex */, 0 /* error*/, 0 /* queue */);
  nr_txn_destroy (&txn);

  nr_random_destroy (&app->rnd);
  nr_rules_destroy (&app->url_rules);
  nr_rules_destroy (&app->txn_rules);
  nrt_mutex_destroy (&app->app_lock);
  nro_delete (app->connect_reply);
  nr_free (app->agent_run_id);
  nr_free (app->info.appname);
  nr_free (app->info.license);
  nr_free (app->info.host_display_name);
}

static void
test_create_guid (void)
{
  char *guid;
  nr_random_t *rnd = nr_random_create ();

  nr_random_seed (rnd, 345345);

  guid = nr_txn_create_guid (NULL);
  tlib_pass_if_str_equal ("NULL random", guid, "0000000000000000");
  nr_free (guid);

  guid = nr_txn_create_guid (rnd);
  tlib_pass_if_str_equal ("guid creation", guid, "078ad44c1960eab7");
  nr_free (guid);

  guid = nr_txn_create_guid (rnd);
  tlib_pass_if_str_equal ("repeat guid creation", guid, "11da3087c4400533");
  nr_free (guid);

  nr_random_destroy (&rnd);
}

static void
test_should_force_persist (void)
{
  int should_force_persist;
  nrtxn_t txn;

  txn.status.has_inbound_record_tt = 0;
  txn.status.has_outbound_record_tt = 0;

  should_force_persist = nr_txn_should_force_persist (0);
  tlib_pass_if_true ("null txn", 0 == should_force_persist, "should_force_persist=%d", should_force_persist);

  should_force_persist = nr_txn_should_force_persist (&txn);
  tlib_pass_if_true ("nope", 0 == should_force_persist, "should_force_persist=%d", should_force_persist);

  txn.status.has_inbound_record_tt = 1;
  should_force_persist = nr_txn_should_force_persist (&txn);
  tlib_pass_if_true ("has_inbound_record_tt", 1 == should_force_persist, "should_force_persist=%d", should_force_persist);
  txn.status.has_inbound_record_tt = 0;

  txn.status.has_outbound_record_tt = 1;
  should_force_persist = nr_txn_should_force_persist (&txn);
  tlib_pass_if_true ("has_outbound_record_tt", 1 == should_force_persist, "should_force_persist=%d", should_force_persist);
  txn.status.has_outbound_record_tt = 0;

  txn.status.has_inbound_record_tt = 1;
  txn.status.has_outbound_record_tt = 1;
  should_force_persist = nr_txn_should_force_persist (&txn);
  tlib_pass_if_true ("has everything", 1 == should_force_persist, "should_force_persist=%d", should_force_persist);
}

static void
test_set_as_background_job (void)
{
  nrtxn_t txn;
  char *json;

  txn.status.path_is_frozen = 0;
  txn.status.background = 0;
  txn.unscoped_metrics = NULL;

  /* Don't blow up */
  nr_txn_set_as_background_job (0, 0);

  txn.status.path_is_frozen = 1;
  txn.unscoped_metrics = nrm_table_create (0);
  nr_txn_set_as_background_job (&txn, 0);
  tlib_pass_if_int_equal ("can't change background after path frozen", 0, txn.status.background);
  json = nr_metric_table_to_daemon_json (txn.unscoped_metrics);
  tlib_pass_if_str_equal ("supportability metric created", json,
    "[{\"name\":\"Supportability\\/background_status_change_prevented\","
      "\"data\":[1,0.00000,0.00000,0.00000,0.00000,0.00000],\"forced\":true}]");
  nr_free (json);
  nrm_table_destroy (&txn.unscoped_metrics);
  txn.status.path_is_frozen = 0;

  txn.unscoped_metrics = nrm_table_create (0);
  nr_txn_set_as_background_job (&txn, 0);
  tlib_pass_if_int_equal ("change background status success", 1, txn.status.background);
  tlib_pass_if_int_equal ("no supportability metric created", 0, nrm_table_size (txn.unscoped_metrics));
  nrm_table_destroy (&txn.unscoped_metrics);
}

static void
test_set_as_web_transaction (void)
{
  nrtxn_t txn;
  char *json;

  txn.status.path_is_frozen = 0;
  txn.status.background = 1;
  txn.unscoped_metrics = NULL;

  /* Don't blow up */
  nr_txn_set_as_web_transaction (0, 0);

  txn.status.path_is_frozen = 1;
  txn.unscoped_metrics = nrm_table_create (0);
  nr_txn_set_as_web_transaction (&txn, 0);
  tlib_pass_if_int_equal ("can't change background after path frozen", 1, txn.status.background);
  json = nr_metric_table_to_daemon_json (txn.unscoped_metrics);
  tlib_pass_if_str_equal ("supportability metric created", json,
    "[{\"name\":\"Supportability\\/background_status_change_prevented\","
      "\"data\":[1,0.00000,0.00000,0.00000,0.00000,0.00000],\"forced\":true}]");
  nr_free (json);
  nrm_table_destroy (&txn.unscoped_metrics);
  txn.status.path_is_frozen = 0;

  txn.unscoped_metrics = nrm_table_create (0);
  nr_txn_set_as_web_transaction (&txn, 0);
  tlib_pass_if_int_equal ("change background status success", 0, txn.status.background);
  tlib_pass_if_int_equal ("no supportability metric created", 0, nrm_table_size (txn.unscoped_metrics));
  nrm_table_destroy (&txn.unscoped_metrics);
}

static void
test_set_http_status (void)
{
  nrtxn_t txn;
  nrobj_t *obj;
  char *json;

  txn.status.background = 0;
  txn.attributes = nr_attributes_create (0);

  /*
   * Bad params, don't blow up!
   */
  nr_txn_set_http_status (0, 0);
  nr_txn_set_http_status (0, 503);

  nr_txn_set_http_status (&txn, 0);
  obj = nr_attributes_agent_to_obj (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_true ("zero http code", 0 == obj, "obj=%p", obj);

  txn.status.background = 1;
  nr_txn_set_http_status (&txn, 503);
  obj = nr_attributes_agent_to_obj (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_true ("background task", 0 == obj, "obj=%p", obj);
  txn.status.background = 0;

  nr_txn_set_http_status (&txn, 503);
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("success", 0 == nr_strcmp (
    "{"
      "\"user\":[],"
      "\"agent\":["
        "{"
          "\"dests\":[\"event\",\"trace\",\"error\"],"
          "\"key\":\"httpResponseCode\",\"value\":\"503\""
        "}"
      "]"
    "}",
    json), "json=%s", NRSAFESTR (json));
  nr_free (json);

  nr_attributes_destroy (&txn.attributes);
}

static void
test_add_user_custom_parameter (void)
{
  nrtxn_t txn;
  nrobj_t *obj = nro_new_int (123);
  nr_status_t st;
  nrobj_t *out;

  txn.attributes = nr_attributes_create (0);
  txn.high_security = 0;

  st = nr_txn_add_user_custom_parameter (NULL, NULL, NULL);
  tlib_pass_if_status_failure ("null params", st);

  st = nr_txn_add_user_custom_parameter (NULL, "my_key", obj);
  tlib_pass_if_status_failure ("null txn", st);

  st = nr_txn_add_user_custom_parameter (&txn, NULL, obj);
  tlib_pass_if_status_failure ("null key", st);

  st = nr_txn_add_user_custom_parameter (&txn, "my_key", NULL);
  tlib_pass_if_status_failure ("null obj", st);

  txn.high_security = 1;
  st = nr_txn_add_user_custom_parameter (&txn, "my_key", obj);
  tlib_pass_if_status_failure ("high_security", st);
  txn.high_security = 0;

  st = nr_txn_add_user_custom_parameter (&txn, "my_key", obj);
  tlib_pass_if_status_success ("success", st);
  out = nr_attributes_user_to_obj (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  test_obj_as_json ("success", out, "{\"my_key\":123}");
  nro_delete (out);

  nro_delete (obj);
  nr_attributes_destroy (&txn.attributes);
}

static void
test_add_request_parameter (void)
{
  nrtxn_t txn;
  nr_attribute_config_t *config;
  int legacy_enable = 0;
  char *json;

  txn.high_security = 0;
  config = nr_attribute_config_create ();
  nr_attribute_config_modify_destinations (config, "request.parameters.*", NR_ATTRIBUTE_DESTINATION_TXN_EVENT, 0);
  txn.attributes = nr_attributes_create (config);
  nr_attribute_config_destroy (&config);

  nr_txn_add_request_parameter (0, 0, 0, legacy_enable); /* Don't blow up */
  nr_txn_add_request_parameter (0, "key", "gamma", legacy_enable);

  nr_txn_add_request_parameter (&txn, "key", 0, legacy_enable);
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("no value", 0 == nr_strcmp ("{\"user\":[],\"agent\":[]}", json), "json=%s", NRSAFESTR (json));
  nr_free (json);

  nr_txn_add_request_parameter (&txn, 0, "gamma", legacy_enable);
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("no name", 0 == nr_strcmp ("{\"user\":[],\"agent\":[]}", json), "json=%s", NRSAFESTR (json));
  nr_free (json);

  nr_txn_add_request_parameter (&txn, "key", "gamma", legacy_enable);
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("success", 0 == nr_strcmp (
    "{\"user\":[],\"agent\":[{\"dests\":[\"event\"],"
      "\"key\":\"request.parameters.key\",\"value\":\"gamma\"}]}",
    json),"json=%s", NRSAFESTR (json));
  nr_free (json);

  legacy_enable = 1;
  nr_txn_add_request_parameter (&txn, "key", "gamma", legacy_enable);
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("legacy enable true", 0 == nr_strcmp (
    "{\"user\":[],\"agent\":[{\"dests\":[\"event\",\"trace\",\"error\"],"
      "\"key\":\"request.parameters.key\",\"value\":\"gamma\"}]}",
    json), "json=%s", NRSAFESTR (json));
  nr_free (json);
  legacy_enable = 0;

  txn.high_security = 1;
  nr_txn_add_request_parameter (&txn, "zip", "zap", legacy_enable);
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("high security prevents capture", 0 == nr_strcmp (
    "{\"user\":[],\"agent\":[{\"dests\":[\"event\",\"trace\",\"error\"],"
      "\"key\":\"request.parameters.key\",\"value\":\"gamma\"}]}",
    json), "json=%s", NRSAFESTR (json));
  nr_free (json);
  txn.high_security = 0;

  nr_attributes_destroy (&txn.attributes);
}

static void
test_set_stop_time (void)
{
  nrtxn_t txn;
  nrtxntime_t start;
  nrtxntime_t stop;
  nr_status_t rv;

  txn.status.recording = 1;
  txn.root.start_time.when = nr_get_time () - (NR_TIME_DIVISOR_MS * 5);
  txn.stamp = 10;
  start.stamp = 5;
  start.when = txn.root.start_time.when + 1;
  stop.stamp = 0;
  stop.when = 0;

  rv = nr_txn_set_stop_time (0, 0, 0);
  tlib_pass_if_true ("null params", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_txn_set_stop_time (0, &start, &stop);
  tlib_pass_if_true ("null txn", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_txn_set_stop_time (&txn, 0, &stop);
  tlib_pass_if_true ("null start", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_txn_set_stop_time (&txn, &start, 0);
  tlib_pass_if_true ("null stop", NR_FAILURE == rv, "rv=%d", (int)rv);

  txn.status.recording = 0;
  rv = nr_txn_set_stop_time (&txn, &start, &stop);
  tlib_pass_if_true ("not recording", NR_FAILURE == rv, "rv=%d", (int)rv);
  txn.status.recording = 1;

  start.when = txn.root.start_time.when - 1;
  rv = nr_txn_set_stop_time (&txn, &start, &stop);
  tlib_pass_if_true ("start before txn", NR_FAILURE == rv, "rv=%d", (int)rv);
  start.when = txn.root.start_time.when + 1;

  txn.root.start_time.when = nr_get_time () + (NR_TIME_DIVISOR * 10);
  start.when = txn.root.start_time.when + 1;
  rv = nr_txn_set_stop_time (&txn, &start, &stop);
  tlib_pass_if_true ("start time after stop time", NR_FAILURE == rv, "rv=%d", (int)rv);
  txn.root.start_time.when = nr_get_time () - (NR_TIME_DIVISOR_MS * 5);
  start.when = txn.root.start_time.when + 1;

  txn.stamp = 10;
  start.stamp = 15;
  rv = nr_txn_set_stop_time (&txn, &start, &stop);
  tlib_pass_if_true ("start stamp after stop stamp", NR_FAILURE == rv, "rv=%d", (int)rv);
  txn.stamp = 10;
  start.stamp = 5;

  rv = nr_txn_set_stop_time (&txn, &start, &stop);
  tlib_pass_if_true ("success", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true ("success", 10 == stop.stamp, "stop.stamp=%d", stop.stamp);
  tlib_pass_if_true ("success", 0 != stop.when, "stop.when=" NR_TIME_FMT, stop.when);
}

static void
test_set_request_referer (void)
{
  nrtxn_t txn;
  char *json;

  txn.attributes = nr_attributes_create (0);

  /* Don't blow up! */
  nr_txn_set_request_referer (0, 0);
  nr_txn_set_request_referer (&txn, 0);
  nr_txn_set_request_referer (0, "zap");

  nr_txn_set_request_referer (&txn, "zap");
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("request referer added successfully with correct destinations",
    0 == nr_strcmp (
      "{\"user\":[],\"agent\":[{\"dests\":[\"error\"],"
        "\"key\":\"request.headers.referer\",\"value\":\"zap\"}]}", json), "json=%s", NRSAFESTR (json));
  nr_free (json);

  /*
   * authentication credentials, query strings and fragments should be removed
   */
  nr_txn_set_request_referer (&txn, "http://user:pass@example.com/foo?q=bar#fragment");
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("request referer should be cleaned",
    0 == nr_strcmp (
      "{\"user\":[],\"agent\":[{\"dests\":[\"error\"],"
        "\"key\":\"request.headers.referer\",\"value\":\"http:\\/\\/example.com\\/foo\"}]}", json),
    "json=%s", NRSAFESTR (json));
  nr_free (json);

  nr_attributes_destroy (&txn.attributes);
}

static void
test_set_request_content_length (void)
{
  nrtxn_t txn;
  nrobj_t *obj;
  char *json;

  txn.attributes = nr_attributes_create (0);

  /* Bad params, don't blow up! */
  nr_txn_set_request_content_length (NULL, NULL);
  nr_txn_set_request_content_length (NULL, "12");

  nr_txn_set_request_content_length (&txn, NULL);
  obj = nr_attributes_agent_to_obj (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null ("null request content length", obj);

  nr_txn_set_request_content_length (&txn, "");
  obj = nr_attributes_agent_to_obj (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null ("empty request content length", obj);

  nr_txn_set_request_content_length (&txn, "whomp");
  obj = nr_attributes_agent_to_obj (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null ("nonsense request content length", obj);

  nr_txn_set_request_content_length (&txn, "0");
  obj = nr_attributes_agent_to_obj (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null ("zero content length", obj);

  nr_txn_set_request_content_length (&txn, "42");
  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_str_equal ("request content length added successfully with correct destinations",
    "{"
      "\"user\":[],"
      "\"agent\":["
        "{"
          "\"dests\":[\"event\",\"trace\",\"error\"],"
          "\"key\":\"request.headers.contentLength\",\"value\":42"
        "}"
      "]"
    "}",
    json);
  nr_free (json);

  nr_attributes_destroy (&txn.attributes);
}

static void
test_add_error_attributes (void)
{
  nrtxn_t txn;
  char *json;

  /* Don't blow up! */
  nr_txn_add_error_attributes (NULL);
  txn.error = NULL;
  nr_txn_add_error_attributes (&txn);

  txn.error = nr_error_create (1, "the_msg", "the_klass", "[]", 12345);
  txn.attributes = nr_attributes_create (0);

  nr_txn_add_error_attributes (&txn);

  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_true ("error attributes added successfully",
    0 == nr_strcmp (
      "{\"user\":[],\"agent\":[{\"dests\":[\"event\"],\"key\":\"errorType\",\"value\":\"the_klass\"},"
        "{\"dests\":[\"event\"],\"key\":\"errorMessage\",\"value\":\"the_msg\"}]}", json), "json=%s", NRSAFESTR (json));
  nr_free (json);

  nr_attributes_destroy (&txn.attributes);
  nr_error_destroy (&txn.error);
}

static void
test_duration (void)
{
  nrtxn_t txn;
  nrtime_t duration;

  duration = nr_txn_duration (NULL);
  tlib_pass_if_true ("null txn", 0 == duration, "duration=" NR_TIME_FMT, duration);

  txn.root.start_time.when = 1;
  txn.root.stop_time.when = 0;
  duration = nr_txn_duration (&txn);
  tlib_pass_if_true ("unfinished txn", 0 == duration, "duration=" NR_TIME_FMT, duration);

  txn.root.start_time.when = 1;
  txn.root.stop_time.when = 2;
  duration = nr_txn_duration (&txn);
  tlib_pass_if_true ("finished txn", 1 == duration, "duration=" NR_TIME_FMT, duration);
}

static void
test_queue_time (void)
{
  nrtxn_t txn;
  nrtime_t queue_time;

  txn.status.http_x_start  = 6 * NR_TIME_DIVISOR_MS;
  txn.root.start_time.when = 10 * NR_TIME_DIVISOR_MS;

  queue_time = nr_txn_queue_time (&txn);
  tlib_pass_if_true ("normal usage", 4 * NR_TIME_DIVISOR_MS == queue_time, "queue_time=" NR_TIME_FMT, queue_time);

  queue_time = nr_txn_queue_time (0);
  tlib_pass_if_true ("null txn", 0 == queue_time, "queue_time=" NR_TIME_FMT, queue_time);

  txn.status.http_x_start = 0;
  queue_time = nr_txn_queue_time (&txn);
  tlib_pass_if_true ("zero http_x_start", 0 == queue_time, "queue_time=" NR_TIME_FMT, queue_time);
  txn.status.http_x_start = 6 * NR_TIME_DIVISOR_MS;

  txn.root.start_time.when = 0;
  queue_time = nr_txn_queue_time (&txn);
  tlib_pass_if_true ("zero start time", 0 == queue_time, "queue_time=" NR_TIME_FMT, queue_time);
  txn.root.start_time.when = 10 * NR_TIME_DIVISOR_MS;
}

static void
test_set_queue_start (void)
{
  nrtxn_t txn;

  txn.status.http_x_start = 0;

  nr_txn_set_queue_start (NULL, 0);  /* Don't blow up! */
  nr_txn_set_queue_start (&txn, 0);
  nr_txn_set_queue_start (NULL, "1368811467146000");

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start (&txn, "t");
  tlib_pass_if_time_equal ("incomplete prefix", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start (&txn, "t=");
  tlib_pass_if_time_equal ("only prefix", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start (&txn, "abc");
  tlib_pass_if_time_equal ("bad value", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start (&txn, "t=abc");
  tlib_pass_if_time_equal ("bad value with prefix", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start (&txn, "1368811467146000");
  tlib_pass_if_time_equal ("success", txn.status.http_x_start, 1368811467146000ULL);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start (&txn, "t=1368811467146000");
  tlib_pass_if_time_equal ("success with prefix", txn.status.http_x_start, 1368811467146000ULL);
}

static void
test_create_rollup_metrics (void)
{
  nrtxn_t txn;
  char *json;

  nr_txn_create_rollup_metrics (NULL); /* Don't blow up! */

  txn.status.background = 0;
  txn.unscoped_metrics = nrm_table_create (0);
  txn.datastore_products = nr_string_pool_create ();
  nrm_force_add (txn.unscoped_metrics, "Datastore/all", 4 * NR_TIME_DIVISOR);
  nrm_force_add (txn.unscoped_metrics, "External/all", 1 * NR_TIME_DIVISOR);
  nrm_force_add (txn.unscoped_metrics, "Datastore/MongoDB/all", 2 * NR_TIME_DIVISOR);
  nrm_force_add (txn.unscoped_metrics, "Datastore/SQLite/all", 3 * NR_TIME_DIVISOR);
  nr_string_add (txn.datastore_products, "MongoDB");
  nr_string_add (txn.datastore_products, "SQLite");
  nr_txn_create_rollup_metrics (&txn);
  json = nr_metric_table_to_daemon_json (txn.unscoped_metrics);
  tlib_pass_if_str_equal ("web txn rollups", json,
    "[{\"name\":\"Datastore\\/all\",\"data\":[1,4.00000,4.00000,4.00000,4.00000,16.00000],\"forced\":true},"
     "{\"name\":\"External\\/all\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,1.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/MongoDB\\/all\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,4.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/SQLite\\/all\",\"data\":[1,3.00000,3.00000,3.00000,3.00000,9.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/allWeb\",\"data\":[1,4.00000,4.00000,4.00000,4.00000,16.00000],\"forced\":true},"
     "{\"name\":\"External\\/allWeb\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,1.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/MongoDB\\/allWeb\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,4.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/SQLite\\/allWeb\",\"data\":[1,3.00000,3.00000,3.00000,3.00000,9.00000],\"forced\":true}]");
  nr_free (json);
  nrm_table_destroy (&txn.unscoped_metrics);
  nr_string_pool_destroy (&txn.datastore_products);

  txn.status.background = 1;
  txn.unscoped_metrics = nrm_table_create (0);
  txn.datastore_products = nr_string_pool_create ();
  nrm_force_add (txn.unscoped_metrics, "Datastore/all", 4 * NR_TIME_DIVISOR);
  nrm_force_add (txn.unscoped_metrics, "External/all", 1 * NR_TIME_DIVISOR);
  nrm_force_add (txn.unscoped_metrics, "Datastore/MongoDB/all", 2 * NR_TIME_DIVISOR);
  nrm_force_add (txn.unscoped_metrics, "Datastore/SQLite/all", 3 * NR_TIME_DIVISOR);
  nr_string_add (txn.datastore_products, "MongoDB");
  nr_string_add (txn.datastore_products, "SQLite");
  nr_txn_create_rollup_metrics (&txn);
  json = nr_metric_table_to_daemon_json (txn.unscoped_metrics);
  tlib_pass_if_str_equal ("background rollups", json,
    "[{\"name\":\"Datastore\\/all\",\"data\":[1,4.00000,4.00000,4.00000,4.00000,16.00000],\"forced\":true},"
     "{\"name\":\"External\\/all\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,1.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/MongoDB\\/all\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,4.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/SQLite\\/all\",\"data\":[1,3.00000,3.00000,3.00000,3.00000,9.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/allOther\",\"data\":[1,4.00000,4.00000,4.00000,4.00000,16.00000],\"forced\":true},"
     "{\"name\":\"External\\/allOther\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,1.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/MongoDB\\/allOther\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,4.00000],\"forced\":true},"
     "{\"name\":\"Datastore\\/SQLite\\/allOther\",\"data\":[1,3.00000,3.00000,3.00000,3.00000,9.00000],\"forced\":true}]");
  nr_free (json);
  nrm_table_destroy (&txn.unscoped_metrics);
  nr_string_pool_destroy (&txn.datastore_products);
}

static void
test_record_custom_event (void)
{
  nrtxn_t txn;
  const char *json;
  nrtime_t now = 123 * NR_TIME_DIVISOR;
  const char *type = "my_event_type";
  nrobj_t *params = nro_create_from_json ("{\"a\":\"x\",\"b\":\"z\"}");

  txn.status.recording = 1;
  txn.high_security = 0;
  txn.custom_events = nr_analytics_events_create (10);
  txn.options.custom_events_enabled = 1;

  /*
   * NULL parameters: don't blow up!
   */
  nr_txn_record_custom_event_internal (NULL, NULL, NULL, 0);
  nr_txn_record_custom_event_internal (NULL, type, params, now);

  txn.options.custom_events_enabled = 0;
  nr_txn_record_custom_event_internal (&txn, type, params, now);
  json = nr_analytics_events_get_event_json (txn.custom_events, 0);
  tlib_pass_if_null ("custom events disabled", json);
  txn.options.custom_events_enabled = 1;

  txn.status.recording = 0;
  nr_txn_record_custom_event_internal (&txn, type, params, now);
  json = nr_analytics_events_get_event_json (txn.custom_events, 0);
  tlib_pass_if_null ("not recording", json);
  txn.status.recording = 1;

  txn.high_security = 1;
  nr_txn_record_custom_event_internal (&txn, type, params, now);
  json = nr_analytics_events_get_event_json (txn.custom_events, 0);
  tlib_pass_if_null ("high security enabled", json);
  txn.high_security = 0;

  nr_txn_record_custom_event_internal (&txn, type, params, now);
  json = nr_analytics_events_get_event_json (txn.custom_events, 0);
  tlib_pass_if_str_equal ("success", json,
    "[{\"type\":\"my_event_type\",\"timestamp\":123.00000},"
      "{\"b\":\"z\",\"a\":\"x\"},{}]");

  nr_analytics_events_destroy (&txn.custom_events);
  nro_delete (params);
}

static void
test_is_account_trusted (void)
{
  nrtxn_t txn;

  txn.app_connect_reply = nro_create_from_json ("{\"trusted_account_ids\":[1,3]}");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal ("NULL txn", 0, nr_txn_is_account_trusted (NULL, 0));
  tlib_pass_if_int_equal ("zero account", 0, nr_txn_is_account_trusted (&txn, 0));
  tlib_pass_if_int_equal ("negative account", 0, nr_txn_is_account_trusted (&txn, -1));

  /*
   * Test : Valid parameters.
   */
  tlib_pass_if_int_equal ("untrusted account", 0, nr_txn_is_account_trusted (&txn, 2));
  tlib_fail_if_int_equal ("trusted account", 0, nr_txn_is_account_trusted (&txn, 1));

  nro_delete (txn.app_connect_reply);
}

static void
test_should_save_trace (void)
{
  nrtxn_t txn;

  txn.nodes_used = 10;
  txn.options.tt_threshold = 100;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal ("NULL txn", 0, nr_txn_should_save_trace (NULL, 0));

  /*
   * Test : Fast, synthetics transaction. (The speed shouldn't matter: that's
   *        the point.)
   */
  txn.type = NR_TXN_TYPE_SYNTHETICS;
  tlib_fail_if_int_equal ("synthetics", 0, nr_txn_should_save_trace (&txn, 0));

  /*
   * Test : Fast, non-synthetics transaction.
   */
  txn.type = 0;
  tlib_pass_if_int_equal ("fast", 0, nr_txn_should_save_trace (&txn, 0));

  txn.nodes_used = 0;
  tlib_pass_if_int_equal ("zero nodes used", 0, nr_txn_should_save_trace (&txn, 100));
  txn.nodes_used = 10;

  /*
   * Test : Slow, non-synthetics transaction.
   */
  txn.type = 0;
  tlib_fail_if_int_equal ("slow", 0, nr_txn_should_save_trace (&txn, 100));
}

static void
test_adjust_exclusive_time (void)
{
  nrtxn_t txn;
  nrtime_t kids_duration;

  nr_txn_adjust_exclusive_time (NULL, 5 * NR_TIME_DIVISOR); /* Don't blow up! */

  txn.cur_kids_duration = NULL;
  nr_txn_adjust_exclusive_time (&txn, 5 * NR_TIME_DIVISOR); /* Don't blow up! */

  kids_duration = 0;
  txn.cur_kids_duration = &kids_duration;
  nr_txn_adjust_exclusive_time (&txn, 5 * NR_TIME_DIVISOR);
  tlib_pass_if_time_equal ("adjust exclusive time success", kids_duration, 5 * NR_TIME_DIVISOR);
}

static void
test_add_async_duration (void)
{
  nrtxn_t txn;

  /*
   * Test : Bad parameters.
   */
  nr_txn_add_async_duration (NULL, 5 * NR_TIME_DIVISOR);

  /*
   * Test : Normal operation.
   */
  txn.async_duration = 0;

  nr_txn_add_async_duration (&txn, 5 * NR_TIME_DIVISOR);
  tlib_pass_if_time_equal ("async_duration", 5 * NR_TIME_DIVISOR, txn.async_duration);

  nr_txn_add_async_duration (&txn, 0);
  tlib_pass_if_time_equal ("async_duration", 5 * NR_TIME_DIVISOR, txn.async_duration);

  nr_txn_add_async_duration (&txn, 10 * NR_TIME_DIVISOR);
  tlib_pass_if_time_equal ("async_duration", 15 * NR_TIME_DIVISOR, txn.async_duration);
}

static void
test_valid_node_end (void)
{
  nrtxn_t txn;
  nrtxntime_t start;
  nrtxntime_t stop;

  start.stamp = 1;
  stop.stamp = 2;
  start.when = 2 * NR_TIME_DIVISOR;
  stop.when = 3 * NR_TIME_DIVISOR;
  txn.status.recording = 1;
  txn.root.start_time.when = 1 * NR_TIME_DIVISOR;

  tlib_pass_if_int_equal ("null params", 0, nr_txn_valid_node_end (NULL, NULL, NULL));
  tlib_pass_if_int_equal ("null txn", 0, nr_txn_valid_node_end (NULL, &start, &stop));
  tlib_pass_if_int_equal ("null start", 0, nr_txn_valid_node_end (&txn, NULL, &stop));
  tlib_pass_if_int_equal ("null stop", 0, nr_txn_valid_node_end (&txn, &start, NULL));

  tlib_pass_if_int_equal ("success", 1, nr_txn_valid_node_end (&txn, &start, &stop));

  txn.status.recording = 0;
  tlib_pass_if_int_equal ("not recording", 0, nr_txn_valid_node_end (&txn, &start, &stop));
  txn.status.recording = 1;

  txn.root.start_time.when = 5 * NR_TIME_DIVISOR;
  tlib_pass_if_int_equal ("start after txn start", 0, nr_txn_valid_node_end (&txn, &start, &stop));
  txn.root.start_time.when = 1 * NR_TIME_DIVISOR;

  start.when = 5 * NR_TIME_DIVISOR;
  tlib_pass_if_int_equal ("start time after stop time", 0, nr_txn_valid_node_end (&txn, &start, &stop));
  start.when = 2 * NR_TIME_DIVISOR;

  start.stamp = 5;
  tlib_pass_if_int_equal ("start stamp after stop stamp", 0, nr_txn_valid_node_end (&txn, &start, &stop));
  start.stamp = 1;

  tlib_pass_if_int_equal ("success", 1, nr_txn_valid_node_end (&txn, &start, &stop));
}

static void
test_event_should_add_guid (void)
{
  nrtxn_t txn;

  tlib_pass_if_int_equal ("null txn", 0, nr_txn_event_should_add_guid (NULL));
  txn.type = 0;
  tlib_pass_if_int_equal ("zero type", 0, nr_txn_event_should_add_guid (&txn));
  txn.type = NR_TXN_TYPE_SYNTHETICS;
  tlib_pass_if_int_equal ("synthetics txn", 1, nr_txn_event_should_add_guid (&txn));
  txn.type = NR_TXN_TYPE_CAT_INBOUND;
  tlib_pass_if_int_equal ("inbound cat txn", 1, nr_txn_event_should_add_guid (&txn));
  txn.type = NR_TXN_TYPE_CAT_OUTBOUND;
  tlib_pass_if_int_equal ("outbound cat txn", 1, nr_txn_event_should_add_guid (&txn));
  txn.type = NR_TXN_TYPE_CAT_OUTBOUND << 1;
  tlib_pass_if_int_equal ("other txn type", 0, nr_txn_event_should_add_guid (&txn));
}

static void
test_unfinished_duration (void)
{
  nrtxn_t txn;
  nrtime_t t;

  txn.root.start_time.when = 0;
  t = nr_txn_unfinished_duration (&txn);
  tlib_pass_if_true ("unfinished duration", t > 0, "t=" NR_TIME_FMT, t);

  txn.root.start_time.when = nr_get_time () * 2;
  t = nr_txn_unfinished_duration (&txn);
  tlib_pass_if_time_equal ("overflow check", t, 0);

  t = nr_txn_unfinished_duration (NULL);
  tlib_pass_if_time_equal ("NULL txn", t, 0);
}

static void
test_should_create_apdex_metrics (void)
{
  nrtxn_t txn;

  tlib_pass_if_int_equal ("null txn", 0, nr_txn_should_create_apdex_metrics (NULL));

  txn.status.ignore_apdex = 0;
  txn.status.background = 0;
  tlib_pass_if_int_equal ("success", 1, nr_txn_should_create_apdex_metrics (&txn));

  txn.status.ignore_apdex = 0;
  txn.status.background = 1;
  tlib_pass_if_int_equal ("background", 0, nr_txn_should_create_apdex_metrics (&txn));

  txn.status.ignore_apdex = 1;
  txn.status.background = 0;
  tlib_pass_if_int_equal ("ignore_apdex", 0, nr_txn_should_create_apdex_metrics (&txn));

  txn.status.ignore_apdex = 1;
  txn.status.background = 1;
  tlib_pass_if_int_equal ("ignore_apdex and background", 0, nr_txn_should_create_apdex_metrics (&txn));
}

static void
test_add_cat_analytics_intrinsics (void)
{
  nrobj_t *bad_intrinsics = nro_new_array ();
  nrobj_t *intrinsics = nro_new_hash ();
  nrtxn_t *txn = (nrtxn_t *) nr_zalloc (sizeof (nrtxn_t));

  /*
   * Test : Bad parameters.
   */
  nr_txn_add_cat_analytics_intrinsics (NULL, intrinsics);
  nr_txn_add_cat_analytics_intrinsics (txn, NULL);
  nr_txn_add_cat_analytics_intrinsics (txn, bad_intrinsics);
  tlib_pass_if_int_equal ("bad parameters", 0, nro_getsize (intrinsics));

  nro_delete (bad_intrinsics);

  /*
   * Test : Non-CAT transaction.
   */
  txn->type = 0;
  nr_txn_add_cat_analytics_intrinsics (txn, intrinsics);
  tlib_pass_if_int_equal ("non-cat txn", 0, nro_getsize (intrinsics));

  /*
   * Test : Inbound CAT transaction without alternate path hashes.
   */
  txn->primary_app_name = nr_strdup ("App");
  txn->type = NR_TXN_TYPE_CAT_INBOUND;
  txn->cat.alternate_path_hashes = nro_create_from_json ("{\"ba2d6260\":null}");
  txn->cat.inbound_guid = nr_strdup ("eeeeeeee");
  txn->cat.referring_path_hash = nr_strdup ("01234567");
  txn->cat.trip_id = nr_strdup ("abcdef12");
  txn->guid = nr_strdup ("ffffffff");

  nr_txn_add_cat_analytics_intrinsics (txn, intrinsics);

  tlib_pass_if_str_equal ("tripId", "abcdef12", nro_get_hash_string (intrinsics, "nr.tripId", NULL));
  tlib_pass_if_str_equal ("pathHash", "ba2d6260", nro_get_hash_string (intrinsics, "nr.pathHash", NULL));
  tlib_pass_if_str_equal ("referringPathHash", "01234567", nro_get_hash_string (intrinsics, "nr.referringPathHash", NULL));
  tlib_pass_if_str_equal ("referringTransactionGuid", "eeeeeeee", nro_get_hash_string (intrinsics, "nr.referringTransactionGuid", NULL));
  tlib_pass_if_null ("alternatePathHashes", nro_get_hash_string (intrinsics, "nr.alternatePathHashes", NULL));

  nro_delete (intrinsics);
  nro_delete (txn->cat.alternate_path_hashes);
  nr_free (txn->cat.inbound_guid);
  nr_free (txn->cat.referring_path_hash);
  nr_free (txn->cat.trip_id);
  nr_free (txn->guid);
  nr_free (txn->primary_app_name);

  /*
   * Test : Inbound CAT transaction with alternate path hashes.
   */
  intrinsics = nro_new_hash ();
  txn->primary_app_name = nr_strdup ("App");
  txn->type = NR_TXN_TYPE_CAT_INBOUND;
  txn->cat.alternate_path_hashes = nro_create_from_json ("{\"a\":null,\"b\":null}");
  txn->cat.inbound_guid = nr_strdup ("eeeeeeee");
  txn->cat.referring_path_hash = nr_strdup ("01234567");
  txn->cat.trip_id = nr_strdup ("abcdef12");
  txn->guid = nr_strdup ("ffffffff");

  nr_txn_add_cat_analytics_intrinsics (txn, intrinsics);

  tlib_pass_if_str_equal ("tripId", "abcdef12", nro_get_hash_string (intrinsics, "nr.tripId", NULL));
  tlib_pass_if_str_equal ("pathHash", "ba2d6260", nro_get_hash_string (intrinsics, "nr.pathHash", NULL));
  tlib_pass_if_str_equal ("referringPathHash", "01234567", nro_get_hash_string (intrinsics, "nr.referringPathHash", NULL));
  tlib_pass_if_str_equal ("referringTransactionGuid", "eeeeeeee", nro_get_hash_string (intrinsics, "nr.referringTransactionGuid", NULL));
  tlib_pass_if_str_equal ("alternatePathHashes", "a,b", nro_get_hash_string (intrinsics, "nr.alternatePathHashes", NULL));

  nro_delete (intrinsics);
  nro_delete (txn->cat.alternate_path_hashes);
  nr_free (txn->cat.inbound_guid);
  nr_free (txn->cat.referring_path_hash);
  nr_free (txn->cat.trip_id);
  nr_free (txn->guid);
  nr_free (txn->primary_app_name);

  /*
   * Test : Outbound CAT transaction without alternate path hashes.
   */
  intrinsics = nro_new_hash ();
  txn->primary_app_name = nr_strdup ("App");
  txn->type = NR_TXN_TYPE_CAT_OUTBOUND;
  txn->cat.alternate_path_hashes = nro_create_from_json ("{\"b86be8ae\":null}");
  txn->cat.inbound_guid = NULL;
  txn->cat.referring_path_hash = NULL;
  txn->cat.trip_id = NULL;
  txn->guid = nr_strdup ("ffffffff");

  nr_txn_add_cat_analytics_intrinsics (txn, intrinsics);

  tlib_pass_if_str_equal ("tripId", "ffffffff", nro_get_hash_string (intrinsics, "nr.tripId", NULL));
  tlib_pass_if_str_equal ("pathHash", "b86be8ae", nro_get_hash_string (intrinsics, "nr.pathHash", NULL));
  tlib_pass_if_null ("referringPathHash", nro_get_hash_string (intrinsics, "nr.referringPathHash", NULL));
  tlib_pass_if_null ("referringTransactionGuid", nro_get_hash_string (intrinsics, "nr.referringTransactionGuid", NULL));
  tlib_pass_if_null ("alternatePathHashes", nro_get_hash_string (intrinsics, "nr.alternatePathHashes", NULL));

  nro_delete (intrinsics);
  nro_delete (txn->cat.alternate_path_hashes);
  nr_free (txn->cat.inbound_guid);
  nr_free (txn->cat.referring_path_hash);
  nr_free (txn->cat.trip_id);
  nr_free (txn->guid);
  nr_free (txn->primary_app_name);

  nr_free (txn);
}

static void
test_add_cat_intrinsics (void)
{
  nrobj_t *bad_intrinsics = nro_new_array ();
  nrobj_t *intrinsics = nro_new_hash ();
  nrtxn_t *txn = (nrtxn_t *) nr_zalloc (sizeof (nrtxn_t));

  /*
   * Test : Bad parameters.
   */
  nr_txn_add_cat_intrinsics (NULL, intrinsics);
  nr_txn_add_cat_intrinsics (txn, NULL);
  nr_txn_add_cat_intrinsics (txn, bad_intrinsics);
  tlib_pass_if_int_equal ("bad parameters", 0, nro_getsize (intrinsics));

  /*
   * Test : Non-CAT transaction.
   */
  txn->type = 0;
  nr_txn_add_cat_intrinsics (txn, intrinsics);
  tlib_pass_if_int_equal ("non-cat txn", 0, nro_getsize (intrinsics));

  /*
   * Test : CAT transaction.
   */
  txn->primary_app_name = nr_strdup ("App");
  txn->type = NR_TXN_TYPE_CAT_INBOUND;
  txn->cat.alternate_path_hashes = nro_create_from_json ("{\"a\":null,\"b\":null}");
  txn->cat.inbound_guid = nr_strdup ("eeeeeeee");
  txn->cat.referring_path_hash = nr_strdup ("01234567");
  txn->cat.trip_id = nr_strdup ("abcdef12");

  nr_txn_add_cat_intrinsics (txn, intrinsics);

  tlib_pass_if_str_equal ("trip_id", "abcdef12", nro_get_hash_string (intrinsics, "trip_id", NULL));
  tlib_pass_if_str_equal ("path_hash", "ba2d6260", nro_get_hash_string (intrinsics, "path_hash", NULL));

  nro_delete (bad_intrinsics);
  nro_delete (intrinsics);
  nro_delete (txn->cat.alternate_path_hashes);
  nr_free (txn->cat.inbound_guid);
  nr_free (txn->cat.referring_path_hash);
  nr_free (txn->cat.trip_id);
  nr_free (txn->primary_app_name);
  nr_free (txn);
}

static void
test_alternate_path_hashes (void)
{
  char *result;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  nr_memset ((void *) txn, 0, sizeof (txnv));
  txn->cat.alternate_path_hashes = nro_new_hash ();

  /*
   * Test : Bad parameters.
   */
  nr_txn_add_alternate_path_hash (NULL, "12345678");
  nr_txn_add_alternate_path_hash (txn, NULL);
  nr_txn_add_alternate_path_hash (txn, "");
  tlib_pass_if_int_equal ("hash size", 0, nro_getsize (txn->cat.alternate_path_hashes));

  tlib_pass_if_null ("NULL txn", nr_txn_get_alternate_path_hashes (NULL));

  /*
   * Test : Empty path hashes.
   */
  result = nr_txn_get_alternate_path_hashes (txn);
  tlib_pass_if_null ("empty path hashes", result);
  nr_free (result);

  /*
   * Test : Simple addition.
   */
  nr_txn_add_alternate_path_hash (txn, "12345678");
  tlib_pass_if_int_equal ("hash size", 1, nro_getsize (txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null ("hash existence", nro_get_hash_value (txn->cat.alternate_path_hashes, "12345678", NULL));

  nr_txn_add_alternate_path_hash (txn, "01234567");
  tlib_pass_if_int_equal ("hash size", 2, nro_getsize (txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null ("hash existence", nro_get_hash_value (txn->cat.alternate_path_hashes, "01234567", NULL));

  /*
   * Test : Duplicate.
   */
  nr_txn_add_alternate_path_hash (txn, "01234567");
  tlib_pass_if_int_equal ("hash size", 2, nro_getsize (txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null ("hash existence", nro_get_hash_value (txn->cat.alternate_path_hashes, "01234567", NULL));

  /*
   * Test : Retrieval.
   */
  result = nr_txn_get_alternate_path_hashes (txn);
  tlib_pass_if_str_equal ("path hashes", "01234567,12345678", result);
  nr_free (result);

  nro_delete (txn->cat.alternate_path_hashes);
}

static void
test_apdex_zone (void)
{
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  txn->error = NULL;
  txn->options.apdex_t = 10;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_char_equal ("NULL txn", 'F', nr_apdex_zone_label (nr_txn_apdex_zone (NULL, 0)));

  /*
   * Test : Normal transaction.
   */
  tlib_pass_if_char_equal ("satisfying", 'S', nr_apdex_zone_label (nr_txn_apdex_zone (txn, 10)));
  tlib_pass_if_char_equal ("tolerating", 'T', nr_apdex_zone_label (nr_txn_apdex_zone (txn, 30)));
  tlib_pass_if_char_equal ("failing",    'F', nr_apdex_zone_label (nr_txn_apdex_zone (txn, 50)));

  /*
   * Test : Error transaction.
   */
  txn->error = nr_error_create (0, "message", "class", "json", 0);
  tlib_pass_if_char_equal ("error", 'F', nr_apdex_zone_label (nr_txn_apdex_zone (txn, 10)));
  nr_error_destroy (&txn->error);
}

static void
test_get_cat_trip_id (void)
{
  char *guid = nr_strdup ("GUID");
  char *trip_id = nr_strdup ("Trip");
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null ("NULL txn", nr_txn_get_cat_trip_id (NULL));

  /*
   * Test : No trip ID or GUID.
   */
  txn->cat.trip_id = NULL;
  txn->guid = NULL;
  tlib_pass_if_null ("NULL txn", nr_txn_get_cat_trip_id (txn));

  /*
   * Test : GUID only.
   */
  txn->cat.trip_id = NULL;
  txn->guid = guid;
  tlib_pass_if_str_equal ("GUID only", guid, nr_txn_get_cat_trip_id (txn));

  /*
   * Test : Trip ID only.
   */
  txn->cat.trip_id = trip_id;
  txn->guid = NULL;
  tlib_pass_if_str_equal ("Trip only", trip_id, nr_txn_get_cat_trip_id (txn));

  /*
   * Test : Trip ID and GUID.
   */
  txn->cat.trip_id = trip_id;
  txn->guid = guid;
  tlib_pass_if_str_equal ("both", trip_id, nr_txn_get_cat_trip_id (txn));

  nr_free (guid);
  nr_free (trip_id);
}

static void
test_get_path_hash (void)
{
  char *result;
  nrtxn_t txnv;
  nrtxn_t *txn = &txnv;

  nr_memset ((void *) txn, 0, sizeof (txnv));
  txn->cat.alternate_path_hashes = nro_new_hash ();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null ("NULL txn", nr_txn_get_path_hash (NULL));

  /*
   * Test : Empty primary app name.
   */
  tlib_pass_if_null ("NULL primary app name", nr_txn_get_path_hash (txn));

  /*
   * Test : Empty transaction name.
   */
  txn->primary_app_name = nr_strdup ("App Name");
  result = nr_txn_get_path_hash (txn);
  tlib_pass_if_str_equal ("empty transaction name", "2838559b", result);
  tlib_pass_if_int_equal ("empty transaction name", 1, nro_getsize (txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null ("empty transaction name", nro_get_hash_value (txn->cat.alternate_path_hashes, "2838559b", NULL));
  nr_free (result);

  /*
   * Test : Non-empty transaction name.
   */
  txn->name = nr_strdup ("txn");
  result = nr_txn_get_path_hash (txn);
  tlib_pass_if_str_equal ("transaction name", "e7e6b10a", result);
  tlib_pass_if_int_equal ("transaction name", 2, nro_getsize (txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null ("transaction name", nro_get_hash_value (txn->cat.alternate_path_hashes, "e7e6b10a", NULL));
  nr_free (result);

  /*
   * Test : Referring path hash.
   */
  txn->cat.referring_path_hash = nr_strdup ("e7e6b10a");
  result = nr_txn_get_path_hash (txn);
  tlib_pass_if_str_equal ("referring path hash", "282bd31f", result);
  tlib_pass_if_int_equal ("referring path hash", 3, nro_getsize (txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null ("referring path hash", nro_get_hash_value (txn->cat.alternate_path_hashes, "282bd31f", NULL));
  nr_free (result);

  nro_delete (txn->cat.alternate_path_hashes);
  nr_free (txn->cat.referring_path_hash);
  nr_free (txn->name);
  nr_free (txn->primary_app_name);
}

static void
test_get_primary_app_name (void)
{
  char *result;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null ("NULL appname", nr_txn_get_primary_app_name (NULL));
  tlib_pass_if_null ("empty appname", nr_txn_get_primary_app_name (""));

  /*
   * Test : No rollup.
   */
  result = nr_txn_get_primary_app_name ("App Name");
  tlib_pass_if_str_equal ("no rollup", "App Name", result);
  nr_free (result);

  /*
   * Test : Rollup.
   */
  result = nr_txn_get_primary_app_name ("App Name;Foo;Bar");
  tlib_pass_if_str_equal ("rollup", "App Name", result);
  nr_free (result);
}

static void
test_is_synthetics (void)
{
  nrtxn_t txn;

  tlib_pass_if_int_equal ("null txn", 0, nr_txn_is_synthetics (NULL));
  txn.type = 0;
  tlib_pass_if_int_equal ("zero type", 0, nr_txn_is_synthetics (&txn));
  txn.type = NR_TXN_TYPE_SYNTHETICS;
  tlib_pass_if_int_equal ("only synthetics", 1, nr_txn_is_synthetics (&txn));
  txn.type = NR_TXN_TYPE_SYNTHETICS | NR_TXN_TYPE_CAT_INBOUND;
  tlib_pass_if_int_equal ("synthetics and cat", 1, nr_txn_is_synthetics (&txn));
}

static void
test_start_time_secs (void)
{
  nrtxn_t txn;

  txn.root.start_time.when = 123456789 * NR_TIME_DIVISOR_US;

  tlib_pass_if_double_equal ("NULL txn", nr_txn_start_time_secs (NULL), 0.0);
  tlib_pass_if_double_equal ("success", nr_txn_start_time_secs (&txn), 123.456789);
}

static void
test_start_time (void)
{
  nrtxn_t txn;

  txn.root.start_time.when = 123 * NR_TIME_DIVISOR;

  tlib_pass_if_time_equal ("NULL txn", nr_txn_start_time (NULL), 0);
  tlib_pass_if_time_equal ("success", nr_txn_start_time (&txn), 123 * NR_TIME_DIVISOR);
}

static nrtxn_t *
test_namer_with_app_and_expressions_and_return_txn (const char *test_name,
  const char *test_pattern, const char *test_filename, const char *expected_match)
{
  nrtxn_t *txn;
  nrapp_t simple_test_app;

  nr_memset (&simple_test_app, 0, sizeof (simple_test_app));
  simple_test_app.state = NR_APP_OK;

  txn = nr_txn_begin (&simple_test_app, &nr_txn_test_options, NULL);
  tlib_pass_if_not_null ("nr_txn_begin succeeds", txn);

  nr_txn_add_match_files (txn, test_pattern);
  nr_txn_match_file (txn, test_filename);
  tlib_pass_if_str_equal (test_name, expected_match, txn->path);

  return txn;
}

static void
test_namer_with_app_and_expressions (const char *test_name,
  const char *test_pattern, const char *test_filename, const char *expected_match)
{
  nrtxn_t *txn;

  txn = test_namer_with_app_and_expressions_and_return_txn  (test_name, test_pattern, test_filename, expected_match);

  nr_txn_destroy (&txn);
  tlib_pass_if_null ("Failed to destroy txn?", txn);
}

static void
test_namer (void)
{
  nrtxn_t *txn = NULL;
  nrapp_t simple_test_app;

  /* apparently named initializers don't work properly in C++. */
  nr_memset (&simple_test_app, 0, sizeof (simple_test_app));
  simple_test_app.state = NR_APP_OK;

  /* Mostly just exercising code paths and checking for segfaults. */
  nr_txn_match_file (NULL, "");
  nr_txn_match_file (NULL, NULL);
  nr_txn_add_file_naming_pattern (NULL, "");

  txn = nr_txn_begin (&simple_test_app, &nr_txn_test_options, NULL);
  nr_txn_add_file_naming_pattern (txn, NULL);
  nr_txn_add_file_naming_pattern (txn, "");

  nr_txn_match_file (txn, "pattern/pattern-pattern");
  tlib_pass_if_null ("No match with no matchers", txn->path);
  nr_txn_add_match_files (txn, "");
  tlib_pass_if_null ("Empty string doesn't add to txn namers", txn->match_filenames);
  nr_txn_match_file (txn, NULL);
  tlib_pass_if_null ("Doesn't match NULL", txn->path);
  nr_txn_match_file (txn, "");
  tlib_pass_if_null ("Nothing in matcher doesn't match empty string", txn->path);

  nr_txn_add_match_files (txn, "pattern");
  nr_txn_match_file (txn, "");
  tlib_pass_if_null ("No match with empty string", txn->path);
  nr_txn_match_file (txn, NULL);
  tlib_pass_if_null ("No match with NULL", txn->path);

  nr_txn_destroy (&txn);

  /* regexes. */
  test_namer_with_app_and_expressions ("All nulls doesn't match.", NULL, NULL, NULL);
  test_namer_with_app_and_expressions ("No pattern to match doesn't match", NULL, "include/foo.php", NULL);
  test_namer_with_app_and_expressions ("No pattern doesn't match empty string", NULL, "", NULL);
  test_namer_with_app_and_expressions ("Last expression matches first", "foo,bar,f.", "foo", "fo");
  test_namer_with_app_and_expressions ("Matches in path", "include", "var/include/bar/foo", "include");
  test_namer_with_app_and_expressions ("Directory matching", "include/", "include/.", "include/.");
  test_namer_with_app_and_expressions ("Directory matching", "include/", "include/..", "include/..");
  /* vvv  this is the weird one. Old behavior. vvv */
  test_namer_with_app_and_expressions ("Directory matching", "include/", "include/...", "include/...");
  test_namer_with_app_and_expressions ("Directory matching", "include", "include/...", "include");
  test_namer_with_app_and_expressions ("Basic regex 0", "f[a-z]+\\d{2}", "fee23", "fee23");
  test_namer_with_app_and_expressions ("Basic regex 1", "f[a-z]+.*5", "fee23954", "fee2395");
  test_namer_with_app_and_expressions ("Basic regex 2", "f[a-z]+\\d{2}", "f23954", NULL);
  test_namer_with_app_and_expressions ("Basic regex 3", "f[a-z]+\\d*/bee", "file99/bee/honey.php", "file99/bee");

  /* Mostly introspection. */
  txn = test_namer_with_app_and_expressions_and_return_txn  ("Look inside the txn after setting", "p.,bla,pkg/", "pkg/./bla/pip.php", "pkg/.");
  nr_txn_match_file (txn, "blabulous.php");
  tlib_pass_if_str_equal ("Match freezes transaction", "pkg/.", txn->path);
  nr_txn_match_file (txn, "park");
  tlib_pass_if_str_equal ("Match freezes transaction", "pkg/.", txn->path);
  nr_txn_destroy (&txn);

  txn = nr_txn_begin (&simple_test_app, &nr_txn_test_options, NULL);

  txn->status.recording = 0;
  nr_txn_add_match_files (txn, "pattern");
  nr_txn_match_file (txn, "pattern/pattern-pattern");
  tlib_pass_if_null ("status.recording == 0 causes name freeze", txn->path);
  txn->status.recording = 1;

  txn->status.path_type = NR_PATH_TYPE_ACTION;
  nr_txn_match_file (txn, "pattern/pattern-pattern");
  tlib_pass_if_null ("status.path_type == NR_PATH_TYPE_ACTION causes name freeze", txn->path);
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  txn->status.path_is_frozen = 1;
  nr_txn_match_file (txn, "pattern/pattern-pattern");
  tlib_pass_if_null ("Setting path_is_frozen causes path not to be updated", txn->path);
  txn->status.path_is_frozen = 0;

  nr_txn_destroy (&txn);
}

static void
test_error_to_event (void)
{
  nrtxn_t txn;
  nr_analytics_event_t *event;

  txn.cat.inbound_guid = NULL;
  txn.error = nr_error_create (1, "the_msg", "the_klass", "[]", 123 * NR_TIME_DIVISOR);
  txn.guid = nr_strdup ("abcd");
  txn.name = nr_strdup ("my_txn_name");
  txn.options.analytics_events_enabled = 1;
  txn.options.error_events_enabled = 1;
  txn.options.apdex_t = 10;
  txn.root.start_time.when = 123 * NR_TIME_DIVISOR;
  txn.root.stop_time.when = txn.root.start_time.when + 987 * NR_TIME_DIVISOR_MS;
  txn.status.background = 0;
  txn.status.ignore_apdex = 0;
  txn.synthetics = NULL;
  txn.type = 0;
  txn.unscoped_metrics = nrm_table_create (100);

  txn.attributes = nr_attributes_create (0);
  nr_attributes_user_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_ERROR, "user_long", 1);
  nr_attributes_agent_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_ERROR, "agent_long", 2);
  nr_attributes_user_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL &~ NR_ATTRIBUTE_DESTINATION_ERROR, "NOPE", 1);
  nr_attributes_agent_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL &~ NR_ATTRIBUTE_DESTINATION_ERROR, "NOPE", 2);

  event = nr_error_to_event (0);
  tlib_pass_if_null ("null txn", event);

  txn.options.error_events_enabled = 0;
  event = nr_error_to_event (&txn);
  tlib_pass_if_null ("error events disabled", event);
  txn.options.error_events_enabled = 1;

  event = nr_error_to_event (&txn);
  tlib_pass_if_str_equal ("no metric parameters", nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"TransactionError\","
        "\"timestamp\":123.00000,"
        "\"error.class\":\"the_klass\","
        "\"error.message\":\"the_msg\","
        "\"transactionName\":\"my_txn_name\","
        "\"duration\":0.98700,"
        "\"nr.transactionGuid\":\"abcd\""
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);

  nrm_add (txn.unscoped_metrics, "Datastore/all",         1 * NR_TIME_DIVISOR);
  nrm_add (txn.unscoped_metrics, "External/all",          2 * NR_TIME_DIVISOR);
  nrm_add (txn.unscoped_metrics, "WebFrontend/QueueTime", 3 * NR_TIME_DIVISOR);

  event = nr_error_to_event (&txn);
  tlib_pass_if_str_equal ("all metric parameters",
    nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"TransactionError\","
        "\"timestamp\":123.00000,"
        "\"error.class\":\"the_klass\","
        "\"error.message\":\"the_msg\","
        "\"transactionName\":\"my_txn_name\","
        "\"duration\":0.98700,"
        "\"queueDuration\":3.00000,"
        "\"externalDuration\":2.00000,"
        "\"databaseDuration\":1.00000,"
        "\"databaseCallCount\":1,"
        "\"externalCallCount\":1,"
        "\"nr.transactionGuid\":\"abcd\""
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);

  txn.synthetics = nr_synthetics_create ("[1,100,\"a\",\"b\",\"c\"]");
  txn.cat.inbound_guid = nr_strdup ("foo_guid");
  event = nr_error_to_event (&txn);
  tlib_pass_if_str_equal ("synthetics txn",
    nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"TransactionError\","
        "\"timestamp\":123.00000,"
        "\"error.class\":\"the_klass\","
        "\"error.message\":\"the_msg\","
        "\"transactionName\":\"my_txn_name\","
        "\"duration\":0.98700,"
        "\"queueDuration\":3.00000,"
        "\"externalDuration\":2.00000,"
        "\"databaseDuration\":1.00000,"
        "\"databaseCallCount\":1,"
        "\"externalCallCount\":1,"
        "\"nr.transactionGuid\":\"abcd\","
        "\"nr.referringTransactionGuid\":\"foo_guid\","
        "\"nr.syntheticsResourceId\":\"a\","
        "\"nr.syntheticsJobId\":\"b\","
        "\"nr.syntheticsMonitorId\":\"c\""
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);

  nr_free (txn.guid);
  nr_free (txn.name);
  nr_free (txn.cat.inbound_guid);
  nr_error_destroy (&txn.error);
  nrm_table_destroy (&txn.unscoped_metrics);
  nr_attributes_destroy (&txn.attributes);
  nr_synthetics_destroy (&txn.synthetics);
}

static void
test_create_event (void)
{
  nrtxn_t txn;
  nr_analytics_event_t *event;

  txn.error = NULL;
  txn.status.background = 0;
  txn.status.ignore_apdex = 0;
  txn.options.analytics_events_enabled = 1;
  txn.options.apdex_t = 10;
  txn.guid = nr_strdup ("abcd");
  txn.name = nr_strdup ("my_txn_name");
  txn.root.start_time.when = 123 * NR_TIME_DIVISOR;
  txn.root.stop_time.when = txn.root.start_time.when + 987 * NR_TIME_DIVISOR_MS;
  txn.unscoped_metrics = nrm_table_create (100);
  txn.synthetics = NULL;
  txn.type = 0;
  txn.async_duration = 0;

  txn.attributes = nr_attributes_create (0);
  nr_attributes_user_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "user_long", 1);
  nr_attributes_agent_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "agent_long", 2);
  nr_attributes_user_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL &~ NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "NOPE", 1);
  nr_attributes_agent_add_long (txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL &~ NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "NOPE", 2);

  event = nr_txn_to_event (0);
  tlib_pass_if_null ("null txn", event);

  txn.options.analytics_events_enabled = 0;
  event = nr_txn_to_event (&txn);
  tlib_pass_if_null ("analytics event disabled", event);
  txn.options.analytics_events_enabled = 1;

  event = nr_txn_to_event (&txn);
  tlib_pass_if_str_equal ("no metric parameters", nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"Transaction\","
        "\"name\":\"my_txn_name\","
        "\"timestamp\":123.00000,"
        "\"duration\":0.98700,"
        "\"totalTime\":0.98700,"
        "\"nr.apdexPerfZone\":\"F\""
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);

  nrm_add (txn.unscoped_metrics, "Datastore/all",         1 * NR_TIME_DIVISOR);
  nrm_add (txn.unscoped_metrics, "External/all",          2 * NR_TIME_DIVISOR);
  nrm_add (txn.unscoped_metrics, "WebFrontend/QueueTime", 3 * NR_TIME_DIVISOR);

  event = nr_txn_to_event (&txn);
  tlib_pass_if_str_equal ("all metric parameters",
    nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"Transaction\","
        "\"name\":\"my_txn_name\","
        "\"timestamp\":123.00000,"
        "\"duration\":0.98700,"
        "\"totalTime\":0.98700,"
        "\"nr.apdexPerfZone\":\"F\","
        "\"queueDuration\":3.00000,"
        "\"externalDuration\":2.00000,"
        "\"databaseDuration\":1.00000"
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);

  txn.status.background = 1;
  event = nr_txn_to_event (&txn);
  tlib_pass_if_str_equal ("background tasks also make events",
    nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"Transaction\","
        "\"name\":\"my_txn_name\","
        "\"timestamp\":123.00000,"
        "\"duration\":0.98700,"
        "\"totalTime\":0.98700,"
        "\"queueDuration\":3.00000,"
        "\"externalDuration\":2.00000,"
        "\"databaseDuration\":1.00000"
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);
  txn.status.background = 0;

  txn.type = NR_TXN_TYPE_SYNTHETICS;
  event = nr_txn_to_event (&txn);
  tlib_pass_if_str_equal ("synthetics txn (note guid!)",
    nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"Transaction\","
        "\"name\":\"my_txn_name\","
        "\"timestamp\":123.00000,"
        "\"duration\":0.98700,"
        "\"totalTime\":0.98700,"
        "\"nr.guid\":\"abcd\","
        "\"nr.apdexPerfZone\":\"F\","
        "\"queueDuration\":3.00000,"
        "\"externalDuration\":2.00000,"
        "\"databaseDuration\":1.00000"
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);
  txn.type = 0;

  txn.async_duration = 333 * NR_TIME_DIVISOR_MS;
  event = nr_txn_to_event (&txn);
  tlib_pass_if_str_equal ("totalTime > duration",
    nr_analytics_event_json (event),
    "["
      "{"
        "\"type\":\"Transaction\","
        "\"name\":\"my_txn_name\","
        "\"timestamp\":123.00000,"
        "\"duration\":0.98700,"
        "\"totalTime\":1.32000,"
        "\"nr.apdexPerfZone\":\"F\","
        "\"queueDuration\":3.00000,"
        "\"externalDuration\":2.00000,"
        "\"databaseDuration\":1.00000"
      "},"
      "{\"user_long\":1},"
      "{\"agent_long\":2}"
    "]");
  nr_analytics_event_destroy (&event);

  nr_free (txn.guid);
  nr_free (txn.name);
  nr_attributes_destroy (&txn.attributes);
  nrm_table_destroy (&txn.unscoped_metrics);
}

static void
test_name_from_function (void)
{
  nrtxn_t txn;

  txn.status.path_is_frozen = 0;
  txn.status.path_type = NR_PATH_TYPE_UNKNOWN;
  txn.path = NULL;

  /* Bad params */
  nr_txn_name_from_function (NULL, NULL, NULL);
  nr_txn_name_from_function (NULL, "my_func", "my_class");
  nr_txn_name_from_function (&txn, NULL, "my_class");
  tlib_pass_if_null ("bad params", txn.path);
  tlib_pass_if_int_equal ("bad params", (int)txn.status.path_type, (int)NR_PATH_TYPE_UNKNOWN);

  /* only function name */
  nr_txn_name_from_function (&txn, "my_func", NULL);
  tlib_pass_if_str_equal ("only function name", txn.path, "my_func");
  tlib_pass_if_int_equal ("only function name", (int)txn.status.path_type, (int)NR_PATH_TYPE_FUNCTION);
  nr_txn_name_from_function (&txn, "other_func", NULL);
  tlib_pass_if_str_equal ("not replaced", txn.path, "my_func");
  tlib_pass_if_int_equal ("not replaced", (int)txn.status.path_type, (int)NR_PATH_TYPE_FUNCTION);

  nr_free (txn.path);
  txn.status.path_type = NR_PATH_TYPE_UNKNOWN;

  /* with class name */
  nr_txn_name_from_function (&txn, "my_func", "my_class");
  tlib_pass_if_str_equal ("with class name", txn.path, "my_class::my_func");
  tlib_pass_if_int_equal ("with class name", (int)txn.status.path_type, (int)NR_PATH_TYPE_FUNCTION);
  nr_txn_name_from_function (&txn, "other_func", NULL);
  tlib_pass_if_str_equal ("not replaced", txn.path, "my_class::my_func");
  tlib_pass_if_int_equal ("not replaced", (int)txn.status.path_type, (int)NR_PATH_TYPE_FUNCTION);

  /* doesnt override higher priority name */
  nr_txn_set_path (NULL, &txn, "api", NR_PATH_TYPE_CUSTOM, NR_OK_TO_OVERWRITE);
  nr_txn_name_from_function (&txn, "my_func", "my_class");
  tlib_pass_if_str_equal ("higher priority name", txn.path, "api");
  tlib_pass_if_int_equal ("higher priority name", (int)txn.status.path_type, (int)NR_PATH_TYPE_CUSTOM);

  nr_free (txn.path);
}

static void
test_txn_ignore (void)
{
  nrtxn_t txn;

  nr_txn_ignore (NULL);

  txn.status.ignore = 0;
  txn.status.recording = 1;

  nr_txn_ignore (&txn);

  tlib_pass_if_int_equal ("nr_txn_ignore sets ignore", txn.status.ignore, 1);
  tlib_pass_if_int_equal ("nr_txn_ignore sets recording", txn.status.recording, 0);
}

static void
test_add_custom_metric (void)
{
  nrtxn_t txn;
  double value_ms = 123.45;
  char *json;

  txn.unscoped_metrics = nrm_table_create (NR_METRIC_DEFAULT_LIMIT);
  txn.status.recording = 1;

  tlib_pass_if_status_failure ("null params", nr_txn_add_custom_metric (NULL, NULL, value_ms));
  tlib_pass_if_status_failure ("null name", nr_txn_add_custom_metric (&txn, NULL, value_ms));
  tlib_pass_if_status_failure ("null txn", nr_txn_add_custom_metric (NULL, "my_metric", value_ms));

  tlib_pass_if_status_failure ("NAN", nr_txn_add_custom_metric (&txn, "my_metric", NAN));
  tlib_pass_if_status_failure ("INFINITY", nr_txn_add_custom_metric (&txn, "my_metric", INFINITY));

  txn.status.recording = 0;
  tlib_pass_if_status_failure ("not recording", nr_txn_add_custom_metric (&txn, "my_metric", value_ms));
  txn.status.recording = 1;

  tlib_pass_if_status_success ("custom metric success", nr_txn_add_custom_metric (&txn, "my_metric", value_ms));
  json = nr_metric_table_to_daemon_json (txn.unscoped_metrics);
  tlib_pass_if_str_equal ("custom metric success", json,
    "[{\"name\":\"my_metric\",\"data\":[1,0.12345,0.12345,0.12345,0.12345,0.01524]}]");
  nr_free (json);

  nrm_table_destroy (&txn.unscoped_metrics);
}

static nr_status_t
test_txn_cat_map_expected_intrinsics (const char *key, const nrobj_t *val, void *ptr)
{
  const nrobj_t *intrinsics = (const nrobj_t *)ptr;
  const char *expected = nro_get_string (val, NULL);
  const char *found = nro_get_hash_string (intrinsics, key, NULL);

  tlib_pass_if_true ("expected intrinsics", 0 == nr_strcmp (expected, found),
    "key='%s' expected='%s' found='%s'",
    NRSAFESTR (key), NRSAFESTR (expected), NRSAFESTR (found));

  return NR_SUCCESS;
}

static void
test_txn_cat_map_cross_agent_testcase (nrapp_t *app, const nrobj_t *hash)
{
  int i;
  int size;
  nrtxn_t *txn;
  const char *testname = nro_get_hash_string (hash, "name", 0);
  const char *appname = nro_get_hash_string (hash, "appName", 0);
  const char *txnname = nro_get_hash_string (hash, "transactionName", 0);
  const char *guid = nro_get_hash_string (hash, "transactionGuid", 0);
  const nrobj_t *inbound_x_newrelic_txn = nro_get_hash_value (hash, "inboundPayload", NULL);
  const nrobj_t *outbound = nro_get_hash_array (hash, "outboundRequests", NULL);
  const nrobj_t *expected_intrinsics = nro_get_hash_hash (hash, "expectedIntrinsicFields", NULL);
  const nrobj_t *missing_intrinsics = nro_get_hash_array (hash, "nonExpectedIntrinsicFields", NULL);
  nrobj_t *intrinsics;

  nr_free (app->info.appname);
  app->info.appname = nr_strdup (appname);

  txn = nr_txn_begin (app, &nr_txn_test_options, NULL);
  tlib_pass_if_not_null ("tests valid", txn);
  if (NULL == txn) {
    return;
  }

  nr_free (txn->guid);
  txn->guid = nr_strdup (guid);

  nr_header_process_x_newrelic_transaction (txn, inbound_x_newrelic_txn);

  size = nro_getsize (outbound);
  for (i = 1; i <= size; i++) {
    const nrobj_t *outbound_request = nro_get_array_hash (outbound, i, NULL);
    const char *outbound_txn_name = nro_get_hash_string (outbound_request, "outboundTxnName", NULL);
    const nrobj_t *payload = nro_get_hash_value (outbound_request, "expectedOutboundPayload", NULL);

    nr_free (txn->path);
    txn->path = nr_strdup (outbound_txn_name);

    {
      char *expected = nro_to_json (payload);
      char *decoded_x_newrelic_id = NULL;
      char *decoded_x_newrelic_txn = NULL;

      nr_header_outbound_request_decoded (txn,
        &decoded_x_newrelic_id,
        &decoded_x_newrelic_txn);

      tlib_pass_if_str_equal (testname, expected, decoded_x_newrelic_txn);

      nr_free (expected);
      nr_free (decoded_x_newrelic_id);
      nr_free (decoded_x_newrelic_txn);
    }
  }

  txn->status.path_is_frozen = 1;
  nr_free (txn->name);
  txn->name = nr_strdup (txnname);

  intrinsics = nr_txn_event_intrinsics (txn);

  /*
   * Test absence of non-expected intrinsic fields.
   */
  size = nro_getsize (missing_intrinsics);
  for (i = 1; i <= size; i++) {
    const char *key = nro_get_array_string (missing_intrinsics, i, NULL);
    const nrobj_t *val = nro_get_hash_value (intrinsics, key, NULL);

    tlib_pass_if_true (testname, 0 == val, "key='%s'", NRSAFESTR (key));
  }

  /*
   * Test presence of expected intrinsics.
   */
  nro_iteratehash (expected_intrinsics, test_txn_cat_map_expected_intrinsics, intrinsics);

  nro_delete (intrinsics);

  nr_txn_destroy (&txn);
}

static void
test_txn_cat_map_cross_agent_tests (void)
{
  char *json = 0;
  nrobj_t *array = 0;
  nrotype_t otype;
  int i;
  nrapp_t app;
  int size;

  nr_memset (&app, 0, sizeof (app));
  app.state = NR_APP_OK;
  app.connect_reply = nro_create_from_json ("{\"cross_process_id\":\"my_cross_process_id\"}");

  json = nr_read_file_contents (CROSS_AGENT_TESTS_DIR "/cat/cat_map.json", 10 * 1000 * 1000);
  array = nro_create_from_json (json);
  otype = nro_type (array);
  tlib_pass_if_int_equal ("tests valid", (int)NR_OBJECT_ARRAY, (int)otype);

  size = nro_getsize (array);
  for (i = 1; i <= size; i++) {
    const nrobj_t *hash = nro_get_array_hash (array, i, 0);

    test_txn_cat_map_cross_agent_testcase (&app, hash);
  }

  nr_free (app.info.appname);
  nro_delete (app.connect_reply);
  nro_delete (array);
  nr_free (json);
}

static void
test_force_single_count (void)
{
  nrtxn_t txn;
  const char *name = "Supportability/InstrumentedFunction/zip::zap";

  nr_txn_force_single_count (NULL, NULL);
  nr_txn_force_single_count (NULL, name);

  txn.unscoped_metrics = nrm_table_create (10);

  nr_txn_force_single_count (&txn, NULL);
  tlib_pass_if_int_equal ("no metric name", 0, nrm_table_size (txn.unscoped_metrics));

  nr_txn_force_single_count (&txn, name);
  tlib_pass_if_int_equal ("metric created", 1, nrm_table_size (txn.unscoped_metrics));
  test_txn_metric_created ("metric created", txn.unscoped_metrics, MET_FORCED, name, 1, 0, 0, 0, 0, 0);

  nrm_table_destroy (&txn.unscoped_metrics);
}

static void
test_fn_supportability_metric (void)
{
  char *name;

  name = nr_txn_create_fn_supportability_metric (NULL, NULL);
  tlib_pass_if_str_equal ("null params", name, "Supportability/InstrumentedFunction/");
  nr_free (name);

  name = nr_txn_create_fn_supportability_metric ("zip::zap", NULL);
  tlib_pass_if_str_equal ("full name as first parameter", name, "Supportability/InstrumentedFunction/zip::zap");
  nr_free (name);

  name = nr_txn_create_fn_supportability_metric ("zip", NULL);
  tlib_pass_if_str_equal ("only function name", name, "Supportability/InstrumentedFunction/zip");
  nr_free (name);

  name = nr_txn_create_fn_supportability_metric ("zap", "zip");
  tlib_pass_if_str_equal ("function name and class name", name, "Supportability/InstrumentedFunction/zip::zap");
  nr_free (name);
}

static void
test_txn_set_attribute (void)
{
  nrtxn_t txn;
  char *json;

  txn.attributes = nr_attributes_create (0);

  nr_txn_set_string_attribute (NULL, NULL, NULL);
  nr_txn_set_string_attribute (NULL, nr_txn_request_user_agent, "user agent");
  nr_txn_set_string_attribute (&txn, NULL,                      "user agent");
  nr_txn_set_string_attribute (&txn, nr_txn_request_user_agent, NULL);
  nr_txn_set_string_attribute (&txn, nr_txn_request_user_agent, "");

  nr_txn_set_long_attribute (NULL, NULL, 0);
  nr_txn_set_long_attribute (NULL, nr_txn_request_content_length, 123);
  nr_txn_set_long_attribute (&txn, NULL, 123);

  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_str_equal ("bad params", json, "{\"user\":[],\"agent\":[]}");
  nr_free (json);

  nr_txn_set_string_attribute (&txn, nr_txn_request_user_agent,     "1");
  nr_txn_set_string_attribute (&txn, nr_txn_request_accept_header,  "2");
  nr_txn_set_string_attribute (&txn, nr_txn_request_host,           "3");
  nr_txn_set_string_attribute (&txn, nr_txn_request_content_type,   "4");
  nr_txn_set_string_attribute (&txn, nr_txn_request_method,         "5");
  nr_txn_set_string_attribute (&txn, nr_txn_server_name,            "6");
  nr_txn_set_string_attribute (&txn, nr_txn_response_content_type,  "7");

  nr_txn_set_long_attribute (&txn, nr_txn_request_content_length,  123);
  nr_txn_set_long_attribute (&txn, nr_txn_response_content_length, 456);

  json = nr_attributes_debug_json (txn.attributes);
  tlib_pass_if_str_equal ("attributes successfully added", json,
  "{\"user\":[],\"agent\":["
  "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"response.headers.contentLength\",\"value\":456},"
  "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"request.headers.contentLength\",\"value\":123},"
  "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"response.headers.contentType\",\"value\":\"7\"},"
  "{\"dests\":[\"trace\",\"error\"],\"key\":\"SERVER_NAME\",\"value\":\"6\"},"
  "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"request.method\",\"value\":\"5\"},"
  "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"request.headers.contentType\",\"value\":\"4\"},"
  "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"request.headers.host\",\"value\":\"3\"},"
  "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":\"request.headers.accept\",\"value\":\"2\"},"
  "{\"dests\":[\"trace\",\"error\"],\"key\":\"request.headers.User-Agent\",\"value\":\"1\"}]}");
  nr_free (json);

  nr_attributes_destroy (&txn.attributes);
}

static void
test_sql_recording_level (void)
{
  nr_tt_recordsql_t level;
  nrtxn_t *txn = new_txn (0);

  level = nr_txn_sql_recording_level (NULL);
  tlib_pass_if_equal ("NULL pointer returns NR_SQL_NONE", NR_SQL_NONE, level, nr_tt_recordsql_t, "%d");

  txn->high_security = 0;
  txn->options.tt_recordsql = NR_SQL_RAW;
  level = nr_txn_sql_recording_level (txn);
  tlib_pass_if_equal ("Raw recording level", NR_SQL_RAW, level, nr_tt_recordsql_t, "%d");

  txn->high_security = 1;
  level = nr_txn_sql_recording_level (txn);
  tlib_pass_if_equal ("High security overrides raw SQL mode", NR_SQL_OBFUSCATED, level, nr_tt_recordsql_t, "%d");

  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;
  txn->high_security = 0;
  level = nr_txn_sql_recording_level (txn);
  tlib_pass_if_equal ("Obfuscated SQL with no high security mode", NR_SQL_OBFUSCATED, level, nr_tt_recordsql_t, "%d");

  txn->high_security = 1;
  level = nr_txn_sql_recording_level (txn);
  tlib_pass_if_equal ("Obfuscated SQL with high security mode", NR_SQL_OBFUSCATED, level, nr_tt_recordsql_t, "%d");

  txn->options.tt_recordsql = NR_SQL_NONE;
  txn->high_security = 0;
  level = nr_txn_sql_recording_level (txn);
  tlib_pass_if_equal ("SQL recording disabled, no high security mode.", NR_SQL_NONE, level, nr_tt_recordsql_t, "%d");

  txn->high_security = 1;
  level = nr_txn_sql_recording_level (txn);
  tlib_pass_if_equal ("SQL recording disabled, high security mode.", NR_SQL_NONE, level, nr_tt_recordsql_t, "%d");

  nr_txn_destroy (&txn);
}

static void
test_nr_txn_is_current_path_named (void)
{
    const char *path_match = "/foo/baz/bar";
    const char *path_not_match = "/not/matched/path";
    nrtxn_t txn;

    nr_memset (&txn, 0, sizeof (txn));

    txn.path = nr_strdup (path_match);    

    tlib_pass_if_true (__func__, 
      nr_txn_is_current_path_named (&txn, path_match), 
      "path=%s,txn->path=%s", path_match, txn.path);
      
    tlib_pass_if_false (__func__, 
      nr_txn_is_current_path_named (&txn, path_not_match), 
      "path=%s,txn->path=%s", path_not_match, txn.path);

    tlib_pass_if_false (__func__, 
      nr_txn_is_current_path_named (&txn, NULL), 
      "path=%s,txn->path=%s", path_not_match, txn.path);

    tlib_pass_if_false (__func__, 
      nr_txn_is_current_path_named (NULL, path_not_match), 
      "path=%s,txn->path=%s", path_not_match, txn.path);

    tlib_pass_if_false (__func__, 
      nr_txn_is_current_path_named (NULL, NULL), 
      "path=%s,txn->path=%s", path_not_match, txn.path);
                                  
    nr_txn_destroy_fields (&txn);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = sizeof (test_txn_state_t)};

void
test_main (void *p NRUNUSED)
{
  test_txn_cmp_options ();
  test_freeze_name_update_apdex ();
  test_create_apdex_metrics ();
  test_create_error_metrics ();
  test_create_duration_metrics ();
  test_create_duration_metrics_async ();
  test_create_queue_metric ();
  test_set_path ();
  test_set_request_uri ();
  test_record_error_worthy ();
  test_record_error ();
  test_save_if_slow_enough ();
  test_begin_bad_params ();
  test_begin ();
  test_end ();
  test_create_guid ();
  test_should_force_persist ();
  test_save_trace_node ();
  test_save_trace_node_async ();
  test_set_as_background_job ();
  test_set_as_web_transaction ();
  test_set_http_status ();
  test_add_user_custom_parameter ();
  test_add_request_parameter ();
  test_set_stop_time ();
  test_set_request_referer ();
  test_set_request_content_length ();
  test_add_error_attributes ();
  test_duration ();
  test_queue_time ();
  test_set_queue_start ();
  test_create_rollup_metrics ();
  test_record_custom_event ();
  test_is_account_trusted ();
  test_should_save_trace ();
  test_adjust_exclusive_time ();
  test_add_async_duration ();
  test_valid_node_end ();
  test_event_should_add_guid ();
  test_unfinished_duration ();
  test_should_create_apdex_metrics ();
  test_add_cat_analytics_intrinsics ();
  test_add_cat_intrinsics ();
  test_alternate_path_hashes ();
  test_apdex_zone ();
  test_get_cat_trip_id ();
  test_get_path_hash ();
  test_get_primary_app_name ();
  test_is_synthetics ();
  test_start_time ();
  test_start_time_secs ();
  test_namer ();
  test_error_to_event ();
  test_create_event ();
  test_name_from_function ();
  test_txn_ignore ();
  test_add_custom_metric ();
  test_txn_cat_map_cross_agent_tests ();
  test_force_single_count ();
  test_fn_supportability_metric ();
  test_txn_set_attribute ();
  test_sql_recording_level ();
  test_nr_txn_is_current_path_named ();
}
