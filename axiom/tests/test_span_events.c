#include "tlib_main.h"
#include "nr_span_events.h"
#include "nr_span_events_private.h"

static void test_span_events_create_destroy(void) {
    // create a few instances to make sure state stays separate
    // and destroy them to make sure any *alloc-y bugs are
    // caught by valgrind
    nr_span_events_t* ev1;
    nr_span_events_t* ev2;
    nr_span_events_t* null_ev = NULL;

    ev1 = nr_span_events_create();
    ev2 = nr_span_events_create();

    tlib_pass_if_not_null("create span events ev1", ev1);
    tlib_pass_if_not_null("create span events ev2", ev2);

    nr_span_events_destroy(&ev1);
    nr_span_events_destroy(&ev2);
    nr_span_events_destroy(&null_ev);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
    test_span_events_create_destroy();
}
