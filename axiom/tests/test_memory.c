#include "nr_axiom.h"

#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

/*
 * nr_malloc (0) must return a valid pointer.
 */
static void
test_malloc_valid (void)
{
  char *rp;

  rp = (char *)nr_malloc (0);
  tlib_pass_if_true ("nr_malloc (0) returns a pointer", 0 != rp, "rp=%p", rp);
  nr_free (rp);
}

/*
 * nr_calloc(0,x) or nr_calloc (x,0) returns a valid pointer.
 */
static void
test_calloc_0_valid (void)
{
  char *rp;

  rp = (char *)nr_calloc (0, 10);
  tlib_pass_if_true ("nr_calloc (0, 10) returns a pointer", 0 != rp, "rp=%p", rp);
  nr_free (rp);
  rp = (char *)nr_calloc (10, 0);
  tlib_pass_if_true ("nr_calloc (10, 0) returns a pointer", 0 != rp, "rp=%p", rp);
  nr_free (rp);
}

/*
 * nr_realloc (NULL, x) returns a valid pointer, as does
 * nr_realloc (ptr, 0) and nr_realloc (0, 0).
 */
static void
test_calloc_null_valid (void)
{
  char *rp;

  rp = (char *)nr_realloc (0, 10);
  tlib_pass_if_true ("nr_realloc (0, 10) returns a pointer", 0 != rp, "rp=%p", rp);
  nr_free (rp);
  rp = (char *)nr_malloc (10);
  tlib_pass_if_true ("nr_malloc (10) for nr_realloc returns a pointer", 0 != rp, "rp=%p", rp);
  rp = (char *)nr_realloc (rp, 0);
  tlib_pass_if_true ("nr_realloc (ptr, 0) returns a pointer", 0 != rp, "rp=%p", rp);
  nr_free (rp);
  rp = (char *)nr_realloc (0, 0);
  tlib_pass_if_true ("nr_realloc (0, 0) returns a pointer", 0 != rp, "rp=%p", rp);
  nr_free (rp);
}

/*
 * test that free also sets the pointer to NULL, and that
 * calling free on a NULL pointer just returns.
 */
static void
test_free_side_effect (void)
{
  char *rp;

  rp = (char *)nr_malloc (16);
  tlib_pass_if_true ("nr_malloc (16) for free tests", 0 != rp, "rp=%p", rp);
  nr_free (rp);
  tlib_pass_if_true ("freed pointer is NULL", 0 == rp, "rp=%p", rp);
  rp = 0;
  nr_free (rp);
  tlib_pass_if_true ("nr_free (0) does not crash", 0 == rp, "rp=%p", rp);
}

/*
 * test that string duplication works.
 */
