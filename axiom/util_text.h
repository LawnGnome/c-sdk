/*
 * This file contains routines for scanning text in various formats.
 */
#ifndef UTIL_TEXT_HDR
#define UTIL_TEXT_HDR

#include <stddef.h>

#include "util_object.h"

/*
 * Reads the contents of a file, and returns the contents in a null terminated
 * string.
 *
 * Params : 1. The name of the file.
 *          2. The maximum size to read.
 *
 * Returns : NULL on some fault (non existant or unreadable file), or
 *           a pointer to newly allocated memory holding the file's contents.
 */
extern char* nr_read_file_contents(const char* file_name, size_t max_bytes);

/*
 * Purpose : Scan the given string looking for textual representations of
 *           key/value assignments.
 *
 *           The scanner looks for lines holding "hash rocket" style
 *           assignments:
 *             key => value
 *
 *           The expected format delimits lines by new line characters, and
 *           expects single space characters before and after the literal '=>'.
 *           Any other spaces (before or after the key and/or value) will be
 *           included in the key or value as appropriate.
 *
 *           This format is generally seen with plain text phpinfo() output.
 *
 * Params  : 1. The string to scan.
 *           2. The length of the string to scan.
 *           3. The object that will have the key/value pairs added to it.
 *
 * Warning : The input string will be modified in place: key and value strings
 *           will have their trailing space or newline replaced with null
 *           bytes.
 */
extern void nr_parse_rocket_assignment_list(char* s,
                                            size_t len,
                                            nrobj_t* kv_hash);

#endif /* UTIL_TEXT_HDR */
