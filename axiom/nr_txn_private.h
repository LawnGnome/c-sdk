/*
 * This header file exposes internal functions that are only made visible for unit testing.
 * Other clients are forbidden.
 */
#ifndef NR_TXN_PRIVATE_HDR
#define NR_TXN_PRIVATE_HDR

#include "nr_txn.h"
#include "util_random.h"
#include "util_time.h"

extern char *nr_txn_create_guid (nr_random_t *rnd);
extern nrtxnnode_t *nr_txn_save_if_slow_enough (nrtxn_t *txn, nrtime_t duration);
extern void nr_txn_node_dispose_fields (nrtxnnode_t *node);
extern void nr_txn_create_rollup_metrics (nrtxn_t *txn);
extern void nr_txn_create_queue_metric (nrtxn_t *txn);
extern void nr_txn_create_duration_metrics (nrtxn_t *txn, const char *txnname, nrtime_t duration);
extern void nr_txn_create_error_metrics (nrtxn_t *txn, const char *txnname);
extern void nr_txn_create_apdex_metrics (nrtxn_t *txn, nrtime_t duration);
extern int nr_txn_pq_data_compare_wrapper (const nrtxnnode_t *a, const nrtxnnode_t *b);
extern void nr_txn_add_error_attributes (nrtxn_t *txn);
extern void nr_txn_record_custom_event_internal (nrtxn_t *txn, const char *type, const nrobj_t *params, nrtime_t now);

/* These sample options are provided for tests. */
extern const nrtxnopt_t nr_txn_test_options;

/*
 * Purpose : Adds CAT intrinsics to the transaction.
 */
extern void
nr_txn_add_cat_intrinsics (const nrtxn_t *txn, nrobj_t *intrinsics);

/*
 * Purpose : Add an alternative path hash to the list maintained in the
 *           transaction.
 *
 * Params  : 1. The transaction.
 *           2. The (possibly) new path hash.
 */
extern void
nr_txn_add_alternate_path_hash (nrtxn_t *txn, const char *path_hash);

/*
 * Purpose : Generate and return the current path hash for a transaction.
 *
 * Params  : 1. The transaction.
 *
 * Returns : The new path hash, which the caller is responsible for freeing.
 *
 * Note    : The key difference between this function and nr_txn_get_path_hash
 *           is that nr_txn_get_path_hash will also add the generated hash to
 *           the list of alternate path hashes, whereas this function only
 *           generates the hash but doesn't record it.
 */
extern char *
nr_txn_current_path_hash (const nrtxn_t *txn);

/*
 * Purpose : Return the alternative path hashes in the form expected by the
 *           collector (see also https://newrelic.jiveon.com/docs/DOC-1798).
 *
 * Params  : 1. The transaction.
 *
 * Returns : A newly allocated string containing the alternative path hashes,
 *           sorted and comma separated.
 */
extern char *
nr_txn_get_alternate_path_hashes (const nrtxn_t *txn);

/*
 * Purpose : Return the primary app name, given an app name string that may
 *           include rollups.
 *
 * Params  : 1. The app name(s) string.
 *
 * Returns : A newly allocated string containing the primary app name.
 */
extern char *
nr_txn_get_primary_app_name (const char *appname);

/*
 * Purpose : Free all transaction fields.  This is provided as a helper function
 *           for tests where the transaction is a local stack variable.
 */
extern void
nr_txn_destroy_fields (nrtxn_t *txn);

extern nrobj_t *
nr_txn_event_intrinsics (const nrtxn_t *txn);

#endif /* NR_TXN_PRIVATE_HDR */
