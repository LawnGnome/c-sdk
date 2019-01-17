#ifndef NODE_DATASTORE_PRIVATE_HDR
#define NODE_DATASTORE_PRIVATE_HDR

/*
 * Purpose : Decide if an SQL node of the given duration would be considered as
 *           a potential slowsql.
 *
 * Params  : 1. The current transaction.
 *           2. The duration of the SQL query.
 *
 * Returns : Non-zero if the node is above the relevant threshold and explain
 *           plans are enabled; zero otherwise.
 */
extern int nr_txn_node_potential_slowsql(const nrtxn_t* txn, nrtime_t duration);

extern char* nr_txn_node_sql_get_operation_and_table(
    const nrtxn_t* txn,
    const char** operation_ptr,
    const char* sql,
    nr_modify_table_name_fn_t modify_table_name_fn);

#endif /* NODE_DATASTORE_PRIVATE_HDR */