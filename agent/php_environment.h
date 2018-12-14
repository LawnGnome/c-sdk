/*
 * This file contains a function that describes the environment.
 */
#ifndef PHP_ENVIRONMENT_HDR
#define PHP_ENVIRONMENT_HDR

/*
 * Purpose : Produce the object that describes the invariant parts of the
 *           execution environment.
 *
 */
extern nrobj_t* nr_php_get_environment(TSRMLS_D);

#endif /* PHP_ENVIRONMENT_HDR */
