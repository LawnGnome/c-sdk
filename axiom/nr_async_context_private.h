#ifndef NR_ASYNC_CONTEXT_PRIVATE_HDR
#define NR_ASYNC_CONTEXT_PRIVATE_HDR

struct _nr_async_context_t {
  nrtime_t start;           /* The start of the set of async operations */
  nrtime_t stop;            /* The end of the set of async operations */
  nrtime_t async_duration;  /* The cumulative duration of each async operation */
};

#endif /* NR_ASYNC_CONTEXT_PRIVATE_HDR */
