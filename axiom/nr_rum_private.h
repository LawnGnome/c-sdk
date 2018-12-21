/*
 * This file contains internal functions relating to real user monitoring.
 */
#ifndef NR_RUM_PRIVATE_HDR
#define NR_RUM_PRIVATE_HDR

/*
 * This header file exposes internal functions that are only made visible for
 * unit testing. Other clients are forbidden.
 */

/*
 * When obfuscating values for the RUM footer, do not use the entire
 * license string: Use this number of license characters instead.
 */
#define NR_RUM_OBFUSCATION_KEY_LENGTH 13

extern const char* nr_rum_scan_html_for_foot(const char* input,
                                             const uint input_len);
extern char* nr_rum_get_attributes(const nr_attributes_t* attributes);
extern nrtime_t nr_rum_get_app_time(const nrtxn_t* txn, nrtime_t now);

extern const char nr_rum_footer_prefix[];

#endif /* NR_RUM_PRIVATE_HDR */
