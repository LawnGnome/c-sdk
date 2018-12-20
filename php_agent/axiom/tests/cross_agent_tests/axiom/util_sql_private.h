/*
 * This file contains internal data structures for the sql interface
 */
#ifndef UTIL_SQL_PRIVATE_HDR
#define UTIL_SQL_PRIVATE_HDR

/*
 * This header file exposes internal functions that are only made visible for
 * unit testing. Other clients are forbidden.
 */

extern const char* nr_sql_whitespace_comment_prefix(const char* sql,
                                                    int show_sql_parsing);

#endif /* UTIL_SQL_PRIVATE_HDR */
