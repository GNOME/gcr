/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
   Copyright (C) 2010 Collabora Ltd

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

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr/gcr.h"
#include "gcr/gcr-internal.h"

#include "gck/gck-test.h"

#include "egg/egg-testing.h"

#include <glib.h>

#include <errno.h>

typedef struct {
	GBytes *bytes;
} Test;

static void
setup (Test *test, gconstpointer unused)
{
	gchar *contents;
	gsize length;
	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/der-certificate.crt", &contents, &length, NULL))
		g_assert_not_reached ();

	test->bytes = g_bytes_new_take (contents, length);
	g_assert (test->bytes);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_clear_pointer (&test->bytes, g_bytes_unref);
}

static void
test_new (Test *test, gconstpointer unused)
{
	GcrCertificate *cert;
	gconstpointer der;
	gsize n_der;

	cert = gcr_simple_certificate_new (test->bytes);
	g_assert (GCR_IS_SIMPLE_CERTIFICATE (cert));

	der = gcr_certificate_get_der_data (cert, &n_der);
	g_assert (der);

	g_object_unref (cert);
}

static void
test_new_from_file (Test *test, gconstpointer unused)
{
	GcrCertificate *cert;
	GFile *file;
	gconstpointer der;
	gsize n_der;
	gconstpointer ref_der;
	gsize ref_n_der;

	file = g_file_new_for_path (SRCDIR "/gcr/fixtures/der-certificate.crt");
	cert = gcr_simple_certificate_new_from_file (file, NULL, NULL);
	g_assert (GCR_IS_SIMPLE_CERTIFICATE (cert));

	der = gcr_certificate_get_der_data (cert, &n_der);
	g_assert (der);
	ref_der = g_bytes_get_data (test->bytes, &ref_n_der);
	egg_assert_cmpsize (n_der, ==, ref_n_der);
	egg_assert_cmpmem (der, n_der, ==, ref_der, ref_n_der);

	g_object_unref (cert);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-simple-certificate");

	g_test_add ("/gcr/simple-certificate/new", Test, NULL, setup, test_new, teardown);
	g_test_add ("/gcr/simple-certificate/new_from_file", Test, NULL, setup, test_new_from_file, teardown);

	return g_test_run ();
}
