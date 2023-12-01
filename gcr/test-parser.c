/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* unit-test-pkix-parser.c: Test PKIX parser

   Copyright (C) 2007 Stefan Walter

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

   Author: Stef Walter <stef@memberwebs.com>
*/

#include "config.h"

#include "egg/egg-error.h"
#include "egg/egg-secure-memory.h"
#include "egg/egg-testing.h"

#include "gcr/gcr.h"
#include "gcr/gcr-internal.h"

#include "gck/gck.h"

#include <glib.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Each test looks like (on one line):
 *     void unit_test_xxxxx (CuTest* cu)
 *
 * Each setup looks like (on one line):
 *     void unit_setup_xxxxx (void);
 *
 * Each teardown looks like (on one line):
 *     void unit_teardown_xxxxx (void);
 *
 * Tests be run in the order specified here.
 */

typedef struct {
	GcrParser *parser;
	const gchar* filedesc;
} Test;

static void
ensure_block_can_be_parsed (GcrDataFormat format,
                            GBytes *block)
{
	GcrParser *parser;
	gboolean result;
	GError *error = NULL;

	g_assert (block != NULL);

	parser = gcr_parser_new ();
	g_object_add_weak_pointer (G_OBJECT (parser), (gpointer *)&parser);
	gcr_parser_format_disable (parser, -1);
	gcr_parser_format_enable (parser, format);
	result = gcr_parser_parse_bytes (parser, block, &error);

	if (!result) {
		g_critical ("The data returned from gcr_parser_get_parsed_block() "
		            "cannot be parsed: %s", error->message);
		g_assert_not_reached ();
	}

	g_object_unref (parser);
	g_assert (parser == NULL);
}

static void
parsed_item (GcrParser *par, gpointer user_data)
{
	GckAttributes *attrs;
	const gchar *description;
	const gchar *label;
	Test *test = user_data;
	GBytes *block;
	GcrDataFormat format;

	g_assert (GCR_IS_PARSER (par));
	g_assert (par == test->parser);

	attrs = gcr_parser_get_parsed_attributes (test->parser);
	g_assert (attrs);

	description = gcr_parser_get_parsed_description (test->parser);
	label = gcr_parser_get_parsed_label (test->parser);
	block = gcr_parser_get_parsed_bytes (test->parser);
	format = gcr_parser_get_parsed_format (test->parser);
	ensure_block_can_be_parsed (format, block);

	if (g_test_verbose ())
		g_print ("%s: '%s'\n", description, label);
}

static gboolean
authenticate (GcrParser *par, gint state, gpointer user_data)
{
	Test *test = user_data;

	g_assert (GCR_IS_PARSER (par));
	g_assert (par == test->parser);

	switch (state) {
	case 0:
		gcr_parser_add_password (test->parser, "booo");
		return TRUE;
	case 1:
		gcr_parser_add_password (test->parser, "usr0052");
		return TRUE;
	default:
		g_printerr ("decryption didn't work for: %s", test->filedesc);
		g_assert_not_reached ();
		return FALSE;
	};
}

static void
setup (Test *test, gconstpointer unused)
{
	test->parser = gcr_parser_new ();
	g_signal_connect (test->parser, "parsed", G_CALLBACK (parsed_item), test);
	g_signal_connect (test->parser, "authenticate", G_CALLBACK (authenticate), test);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_object_unref (test->parser);
}

static void
test_parse_one (Test *test,
                gconstpointer user_data)
{
	const gchar *path = user_data;
	gchar *contents;
	GError *error = NULL;
	gboolean result;
	GBytes *bytes;
	gsize len;

	if (!g_file_get_contents (path, &contents, &len, NULL))
		g_assert_not_reached ();

	test->filedesc = path;
	bytes = g_bytes_new_take (contents, len);
	result = gcr_parser_parse_bytes (test->parser, bytes, &error);
	g_assert_no_error (error);
	g_assert (result);

	g_bytes_unref (bytes);
}

static void
on_parsed_compare_bytes (GcrParser *parser,
                         gpointer user_data)
{
	GBytes *original = user_data;
	GBytes *bytes;
	gconstpointer data;
	gsize n_data;
	GcrParsed *parsed;

	bytes = gcr_parser_get_parsed_bytes (parser);
	g_assert (bytes != NULL);
	g_assert (g_bytes_equal (original, bytes));

	data = gcr_parser_get_parsed_block (parser, &n_data);
	g_assert (data != NULL);
	g_assert_cmpint (n_data, ==, g_bytes_get_size (original));
	g_assert (memcmp (data, g_bytes_get_data (original, NULL), n_data) == 0);

	parsed = gcr_parser_get_parsed (parser);
	g_assert (parsed != NULL);
	bytes = gcr_parsed_get_bytes (parsed);
	g_assert (bytes != NULL);
	g_assert (g_bytes_equal (original, bytes));

	data = gcr_parsed_get_data (parsed, &n_data);
	g_assert (data != NULL);
	g_assert_cmpint (n_data, ==, g_bytes_get_size (original));
	g_assert (memcmp (data, g_bytes_get_data (original, NULL), n_data) == 0);
}

