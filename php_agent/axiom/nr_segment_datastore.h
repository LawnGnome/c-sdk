#ifndef NR_SEGMENT_DATASTORE_H
#define NR_SEGMENT_DATASTORE_H

#include "nr_datastore.h"
#include "nr_datastore_instance.h"
#include "nr_slowsqls.h"
#include "nr_txn.h"

// TODO: remove the #ifndef once we complete the nodeectomy.
#ifndef NODE_DATASTORE_HDR
typedef char* (*nr_backtrace_fn_t)(void);
typedef void (*nr_modify_table_name_fn_t)(char* table_name);
#endif

typedef struct _nr_segment_datastore_params_t {
  /*
   * Common fields to all datastore segments.
   */
  char* collection; /* The null-terminated collection; if NULL, this will be
                       extracted from the SQL for SQL segments. */
  char* operation;  /* The null-terminated operation; if NULL, this will be
                       extracted from the SQL for SQL segments. */
  nr_datastore_instance_t*
      instance; /* Any instance information that was collected. */

  /*
   * Datastore type fields.
   */
  struct {
    nr_datastore_t type; /* The datastore type that made the call. */
    char* string;        /* The datastore type as a string, if datastore is
                            NR_DATASTORE_OTHER. This field is ignored for other type
                            values. */
  } datastore;

  /*
   * These fields are only used for SQL datastore types.
   */
  struct {
    char* sql; /* The null-terminated SQL statement that was executed. */

    /*
     * The explain plan JSON for the SQL node, or NULL if no explain plan is
     * available. Note that this function does not check if explain plans are
     * enabled globally: this should be done before calling this function
     * (preferably, before even generating the explain plan!).
     */
    char* plan_json;

    /*
     * If a query language (such as DQL) was used to create the SQL, put that
     * command here.
     */
    nr_slowsqls_labelled_query_t* input_query;
  } sql;

  /*
   * Fields used to register callbacks.
   */
  struct {
    /* The function used to return a backtrace, if one is required for the
     * slowsql. This may be NULL. */
    nr_backtrace_fn_t backtrace;

    /* The function used to post-process the table (collection) name before it
     * is saved to the segment. This may be NULL.
     *
     * Note that the callback must modify the given name in place. (By
     * extension, it is impossible to modify a table name to be longer.)
     */
    nr_modify_table_name_fn_t modify_table_name;
  } callbacks;
} nr_segment_datastore_params_t;

/*
 * Purpose : Create and record metrics and a segment for a datastore call.
 *
 * Params : 1. The segment that will be created.
 *          2. The parameters listed above.
 *
 */
extern void nr_segment_datastore_end(nr_segment_t* segment,
                                     nr_segment_datastore_params_t* params);

#endif /* NR_SEGMENT_DATASTORE_HDR */
