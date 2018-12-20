/*
 * This file is used to process Datastore calls.
 */
#ifndef NODE_DATASTORE_HDR
#define NODE_DATASTORE_HDR

#include "nr_datastore.h"
#include "nr_txn.h"

/*
 * Function pointer types used in datastore node parameters for calling back
 * into the agent when extra information is required.
 */
typedef char* (*nr_backtrace_fn_t)(void);
typedef void (*nr_modify_table_name_fn_t)(char* table_name);

typedef struct _nr_node_datastore_params_t {
  nrtxntime_t start; /* The call start */
  nrtxntime_t stop;  /* The call stop */
  char* collection;  /* The null-terminated collection; if unset, this will be
                        extracted from the SQL */
  char* operation;   /* The null-terminated operation; if unset, this will be
                        extracted from the SQL */
  nr_datastore_instance_t*
      instance;        /* Any instance information that was collected */
  char* async_context; /* The async context, if any */

  struct {
    nr_datastore_t type; /* The datastore type that made the call */
    char* string;        /* The datastore type as a string, if datastore is
                            NR_DATASTORE_OTHER */
  } datastore;

  /*
   * These fields only make sense for SQL datastore types.
   */
  struct {
    char* sql; /* The null-terminated SQL statement that was executed */

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
   * Callback functions if more information is required.
   */
  struct {
    nr_backtrace_fn_t backtrace;
    nr_modify_table_name_fn_t modify_table_name;
  } callbacks;
} nr_node_datastore_params_t;

/*
 * Purpose : Record metrics and a transaction trace node for a datastore call.
 *
 * Params  : 1. The current transaction.
 *           2. The parameters listed above.
 *           3. An optional pointer to a location to store a copy of the
 *              scoped metric created for this call.
 */
extern void nr_txn_end_node_datastore(nrtxn_t* txn,
                                      const nr_node_datastore_params_t* params,
                                      char** return_scoped_metric);

/*
 * Purpose : Decide if an SQL node of the given duration would be considered as
 *           a potential explain plan.
 *
 * Params  : 1. The current transaction.
 *           2. The duration of the SQL query.
 *
 * Returns : Non-zero if the node is above the relevant threshold and explain
 *           plans are enabled; zero otherwise.
 */
extern int nr_txn_node_potential_explain_plan(const nrtxn_t* txn,
                                              nrtime_t duration);

/*
 * Purpose : Determine if the given node duration is long enough to trigger a
 *           slow SQL node.
 *
 * Params  : 1. The transaction pointer.
 *           2. The node duration.
 *
 * Returns : Non-zero if the node would trigger a node; zero otherwise.
 */
extern int nr_txn_node_datastore_stack_worthy(const nrtxn_t* txn,
                                              nrtime_t duration);

#endif /* NODE_DATASTORE_HDR */