static void
test_parsed_bytes (void)
{
	GcrParser *parser = gcr_parser_new ();
	gchar *contents;
	GError *error = NULL;
	gboolean result;
	GBytes *bytes;
	gsize len;

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/cacert.org.cer", &contents, &len, NULL))
		g_assert_not_reached ();

	bytes = g_bytes_new_take (contents, len);
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parsed_compare_bytes), bytes);
	result = gcr_parser_parse_bytes (parser, bytes, &error);
	g_assert_no_error (error);
	g_assert (result);

	g_bytes_unref (bytes);
	g_object_unref (parser);
}

static void
test_parse_null (void)
{
	GcrParser *parser = gcr_parser_new ();
	GError *error = NULL;
	gboolean result;

	result = gcr_parser_parse_data (parser, NULL, 0, &error);
	g_assert_error (error, GCR_DATA_ERROR, GCR_ERROR_UNRECOGNIZED);
	g_assert (!result);
	g_error_free (error);

	g_object_unref (parser);
}

static void
test_parse_empty (void)
{
	GcrParser *parser = gcr_parser_new ();
	GError *error = NULL;
	gboolean result;

	result = gcr_parser_parse_data (parser, (const guchar *)"", 0, &error);
	g_assert_error (error, GCR_DATA_ERROR, GCR_ERROR_UNRECOGNIZED);
	g_assert (!result);
	g_error_free (error);

	g_object_unref (parser);
}

static void
test_parse_stream (void)
{
	GcrParser *parser = gcr_parser_new ();
	GError *error = NULL;
	gboolean result;
	GFile *file;
	GFileInputStream *fis;
	gchar *contents;
	gsize len;
	GBytes *bytes;

	file = g_file_new_for_path (SRCDIR "/gcr/fixtures/cacert.org.cer");
	fis = g_file_read (file, NULL, &error);
	g_assert_no_error (error);

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/cacert.org.cer", &contents, &len, NULL))
		g_assert_not_reached ();
	bytes = g_bytes_new_take (contents, len);
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parsed_compare_bytes), bytes);

	result = gcr_parser_parse_stream (parser, G_INPUT_STREAM (fis), NULL, &error);
	g_assert_no_error (error);
	g_assert (result);

	g_bytes_unref (bytes);
	g_object_unref (fis);
	g_object_unref (file);
	g_object_unref (parser);
}

static void
on_parsed_ref (GcrParser *parser,
               gpointer user_data)
{
	GcrParsed **parsed = user_data;
	g_assert (parsed != NULL);
	g_assert (*parsed == NULL);
	*parsed = gcr_parsed_ref (gcr_parser_get_parsed (parser));
}

static void
test_parse_filename (void)
{
	GcrParser *parser = gcr_parser_new ();
	GcrParsed *parsed = NULL;
	GError *error = NULL;
	gboolean result;
	gchar *contents;
	gsize len;
	GBytes *bytes;

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/cacert.org.cer", &contents, &len, NULL))
		g_assert_not_reached ();

	bytes = g_bytes_new_take (contents, len);
	gcr_parser_set_filename (parser, "cacert.org.cer");
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parsed_ref), &parsed);

	result = gcr_parser_parse_bytes (parser, bytes, &error);
	g_assert_cmpstr (gcr_parser_get_filename (parser), ==, "cacert.org.cer");
	g_assert_no_error (error);
	g_assert (result);

	g_bytes_unref (bytes);
	g_object_unref (parser);

	g_assert (parsed != NULL);
	g_assert_cmpstr (gcr_parsed_get_filename (parsed), ==, "cacert.org.cer");
	gcr_parsed_unref (parsed);
}

int
main (int argc, char **argv)
{
	const gchar *filename;
	GError *error = NULL;
	GPtrArray *strings;
	GDir *dir;
	gchar *path;
	gchar *lower;
	gchar *test;
	int ret;

	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-parser");

	strings = g_ptr_array_new_with_free_func (g_free);
	dir = g_dir_open (SRCDIR "/gcr/fixtures", 0, &error);
	g_assert_no_error (error);

	for (;;) {
		filename = g_dir_read_name (dir);
		if (!filename)
			break;
		if (filename[0] == '.')
			continue;

#ifdef WITH_GNUTLS
#if GNUTLS_VERSION_NUMBER < 0x030805
		/* Not yet supported in GnuTLS */
		if (g_str_equal (filename, "der-key-PBE-SHA1-DES.p8"))
			continue;
#endif
#endif
		path = g_build_filename (SRCDIR "/gcr/fixtures", filename, NULL);

		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			g_free (path);
			continue;
		}

		lower = g_ascii_strdown (filename, -1);
		test = g_strdup_printf ("/gcr/parser/%s",
		                        g_strcanon (lower, "abcdefghijklmnopqrstuvwxyz0123456789", '_'));
		g_free (lower);

		g_test_add (test, Test, path, setup, test_parse_one, teardown);
		g_ptr_array_add (strings, path);
		g_ptr_array_add (strings, test);
	}

	g_dir_close (dir);

	g_test_add_func ("/gcr/parser/parse_null", test_parse_null);
	g_test_add_func ("/gcr/parser/parse_empty", test_parse_empty);
	g_test_add_func ("/gcr/parser/parse_stream", test_parse_stream);
	g_test_add_func ("/gcr/parser/parsed_bytes", test_parsed_bytes);
	g_test_add_func ("/gcr/parser/filename", test_parse_filename);

	ret = g_test_run ();
	g_ptr_array_free (strings, TRUE);
	return ret;
}
