#include "util_strings.h"
#include "version.h"

/*
 * NEWRELIC_VERSION ultimately comes from the top-level VERSION file.
 */
#ifndef NEWRELIC_VERSION
#define NEWRELIC_VERSION "unreleased"
#endif

const char* newrelic_version(void) {
  return NR_STR2(NEWRELIC_VERSION);
}
