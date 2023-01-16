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

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"

#include "gcr/gcr.h"
#include "gcr/gcr-internal.h"

#include "egg/egg-testing.h"

#include "gck/gck-mock.h"
#include "gck/gck-test.h"
#include "gck/pkcs11n.h"

#include <glib.h>

#include <errno.h>

typedef struct {
	gpointer cert_data;
	gsize n_cert_data;
	gpointer cert2_data;
	gsize n_cert2_data;
	CK_FUNCTION_LIST funcs;
} Test;

static void
setup (Test *test, gconstpointer unused)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GList *modules = NULL;
	CK_FUNCTION_LIST_PTR f;
	GckModule *module;
	GBytes *subject;
	GBytes *bytes;
	GNode *asn, *node;
	CK_RV rv;

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/der-certificate.crt", (gchar**)&test->cert_data,
	                          &test->n_cert_data, NULL))
		g_assert_not_reached ();
	g_assert (test->cert_data);

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/der-certificate-dsa.cer", (gchar**)&test->cert2_data,
	                          &test->n_cert2_data, NULL))
		g_assert_not_reached ();
	g_assert (test->cert2_data);

	rv = gck_mock_C_GetFunctionList (&f);
	gck_assert_cmprv (rv, ==, CKR_OK);
	memcpy (&test->funcs, f, sizeof (test->funcs));

	/* Open a session */
	rv = (test->funcs.C_Initialize) (NULL);
	gck_assert_cmprv (rv, ==, CKR_OK);

	g_assert (!modules);
	module = gck_module_new (&test->funcs);
	modules = g_list_prepend (modules, module);
	gcr_pkcs11_set_modules (modules);
	g_clear_list (&modules, g_object_unref);

	bytes = g_bytes_new_static (test->cert_data, test->n_cert_data);
	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "Certificate", bytes);
	g_assert (asn);
	node = egg_asn1x_node (asn, "tbsCertificate", "subject", NULL);
	subject = egg_asn1x_get_element_raw (node);

	/* Add a certificate to the module */
	gck_builder_add_data (&builder, CKA_VALUE, test->cert_data, test->n_cert_data);
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_CERTIFICATE);
	gck_builder_add_ulong (&builder, CKA_CERTIFICATE_TYPE, CKC_X_509);
	gck_builder_add_data (&builder, CKA_SUBJECT,
	                      g_bytes_get_data (subject, NULL),
	                      g_bytes_get_size (subject));
	gck_builder_add_string (&builder, CKA_ID, "test-issuer");
	gck_mock_module_add_object (gck_builder_end (&builder));

	g_bytes_unref (bytes);
	g_bytes_unref (subject);
	egg_asn1x_destroy (asn);
}

static void
teardown (Test *test, gconstpointer unused)
{
	CK_RV rv;

	g_free (test->cert_data);
	g_free (test->cert2_data);

	rv = (test->funcs.C_Finalize) (NULL);
	gck_assert_cmprv (rv, ==, CKR_OK);

	_gcr_uninitialize_library ();
}

static void
assert_pkcs11_certificates_matches (Test *test, GcrCertificate *cert)
{
	GckAttributes *attrs;
	const GckAttribute *attr;
	gconstpointer der;
	gsize n_der;

	/* Should be the same certificate */
	der = gcr_certificate_get_der_data (cert, &n_der);
	egg_assert_cmpsize (n_der, ==, test->n_cert_data);
	g_assert (memcmp (der, test->cert_data, test->n_cert_data) == 0);

	/* Should return the same certificate here too */
	attrs = gcr_pkcs11_certificate_get_attributes (GCR_PKCS11_CERTIFICATE (cert));
	g_assert (attrs);
	attr = gck_attributes_find (attrs, CKA_VALUE);
	g_assert (attr);
	egg_assert_cmpsize (attr->length, ==, test->n_cert_data);
	g_assert (memcmp (attr->value, test->cert_data, test->n_cert_data) == 0);

	/* Should return the same certificate here too */
	attrs = NULL;
	g_object_get (cert, "attributes", &attrs, NULL);
	g_assert (attrs);
	attr = gck_attributes_find (attrs, CKA_VALUE);
	g_assert (attr);
	egg_assert_cmpsize (attr->length, ==, test->n_cert_data);
	g_assert (memcmp (attr->value, test->cert_data, test->n_cert_data) == 0);
	gck_attributes_unref (attrs);
}

static void
test_lookup_certificate_issuer (Test *test, gconstpointer unused)
{
	GcrCertificate *cert, *issuer;
	GError *error = NULL;

	cert = gcr_simple_certificate_new_static (test->cert_data, test->n_cert_data);
	g_assert (cert);

	/* Should be self-signed, so should find itself (added in setup) */
	issuer = gcr_pkcs11_certificate_lookup_issuer (cert, NULL, &error);
	g_assert (GCR_IS_PKCS11_CERTIFICATE (issuer));
	g_assert (error == NULL);

	assert_pkcs11_certificates_matches (test, issuer);

	g_object_unref (cert);
	g_object_unref (issuer);
}

static void
test_lookup_certificate_issuer_not_found (Test *test, gconstpointer unused)
{
	GcrCertificate *cert, *issuer;
	GError *error = NULL;

	cert = gcr_simple_certificate_new_static (test->cert2_data, test->n_cert2_data);
	g_assert (cert);

	/* Issuer shouldn't be found */
	issuer = gcr_pkcs11_certificate_lookup_issuer (cert, NULL, &error);
	g_assert (issuer == NULL);
	g_assert (error == NULL);

	g_object_unref (cert);
}

