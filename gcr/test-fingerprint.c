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
#define GCR_COMPILATION 1

#include "gcr/gcr.h"
#include "gcr/gcr-internal.h"
#include "gcr/gcr-fingerprint.h"

#include "gck/gck-test.h"
#include "gck/pkcs11n.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-testing.h"

#include <glib.h>

#include <errno.h>

typedef struct {
	GBytes *cert_rsa;
	GBytes *key_rsa;
	GBytes *cert_dsa;
	GBytes *key_dsa;
} Test;

static void
setup (Test *test, gconstpointer unused)
{
	GError *error = NULL;
	gchar *contents;
	gsize length;

	g_file_get_contents (SRCDIR "/gcr/fixtures/client.crt", &contents, &length, &error);
	g_assert_no_error (error);
	test->cert_rsa = g_bytes_new_take (contents, length);

	g_file_get_contents (SRCDIR "/gcr/fixtures/client.key", &contents, &length, &error);
	g_assert_no_error (error);
	test->key_rsa = g_bytes_new_take (contents, length);

	g_file_get_contents (SRCDIR "/gcr/fixtures/generic-dsa.crt", &contents, &length, &error);
	g_assert_no_error (error);
	test->cert_dsa = g_bytes_new_take (contents, length);

	g_file_get_contents (SRCDIR "/gcr/fixtures/generic-dsa.key", &contents, &length, &error);
	g_assert_no_error (error);
	test->key_dsa = g_bytes_new_take (contents, length);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_bytes_unref (test->cert_rsa);
	g_bytes_unref (test->key_rsa);
	g_bytes_unref (test->cert_dsa);
	g_bytes_unref (test->key_dsa);
}

static void
on_parser_parsed (GcrParser *parser,
                  gpointer user_data)
{
	GckAttributes **attrs = user_data;
	g_assert (!*attrs);
	*attrs = gcr_parser_get_parsed_attributes (parser);
	g_assert (*attrs);
	gck_attributes_ref (*attrs);
}

static GckAttributes*
parse_attributes_for_key (GBytes *data)
{
	GcrParser *parser;
	GckAttributes *attrs = NULL;
	GError *error = NULL;

	parser = gcr_parser_new ();
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), &attrs);
	gcr_parser_parse_bytes (parser, data, &error);
	g_assert_no_error (error);
	g_object_unref (parser);

	g_assert (attrs);
	return attrs;
}

static GckAttributes *
build_attributes_for_cert (GBytes *data)
{
	GckBuilder builder = GCK_BUILDER_INIT;

	gck_builder_add_data (&builder, CKA_VALUE, g_bytes_get_data (data, NULL),
	                      g_bytes_get_size (data));
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_CERTIFICATE);
	gck_builder_add_ulong (&builder, CKA_CERTIFICATE_TYPE, CKC_X_509);

	return gck_builder_end (&builder);
}

static GBytes *
parse_subject_public_key_info_for_cert (GBytes *data)
{
	GBytes *info;
	GNode *asn;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "Certificate", data);
	g_assert (asn != NULL);

	info = egg_asn1x_get_element_raw (egg_asn1x_node (asn, "tbsCertificate", "subjectPublicKeyInfo", NULL));
	g_assert (info != NULL);

	egg_asn1x_destroy (asn);
	return info;
}

static void
test_rsa (Test *test, gconstpointer unused)
{
	GckAttributes *key, *cert;
	GBytes *info;
	guchar *fingerprint1, *fingerprint2, *fingerprint3;
	gsize n_fingerprint1, n_fingerprint2, n_fingerprint3;

	key = parse_attributes_for_key (test->key_rsa);
	info = parse_subject_public_key_info_for_cert (test->cert_rsa);
	cert = build_attributes_for_cert (test->cert_rsa);

	fingerprint1 = gcr_fingerprint_from_subject_public_key_info (g_bytes_get_data (info, NULL),
	                                                             g_bytes_get_size (info),
	                                                             G_CHECKSUM_SHA1, &n_fingerprint1);
	fingerprint2 = gcr_fingerprint_from_attributes (key, G_CHECKSUM_SHA1, &n_fingerprint2);
	fingerprint3 = gcr_fingerprint_from_attributes (cert, G_CHECKSUM_SHA1, &n_fingerprint3);

	egg_assert_cmpmem (fingerprint1, n_fingerprint1, ==, fingerprint2, n_fingerprint2);
	egg_assert_cmpmem (fingerprint1, n_fingerprint1, ==, fingerprint3, n_fingerprint3);

	g_free (fingerprint1);
	g_free (fingerprint2);
	g_free (fingerprint3);

	g_bytes_unref (info);
	gck_attributes_unref (key);
	gck_attributes_unref (cert);
}

static void
test_dsa (Test *test, gconstpointer unused)
{
	GckAttributes *key, *cert;
	GBytes *info;
	guchar *fingerprint1, *fingerprint2, *fingerprint3;
	gsize n_fingerprint1, n_fingerprint2, n_fingerprint3;

	key = parse_attributes_for_key (test->key_dsa);
	info = parse_subject_public_key_info_for_cert (test->cert_dsa);
	cert = build_attributes_for_cert (test->cert_dsa);

	fingerprint1 = gcr_fingerprint_from_subject_public_key_info (g_bytes_get_data (info, NULL),
	                                                             g_bytes_get_size (info),
	                                                             G_CHECKSUM_SHA1, &n_fingerprint1);
	fingerprint2 = gcr_fingerprint_from_attributes (key, G_CHECKSUM_SHA1, &n_fingerprint2);
	fingerprint3 = gcr_fingerprint_from_attributes (cert, G_CHECKSUM_SHA1, &n_fingerprint3);

	egg_assert_cmpmem (fingerprint1, n_fingerprint1, ==, fingerprint2, n_fingerprint2);
	egg_assert_cmpmem (fingerprint1, n_fingerprint1, ==, fingerprint3, n_fingerprint3);

	g_free (fingerprint1);
	g_free (fingerprint2);
	g_free (fingerprint3);

	g_bytes_unref (info);
	gck_attributes_unref (key);
	gck_attributes_unref (cert);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_add ("/gcr/fingerprint/rsa", Test, NULL, setup, test_rsa, teardown);
	g_test_add ("/gcr/fingerprint/dsa", Test, NULL, setup, test_dsa, teardown);

	return g_test_run ();
}
