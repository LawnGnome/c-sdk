/*!
 * @file version.h
 *
 * @brief Function declarations necessary to support versioning the C agent.
 */
#ifndef LIBNEWRELIC_VERSION_H
#define LIBNEWRELIC_VERSION_H

/*!
 * @brief Return the string in this repository's top-level VERSION file
 *
 * @return The string in this repository's top-level VERSION file, i.e. the
 * stringified environment variable NEWRELIC_VERSION. If this file is missing or
 * empty, or the environment variable is not defined, return the string
 * "NEWRELIC_VERSION".
 */
const char* newrelic_version(void);

#endif /* LIBNEWRELIC_VERSION_H */