static void
fetch_async_result (GObject *source, GAsyncResult *result, gpointer user_data)
{
	*((GAsyncResult**)user_data) = result;
	g_object_ref (result);
	egg_test_wait_stop ();
}

static void
test_lookup_certificate_issuer_async (Test *test, gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GcrCertificate *cert, *issuer;
	GError *error = NULL;

	cert = gcr_simple_certificate_new_static (test->cert_data, test->n_cert_data);
	g_assert (cert);

	/* Should be self-signed, so should find itself (added in setup) */
	gcr_pkcs11_certificate_lookup_issuer_async (cert, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert (result);
	issuer = gcr_pkcs11_certificate_lookup_issuer_finish (result, &error);
	g_assert (GCR_IS_PKCS11_CERTIFICATE (issuer));
	g_assert (error == NULL);
	g_object_unref (result);
	result = NULL;

	assert_pkcs11_certificates_matches (test, issuer);

	g_object_unref (cert);
	g_object_unref (issuer);
}

static void
test_lookup_certificate_issuer_failure (Test *test, gconstpointer unused)
{
	GcrCertificate *cert, *issuer;
	GError *error = NULL;

	cert = gcr_simple_certificate_new_static (test->cert_data, test->n_cert_data);
	g_assert (cert);

	/* Make the lookup fail */
	test->funcs.C_GetAttributeValue = gck_mock_fail_C_GetAttributeValue;

	issuer = gcr_pkcs11_certificate_lookup_issuer (cert, NULL, &error);
	g_assert (issuer == NULL);
	g_assert_error (error, GCK_ERROR, CKR_FUNCTION_FAILED);
	g_assert (error->message);
	g_clear_error (&error);

	g_object_unref (cert);
}

static void
test_lookup_certificate_issuer_fail_async (Test *test, gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GcrCertificate *cert, *issuer;
	GError *error = NULL;

	cert = gcr_simple_certificate_new_static (test->cert_data, test->n_cert_data);
	g_assert (cert);

	/* Make the lookup fail */
	test->funcs.C_GetAttributeValue = gck_mock_fail_C_GetAttributeValue;

	/* Should be self-signed, so should find itself (added in setup) */
	gcr_pkcs11_certificate_lookup_issuer_async (cert, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert (result);
	issuer = gcr_pkcs11_certificate_lookup_issuer_finish (result, &error);
	g_assert (issuer == NULL);
	g_assert_error (error, GCK_ERROR, CKR_FUNCTION_FAILED);
	g_assert (error->message);
	g_clear_error (&error);
	g_object_unref (result);
	result = NULL;

	g_object_unref (cert);
}

static void
test_new_from_uri (Test *test, gconstpointer unused)
{
	GcrCertificate *cert, *issuer;
	GError *error = NULL;

	cert = gcr_simple_certificate_new_static (test->cert_data, test->n_cert_data);
	g_assert (cert);

	issuer = gcr_pkcs11_certificate_new_from_uri ("pkcs11:id=test-issuer", NULL, &error);
	g_assert (GCR_IS_PKCS11_CERTIFICATE (issuer));
	g_assert (error == NULL);

	assert_pkcs11_certificates_matches (test, issuer);

	g_object_unref (cert);
	g_object_unref (issuer);
}

static void
test_new_from_uri_async (Test *test, gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GcrCertificate *cert, *issuer;
	GError *error = NULL;

	cert = gcr_simple_certificate_new_static (test->cert_data, test->n_cert_data);
	g_assert (cert);

	gcr_pkcs11_certificate_new_from_uri_async ("pkcs11:id=test-issuer", NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert (result);
	issuer = gcr_pkcs11_certificate_new_from_uri_finish (result, &error);
	g_assert (GCR_IS_PKCS11_CERTIFICATE (issuer));
	g_assert (error == NULL);
	g_object_unref (result);
	result = NULL;

	assert_pkcs11_certificates_matches (test, issuer);

	g_object_unref (cert);
	g_object_unref (issuer);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-pkcs11-certificate");

	g_test_add ("/gcr/pkcs11-certificate/lookup_certificate_issuer", Test, NULL, setup, test_lookup_certificate_issuer, teardown);
	g_test_add ("/gcr/pkcs11-certificate/lookup_certificate_issuer_not_found", Test, NULL, setup, test_lookup_certificate_issuer_not_found, teardown);
	g_test_add ("/gcr/pkcs11-certificate/lookup_certificate_issuer_async", Test, NULL, setup, test_lookup_certificate_issuer_async, teardown);
	g_test_add ("/gcr/pkcs11-certificate/lookup_certificate_issuer_failure", Test, NULL, setup, test_lookup_certificate_issuer_failure, teardown);
	g_test_add ("/gcr/pkcs11-certificate/lookup_certificate_issuer_fail_async", Test, NULL, setup, test_lookup_certificate_issuer_fail_async, teardown);
	g_test_add ("/gcr/pkcs11-certificate/new_from_uri", Test, NULL, setup, test_new_from_uri, teardown);
	g_test_add ("/gcr/pkcs11-certificate/new_from_uri_async", Test, NULL, setup, test_new_from_uri_async, teardown);

	return egg_tests_run_with_loop ();
}
