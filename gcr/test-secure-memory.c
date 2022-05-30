/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* unit-test-memory.c: Test memory allocation functionality

   Copyright (C) 2007 Stefan Walter
   Copyright (C) 2012 Red Hat Inc.

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@gnome.org>
*/

#include "config.h"

#include "gcr/gcr-secure-memory.h"
#include "egg/egg-secure-memory-private.h"

#include <glib.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/types.h>

#define IS_ZERO -1

static int
find_non_zero (gpointer mem, gsize len)
{
	guchar *b, *e;
	gsize sz = 0;
	for (b = (guchar*)mem, e = ((guchar*)mem) + len; b != e; ++b, ++sz) {
		if (*b != 0x00)
			return (int)sz;
	}

	return -1;
}

static gsize
get_rlimit_memlock (void)
{
	struct rlimit memlock;

	/* If the test program is running as a privileged user, it is
	 * not possible to set the limit. Skip the test entirely.
	 * FIXME: make this check more accurate, e.g., using capabilities
	 */
	if (getuid () == 0) {
		g_test_skip ("test cannot run as root");
		return 0;
	}

	if (getrlimit (RLIMIT_MEMLOCK, &memlock) != 0)
		g_error ("getrlimit() failed: %s", strerror (errno));

	/* Check that the limit is finite, and that if the caller increases its
	 * size by one (to get the first invalid allocation size), things won’t
	 * explode. */
	if (memlock.rlim_cur == RLIM_INFINITY ||
	    memlock.rlim_cur >= G_MAXSIZE - 1) {
		/* Try reducing the limit to 64KiB. */
		memlock.rlim_cur = MIN (64 * 1024, memlock.rlim_max);
		if (setrlimit (RLIMIT_MEMLOCK, &memlock) != 0) {
			g_test_skip ("setrlimit() failed");
			return 0;
		}
	}

	return memlock.rlim_cur;
}

static void
test_alloc_free (void)
{
	gpointer p;
	gboolean ret;

	p = gcr_secure_memory_alloc (512);
	g_assert (p != NULL);
	g_assert_cmpint (IS_ZERO, ==, find_non_zero (p, 512));

	memset (p, 0x67, 512);

	ret = gcr_secure_memory_is_secure (p);
	g_assert (ret == TRUE);

	gcr_secure_memory_free (p);
}

static void
test_alloc_two (void)
{
	gpointer p, p2;
	gboolean ret;

	p2 = gcr_secure_memory_alloc (4);
	g_assert(p2 != NULL);
	g_assert_cmpint (IS_ZERO, ==, find_non_zero (p2, 4));

	memset (p2, 0x67, 4);

	p = gcr_secure_memory_alloc (16200);
	g_assert (p != NULL);
	g_assert_cmpint (IS_ZERO, ==, find_non_zero (p, 16200));

	memset (p, 0x67, 16200);

	ret = gcr_secure_memory_is_secure (p);
	g_assert (ret == TRUE);

	gcr_secure_memory_free (p2);
	gcr_secure_memory_free (p);
}

/* Test alloc() with an allocation larger than RLIMIT_MEMLOCK, which should
 * fail. */
static void
test_alloc_oversized (void)
{
	gsize limit;
	gpointer mem;

	limit = get_rlimit_memlock ();
	if (limit == 0)
		return;

	/* Try the allocation. */
	egg_set_secure_warnings (0);

	mem = gcr_secure_memory_try_alloc (limit + 1);
	g_assert_null (mem);

	egg_set_secure_warnings (1);
}

static void
test_realloc (void)
{
	const gchar *str = "a test string to see if realloc works properly";
	gpointer p, p2;
	gsize len;

	len = strlen (str) + 1;

	p = gcr_secure_memory_realloc (NULL, len);
	g_assert (p != NULL);
	g_assert_cmpint (IS_ZERO, ==, find_non_zero (p, len));

	strcpy ((gchar*)p, str);

	p2 = gcr_secure_memory_realloc (p, 512);
	g_assert (p2 != NULL);

	/* "strings not equal after realloc" */
	g_assert_cmpstr (p2, ==, str);

	p = gcr_secure_memory_realloc (p2, 0);
	/* "should have freed memory" */
	g_assert (p == NULL);
}

static void
test_realloc_across (void)
{
	gpointer p, p2;

	/* Tiny allocation */
	p = gcr_secure_memory_realloc (NULL, 1088);
	g_assert (p != NULL);
	g_assert_cmpint (IS_ZERO, ==, find_non_zero (p, 1088));

	/* Reallocate to a large one, will have to have changed blocks */
	p2 = gcr_secure_memory_realloc (p, 16200);
	g_assert (p2 != NULL);
	g_assert_cmpint (IS_ZERO, ==, find_non_zero (p2, 16200));

	gcr_secure_memory_free (p2);
}

/* Test realloc() with an allocation larger than RLIMIT_MEMLOCK, which should
 * fail. */
static void
test_realloc_oversized (void)
{
	gsize limit;
	gpointer mem, new_mem;

	limit = get_rlimit_memlock ();
	if (limit == 0)
		return;

	/* Try the allocation. */
	mem = gcr_secure_memory_alloc (64);
	g_assert_nonnull (mem);

	egg_set_secure_warnings (0);

	new_mem = gcr_secure_memory_try_realloc (mem, limit + 1);
	g_assert_null (new_mem);

	egg_set_secure_warnings (1);

	gcr_secure_memory_free (mem);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-memory");

	g_test_add_func ("/memory/alloc-free", test_alloc_free);
	g_test_add_func ("/memory/alloc-two", test_alloc_two);
	g_test_add_func ("/memory/alloc-oversized", test_alloc_oversized);
	g_test_add_func ("/memory/realloc", test_realloc);
	g_test_add_func ("/memory/realloc-across", test_realloc_across);
	g_test_add_func ("/memory/realloc-oversized", test_realloc_oversized);

	return g_test_run ();
}
