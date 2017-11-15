/*
 * This file contains analytics events data internals.
 */
#ifndef NR_ANALYTICS_EVENTS_PRIVATE_HDR
#define NR_ANALYTICS_EVENTS_PRIVATE_HDR

/*
 * This header file exposes internal functions that are only made visible for unit testing.
 * Other clients are forbidden.
 */

extern nr_analytics_event_t *nr_analytics_event_create_from_string (const char *str);

#endif /* NR_ANALYTICS_PRIVATE_HDR */
