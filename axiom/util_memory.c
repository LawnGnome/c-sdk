/*
 * Imlementation of memory utility functions.
 */
#include "nr_axiom.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "util_logging.h"
#include "util_memory.h"

#undef free

void
nr_realfree (void **oldptr)
{
  if ((0 == oldptr) || (0 == *oldptr)) {
    return;
  }
  (free) (*oldptr);
  *oldptr = 0;
}

void * NRMALLOC NRMALLOCSZ (1)
nr_malloc (int size)
{
  void *ret;

  if (nrunlikely (size <= 0)) {
    size = 8;
  }

  ret = (malloc) (size);
  if (nrunlikely (0 == ret)) {
    nrl_error (NRL_MEMORY, "failed to allocate %d byte(s)", size);
    exit (3);
  }

  return ret;
}

void * NRMALLOC NRMALLOCSZ (1)
nr_zalloc (int size)
{
  void *ret;

  if (nrunlikely (size <= 0)) {
    size = 8;
  }

  ret = (calloc) (1, size);
  if (nrunlikely (0 == ret)) {
    nrl_error (NRL_MEMORY, "failed to allocate %d byte(s)", size);
    exit (3);
  }

  return ret;
}

void * NRMALLOC NRCALLOCSZ (1,2)
nr_calloc (int nelem, int elsize)
{
  void *ret;

  if (nrunlikely (nelem <= 0)) {
    nelem = 1;
  }

  if (nrunlikely (elsize <= 0)) {
    elsize = 1;
  }

  ret = (calloc) (nelem, elsize);
  if (nrunlikely (0 == ret)) {
    nrl_error (NRL_MEMORY, "failed to allocate %d x %d bytes", nelem, elsize);
    exit (3);
  }

  return ret;
}

void * NRMALLOCSZ (2)
nr_realloc (void *oldptr, int newsize)
{
  void *ret;

  if (nrunlikely (0 == oldptr)) {
    return nr_malloc (newsize);
  }

  if (nrunlikely (0 == newsize)) {
    newsize = 8;
  }

  ret = (realloc) (oldptr, newsize);
  if (nrunlikely (0 == ret)) {
    nrl_error (NRL_MEMORY, "failed to reallocate %p for %d bytes", oldptr, newsize);
    exit (3);
  }

  return ret;
}

char * NRMALLOC
nr_strdup (const char *orig)
{
  char *ret;
  int lg;

  if (NULL == orig) {
    orig = "";
  }

  /*
   * malloc and strcpy are used here in place of strdup because gcc 4.9.1
   * address and thread sanitizer have issues with strdup in the context of PHP.
   * Quickly browsing the changelogs shows that the sanitizers sometimes treat
   * strdup specially.
   */
  lg = (strlen) (orig);
  ret = (char *)(malloc) (lg + 1);

  if (NULL == ret) {
    nrl_error (NRL_MEMORY | NRL_STRING, "failed to duplicate string %p", orig);
    exit (3);
  }

  (strcpy) (ret, orig);

  return ret;
}

char * NRMALLOC
nr_strndup (const char *orig, int len)
{
  char *ret;
  int olen;
  const char *np;

  if (nrunlikely (len <= 0)) {
    return nr_strdup ("");
  }

  if (nrunlikely (0 == orig)) {
    return nr_strdup ("");
  }

  np = (const char *)(memchr) (orig, 0, (size_t)len);
  if (nrlikely (0 != np)) {
    olen = np - orig;
  } else {
    olen = len;
  }

  ret = (char *)nr_malloc (olen + 1);

  if (nrunlikely (0 == ret)) {
    nrl_error (NRL_MEMORY | NRL_STRING, "failed to duplicate string %p %d", orig, len);
    exit (3);
  }

  (memcpy) (ret, orig, olen);
  ret[olen] = 0;

  return ret;
}

#define free free_notimplemented