static void
test_strdup (void)
{
  char *rp;

  rp = nr_strdup ("abc");
  tlib_pass_if_true ("simple nr_strdup", (0 != rp) && (0 == nr_strcmp (rp, "abc")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);
  rp = nr_strdup ("");
  tlib_pass_if_true ("nr_strdup of empty string", (0 != rp) && (0 == nr_strcmp (rp, "")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);
  rp = nr_strdup (0);
  tlib_pass_if_true ("nr_strdup of NULL string", (0 != rp) && (0 == nr_strcmp (rp, "")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);
}

/*
 * test that string duplication with default fallback works.
 */
static void
test_strdup_or (void)
{

  char *rp;

  rp = nr_strdup_or ("abc", "default");
  tlib_pass_if_true ("simple nr_strdup_or", (0 != rp) && (0 == nr_strcmp (rp, "abc")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);

  rp = nr_strdup_or ("", "default");
  tlib_pass_if_true ("nr_strdup_or of empty string", (0 != rp) && (0 == nr_strcmp (rp, "")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);

  rp = nr_strdup_or (0,"default");
  tlib_pass_if_true ("nr_strdup_or of NULL string", (0 != rp) && (0 == nr_strcmp (rp, "default")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free(rp);

  rp = nr_strdup_or (0,0);
  tlib_pass_if_true ("nr_strdup_or of NULL string with NULL backup", (0 != rp) && (0 == nr_strcmp (rp, "")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);

  rp = nr_strdup_or ("abc",0);
  tlib_pass_if_true ("nr_strdup_or of string with NULL backup", (0 != rp) && (0 == nr_strcmp (rp, "abc")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);
}

/*
 * test that nr_strndup() works.
 */
static void
test_strndup (void)
{
  char *rp;

  rp = nr_strndup ("abc", 3);
  tlib_pass_if_true ("simple nr_strndup", (0 != rp) && (0 == nr_strcmp (rp, "abc")), "rp=%p rp='%s'", rp, rp ? rp: "");
  nr_free (rp);
  rp = nr_strndup ("", 16);
  tlib_pass_if_true ("nr_strndup of empty string", (0 != rp) && (0 == nr_strcmp (rp, "")), "rp=%p rp='%s'", rp, rp ? rp : rp);
  nr_free (rp);
  rp = nr_strndup (0, 16);
  tlib_pass_if_true ("nr_strndup of NULL string", (0 != rp) && (0 == nr_strcmp (rp, "")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);
  rp = nr_strndup ("abcdef", 4);
  tlib_pass_if_true ("nr_strndup of longer string", (0 != rp) && (0 == nr_strcmp (rp, "abcd")), "rp=%p rp='%s'", rp, rp ? rp : "");
  nr_free (rp);
}

static void
test_memcpy (void)
{
  void *as;
  void *bs;
  void *dest;
  void *retval;
  int len;

  len = 64;
  as = nr_malloc (len);
  bs = nr_malloc (len);
  dest = nr_malloc (len);

  nr_memset (as, 0xaa, len);
  nr_memset (bs, 0xbb, len);

  tlib_pass_if_null("memcpy to NULL dest", nr_memcpy (NULL, as, len));

  retval = nr_memcpy (dest, as, len);
  tlib_pass_if_ptr_equal ("memcpy returns dest", dest, retval);
  tlib_pass_if_bytes_equal ("memcpy copies src", dest, len, as, len);

  retval = nr_memcpy (dest, NULL, len);
  tlib_pass_if_ptr_equal ("memcpy from NULL src", dest, retval);
  tlib_pass_if_bytes_equal ("memcpy copies src", dest, len, as, len);

  retval = nr_memcpy (dest, bs, 0);
  tlib_pass_if_ptr_equal ("memcpy from NULL src", dest, retval);
  tlib_pass_if_bytes_equal ("memcpy zero size doesn't modify dest", dest, len, as, len);

  retval = nr_memcpy (dest, bs, -1);
  tlib_pass_if_ptr_equal ("memcpy from NULL src", dest, retval);
  tlib_pass_if_bytes_equal ("memcpy negative size doesn't modify dest", dest, len, as, len);

  nr_free (as);
  nr_free (bs);
  nr_free (dest);
}

static void
test_memmove (void)
{
  void *as;
  void *bs;
  void *dest;
  void *retval;
  int len;

  len = 64;
  as = nr_malloc (len);
  bs = nr_malloc (len);
  dest = nr_malloc (len);

  nr_memset (as, 0xaa, len);
  nr_memset (bs, 0xbb, len);

  tlib_pass_if_null("memmove to NULL dest", nr_memmove (NULL, as, len));

  retval = nr_memmove (dest, as, len);
  tlib_pass_if_ptr_equal ("memmove returns dest", dest, retval);
  tlib_pass_if_bytes_equal ("memmove copies src", dest, len, as, len);

  retval = nr_memmove (dest, NULL, len);
  tlib_pass_if_ptr_equal ("memmove from NULL src", dest, retval);
  tlib_pass_if_bytes_equal ("memmove copies src", dest, len, as, len);

  retval = nr_memmove (dest, bs, 0);
  tlib_pass_if_ptr_equal ("memmove from NULL src", dest, retval);
  tlib_pass_if_bytes_equal ("memmove zero size doesn't modify dest", dest, len, as, len);

  retval = nr_memmove (dest, bs, -1);
  tlib_pass_if_ptr_equal ("memmove from NULL src", dest, retval);
  tlib_pass_if_bytes_equal ("memmove negative size doesn't modify dest", dest, len, as, len);

  nr_free (as);
  nr_free (bs);
  nr_free (dest);
}

static void
test_memcmp (void)
{
  int rv;

  /*
   * Test invalid length.
   */

  rv = nr_memcmp (NULL, NULL, -1);
  tlib_pass_if_int_equal ("nr_memcmp(NULL, NULL, 0)", 0, rv);

  rv = nr_memcmp ("", NULL, -1);
  tlib_pass_if_int_equal ("nr_memcmp(\"\", NULL, 0)", 0, rv);

  rv = nr_memcmp (NULL, "", -1);
  tlib_pass_if_int_equal ("nr_memcmp(NULL, \"\", 0)", 0, rv);

  rv = nr_memcmp ("", "", -1);
  tlib_pass_if_int_equal ("nr_memcmp(\"\", \"\", -1)", 0, rv);

  rv = nr_memcmp ("a", "b", -1);
  tlib_pass_if_int_equal ("nr_memcmp(\"a\", \"b\", -1)", 0, rv);

  /*
   * Test zero-length comparisons.
   */

  rv = nr_memcmp (NULL, NULL, 0);
  tlib_pass_if_int_equal ("nr_memcmp(NULL, NULL, 0)", 0, rv);

  rv = nr_memcmp ("", NULL, 0);
  tlib_pass_if_int_equal ("nr_memcmp(\"\", NULL, 0)", 0, rv);

  rv = nr_memcmp (NULL, "", 0);
  tlib_pass_if_int_equal ("nr_memcmp(NULL, \"\", 0)", 0, rv);

  rv = nr_memcmp ("", "", 0);
  tlib_pass_if_int_equal ("nr_memcmp(\"\", \"\", 0)", 0, rv);

  rv = nr_memcmp ("a", "b", 0);
  tlib_pass_if_int_equal ("nr_memcmp(\"aB\", \"ab\", 0)", 0, rv);

  /*
   * Test positive lengths.
   */

  rv = nr_memcmp (NULL, NULL, 1);
  tlib_pass_if_int_equal ("nr_memcmp(NULL, NULL, 1)", 0, rv);

  rv = nr_memcmp ("", NULL, 1);
  tlib_pass_if_true ("nr_memcmp(\"\", NULL, 1)", rv > 0, "rv=%d", rv);

  rv = nr_memcmp (NULL, "", 1);
  tlib_pass_if_true ("nr_memcmp(NULL, \"\", 1)", rv < 0, "rv=%d", rv);

  rv = nr_memcmp ("a", "a", 1);
  tlib_pass_if_int_equal ("nr_memcmp(\"a\", \"a\", 1)", 0, rv);

  rv = nr_memcmp ("a", "b", 1);
  tlib_pass_if_true ("nr_memcmp(\"a\", \"b\", 2)", rv < 0, "rv=%d", rv);

  rv = nr_memcmp ("b", "a", 1);
  tlib_pass_if_true ("nr_memcmp(\"b\", \"a\", 2)", rv > 0, "rv=%d", rv);
}

static void
test_memchr (void)
{
  char buf[] = "abc";
  void *rv;

  rv = nr_memchr (NULL, 'a', -1);
  tlib_pass_if_null ("null buffer and negative length", rv);

  rv = nr_memchr (buf, 'd', -1);
  tlib_pass_if_null ("negative length and value not present", rv);

  rv = nr_memchr (buf, 'a', -1);
  tlib_pass_if_null ("negative length and value present", rv);

  rv = nr_memchr (NULL, 'a', 0);
  tlib_pass_if_null ("null buffer", rv);

  rv = nr_memchr (buf, 'd', 0);
  tlib_pass_if_null ("zero length and value not present", rv);

  rv = nr_memchr (buf, 'a', 0);
  tlib_pass_if_null ("zero length and value present", rv);

  rv = nr_memchr (buf, 'b', 1);
  tlib_pass_if_null ("value not present", rv);

  rv = nr_memchr (buf, 'a', 1);
  tlib_pass_if_ptr_equal ("value present at buf[0]", buf, rv);

  rv = nr_memchr (buf, 'c', 4);
  tlib_pass_if_ptr_equal ("value present at buf[2]", &buf[2], rv);
}

void
test_main (void *p NRUNUSED)
{
  test_malloc_valid ();
  test_calloc_0_valid ();
  test_calloc_null_valid ();
  test_free_side_effect ();

  test_strdup ();
  test_strdup_or ();
  test_strndup ();

  test_memcpy ();
  test_memmove ();
  test_memcmp ();
  test_memchr ();
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};
