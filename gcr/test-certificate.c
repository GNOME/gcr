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
#include "gcr/gcr-oids.h"
#include "gcr/gcr-certificate-extensions-private.h"

#include "egg/egg-testing.h"

#include <glib.h>

#include <errno.h>
#include <string.h>

typedef struct {
	GcrCertificate *certificate;
	GcrCertificate *dsa_cert;
	GcrCertificate *dhansak_cert;
} Test;

static void
setup (Test *test, gconstpointer unused)
{
	gchar *contents;
	gsize n_contents;

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/der-certificate.crt", &contents, &n_contents, NULL))
		g_assert_not_reached ();
	test->certificate = gcr_simple_certificate_new ((const guchar *)contents, n_contents);
	g_assert_nonnull (test->certificate);
	g_free (contents);

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/der-certificate-dsa.cer", &contents, &n_contents, NULL))
		g_assert_not_reached ();
	test->dsa_cert = gcr_simple_certificate_new ((const guchar *)contents, n_contents);
	g_assert_nonnull (test->dsa_cert);
	g_free (contents);

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/dhansak-collabora.cer", &contents, &n_contents, NULL))
		g_assert_not_reached ();
	test->dhansak_cert = gcr_simple_certificate_new ((const guchar *)contents, n_contents);
	g_assert_nonnull (test->dhansak_cert);
	g_free (contents);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_object_unref (test->certificate);
	g_object_unref (test->dsa_cert);
	g_object_unref (test->dhansak_cert);
}

static void
test_issuer_cn (Test *test, gconstpointer unused)
{
	gchar *cn = gcr_certificate_get_issuer_cn (test->certificate);
	g_assert_nonnull (cn);
	g_assert_cmpstr (cn, ==, "http://www.valicert.com/");
	g_free (cn);
}

static void
test_issuer_dn (Test *test, gconstpointer unused)
{
	gchar *dn = gcr_certificate_get_issuer_dn (test->certificate);
	g_assert_nonnull (dn);
	g_assert_cmpstr (dn, ==, "L=ValiCert Validation Network, O=ValiCert, Inc., OU=ValiCert Class 3 Policy Validation Authority, CN=http://www.valicert.com/, EMAIL=info@valicert.com");
	g_free (dn);
}

static void
test_issuer_part (Test *test, gconstpointer unused)
{
	gchar *part = gcr_certificate_get_issuer_part (test->certificate, "l");
	g_assert_nonnull (part);
	g_assert_cmpstr (part, ==, "ValiCert Validation Network");
	g_free (part);
}

static void
test_issuer_raw (Test *test, gconstpointer unused)
{
	gpointer der;
	gsize n_der;

	der = gcr_certificate_get_issuer_raw (test->certificate, &n_der);
	g_assert_nonnull (der);
	egg_assert_cmpsize (n_der, ==, 190);
	g_free (der);
}

static void
test_subject_cn (Test *test, gconstpointer unused)
{
	gchar *cn = gcr_certificate_get_subject_cn (test->certificate);
	g_assert_nonnull (cn);
	g_assert_cmpstr (cn, ==, "http://www.valicert.com/");
	g_free (cn);

	cn = gcr_certificate_get_subject_cn (test->dhansak_cert);
	g_assert_nonnull (cn);
	g_assert_cmpstr (cn, ==, "dhansak.collabora.co.uk");
	g_free (cn);
}

static void
test_subject_dn (Test *test, gconstpointer unused)
{
	gchar *dn = gcr_certificate_get_subject_dn (test->certificate);
	g_assert_nonnull (dn);
	g_assert_cmpstr (dn, ==, "L=ValiCert Validation Network, O=ValiCert, Inc., OU=ValiCert Class 3 Policy Validation Authority, CN=http://www.valicert.com/, EMAIL=info@valicert.com");
	g_free (dn);

	dn = gcr_certificate_get_subject_dn (test->dhansak_cert);
	g_assert_nonnull (dn);
	g_assert_cmpstr (dn, ==, "CN=dhansak.collabora.co.uk, EMAIL=sysadmin@collabora.co.uk");
	g_free (dn);

}

static void
test_subject_part (Test *test, gconstpointer unused)
{
	gchar *part = gcr_certificate_get_subject_part (test->certificate, "OU");
	g_assert_nonnull (part);
	g_assert_cmpstr (part, ==, "ValiCert Class 3 Policy Validation Authority");
	g_free (part);

	part = gcr_certificate_get_subject_part (test->dhansak_cert, "EMAIL");
	g_assert_nonnull (part);
	g_assert_cmpstr (part, ==, "sysadmin@collabora.co.uk");
	g_free (part);

}

static void
test_subject_raw (Test *test, gconstpointer unused)
{
	gpointer der;
	gsize n_der;

	der = gcr_certificate_get_subject_raw (test->certificate, &n_der);
	g_assert_nonnull (der);
	egg_assert_cmpsize (n_der, ==, 190);
	g_free (der);

	der = gcr_certificate_get_subject_raw (test->dhansak_cert, &n_der);
	g_assert_nonnull (der);
	egg_assert_cmpsize (n_der, ==, 77);
	g_free (der);
}

static void
test_issued_date (Test *test, gconstpointer unused)
{
	GDateTime *date = gcr_certificate_get_issued_date (test->certificate);
	g_assert_nonnull (date);
	g_assert_cmpuint (g_date_time_get_year (date), ==, 1999);
	g_assert_cmpuint (g_date_time_get_month (date), ==, 6);
	g_assert_cmpuint (g_date_time_get_day_of_month (date), ==, 26);
	g_date_time_unref (date);
}

static void
test_expiry_date (Test *test, gconstpointer unused)
{
	GDateTime *date = gcr_certificate_get_expiry_date (test->certificate);
	g_assert_nonnull (date);
	g_assert_cmpuint (g_date_time_get_year (date), ==, 2019);
	g_assert_cmpuint (g_date_time_get_month (date), ==, 6);
	g_assert_cmpuint (g_date_time_get_day_of_month (date), ==, 26);
	g_date_time_unref (date);
}

static void
test_version (Test *test, gconstpointer unused)
{
	gulong version = gcr_certificate_get_version (test->certificate);
	g_assert_cmpuint (version, ==, 1);
}

static void
test_serial_number (Test *test, gconstpointer unused)
{
	gsize n_serial;
	guchar *serial;
	gchar *hex;

	serial = gcr_certificate_get_serial_number (test->certificate, &n_serial);
	g_assert_nonnull (serial);
	g_assert_cmpuint (n_serial, ==, 1);
	g_assert_true (memcmp (serial, "\1", n_serial) == 0);
	g_free (serial);

	hex = gcr_certificate_get_serial_number_hex (test->certificate);
	g_assert_nonnull (hex);
	g_assert_cmpstr (hex, ==, "01");
	g_free (hex);
}

static void
test_fingerprint (Test *test, gconstpointer unused)
{
	gsize n_print;
	guchar *print = gcr_certificate_get_fingerprint (test->certificate, G_CHECKSUM_MD5, &n_print);
	g_assert_nonnull (print);
	g_assert_cmpuint (n_print, ==, g_checksum_type_get_length (G_CHECKSUM_MD5));
	g_assert_true (memcmp (print, "\xa2\x6f\x53\xb7\xee\x40\xdb\x4a\x68\xe7\xfa\x18\xd9\x10\x4b\x72", n_print) == 0);
	g_free (print);
}

static void
test_fingerprint_hex (Test *test, gconstpointer unused)
{
	gchar *print = gcr_certificate_get_fingerprint_hex (test->certificate, G_CHECKSUM_MD5);
	g_assert_nonnull (print);
	g_assert_cmpstr (print, ==, "A2 6F 53 B7 EE 40 DB 4A 68 E7 FA 18 D9 10 4B 72");
	g_free (print);
}

static void
test_certificate_key_size (Test *test, gconstpointer unused)
{
	guint key_size = gcr_certificate_get_key_size (test->certificate);
	g_assert_cmpuint (key_size, ==, 1024);

	key_size = gcr_certificate_get_key_size (test->dsa_cert);
	g_assert_cmpuint (key_size, ==, 1024);
}

static void
test_certificate_is_issuer (Test *test, gconstpointer unused)
{
	gboolean ret = gcr_certificate_is_issuer (test->certificate, test->certificate);
	g_assert_true (ret);

	ret = gcr_certificate_is_issuer (test->certificate, test->dsa_cert);
	g_assert_false (ret);
}

static void
test_list_extensions (Test *test,
                      gconstpointer unused)
{
	GcrCertificateExtensionList *extensions;
	GListModel *ext_list;
	unsigned int n_items;
	GcrCertificateExtension *extension;

	extensions = gcr_certificate_list_extensions (test->dsa_cert);
	g_assert_nonnull (extensions);

	/* Basic GListModel API */
	ext_list = G_LIST_MODEL (extensions);
	g_assert_true (g_list_model_get_item_type (ext_list) == GCR_TYPE_CERTIFICATE_EXTENSION);
	g_assert_cmpint (g_list_model_get_n_items (ext_list), ==, 9);
	g_object_get (extensions, "n-items", &n_items, NULL);
	g_assert_cmpint (n_items, ==, 9);
	g_assert_null (g_list_model_get_item (ext_list, 9));

	/* check get_extension() for all elements */
	for (unsigned int i = 0; i < n_items; i++) {
		extension = gcr_certificate_extension_list_get_extension (extensions, i);
		g_assert_nonnull (extension);
		g_assert_true (GCR_IS_CERTIFICATE_EXTENSION (extension));
	}

	/* find_by_oid() */
	extension = gcr_certificate_extension_list_find_by_oid (extensions,
	                                                        g_quark_to_string (GCR_OID_BASIC_CONSTRAINTS));
	g_assert_nonnull (extension);

	extension = gcr_certificate_extension_list_find_by_oid (extensions,
	                                                        g_quark_to_string (GCR_OID_SUBJECT_ALT_NAME));
	g_assert_null (extension);

	g_object_unref (extensions);
}

static void
test_basic_constraints (Test *test,
                        gconstpointer unused)
{
	gboolean is_ca = TRUE;
	gint path_len = 0;

	if (!gcr_certificate_get_basic_constraints (test->dsa_cert, &is_ca, &path_len))
		g_assert_not_reached ();

	g_assert_false (is_ca);
	g_assert_cmpint (path_len, ==, -1);
}

static void
test_authority_key_identifier (Test       *test,
                               const void *unused)
{
	GcrCertificateExtensionList *extensions;
	GcrCertificateExtension *ext;
	GcrCertificateExtensionAuthorityKeyIdentifier *ext_aki;
	GBytes *keyid;
	guint8 expected_keyid[] = {
		0x67, 0xb0, 0xd5, 0x59, 0x84, 0xd2, 0x2a, 0xa1, 0xe8, 0x29,
		0x2d, 0x4a, 0xd8, 0xc8, 0xd2, 0xb0, 0x0f, 0x72, 0xf7, 0x75
	};

	extensions = gcr_certificate_list_extensions (test->dsa_cert);
	ext = gcr_certificate_extension_list_find_by_oid (extensions,
	                                                  g_quark_to_string (GCR_OID_AUTHORITY_KEY_IDENTIFIER));
	g_assert_nonnull (ext);
	g_assert_true (GCR_IS_CERTIFICATE_EXTENSION_AUTHORITY_KEY_IDENTIFIER (ext));
	ext_aki = GCR_CERTIFICATE_EXTENSION_AUTHORITY_KEY_IDENTIFIER (ext);

	keyid = gcr_certificate_extension_authority_key_identifier_get_key_id (ext_aki);
	g_assert_nonnull (keyid);
	g_assert_cmpmem (g_bytes_get_data (keyid, NULL), g_bytes_get_size (keyid),
	                 expected_keyid, G_N_ELEMENTS (expected_keyid));

	g_object_unref (extensions);
}

static void
test_interface_elements (Test *test,
                         gconstpointer unused)
{
	GList* sections = gcr_certificate_get_interface_elements (test->dsa_cert);
	for (GList *l = sections; l != NULL; l = l->next) {
		GcrCertificateSection *section = l->data;
		GListModel *fields;

		gcr_certificate_section_get_flags (section);
		g_assert_nonnull (gcr_certificate_section_get_label (section));
		fields = gcr_certificate_section_get_fields (section);
		g_assert_nonnull (fields);
		g_assert_true (g_list_model_get_item_type (fields) == GCR_TYPE_CERTIFICATE_FIELD);
		for (guint i = 0; i < g_list_model_get_n_items (fields); i++) {
			GValue val = G_VALUE_INIT;
			GType value_type;
			GcrCertificateField *field = g_list_model_get_item (fields, i);
			g_assert_nonnull (gcr_certificate_field_get_label (field));
			value_type = gcr_certificate_field_get_value_type (field);
			g_value_init (&val, value_type);
			g_assert_true (gcr_certificate_field_get_value (field, &val));
			g_value_unset (&val);
			g_assert_true (gcr_certificate_field_get_section (field) == section);
			g_object_unref (field);
		}
	}

	g_list_free_full (sections, (GDestroyNotify) g_object_unref);
}

static void
test_subject_alt_name (void)
{
	const guint8 extension[] = {
		0x30, 0x40,
		0x87, 0x04, 0xC0, 0x00, 0x02, 0x01,
		0x82, 0x10, 0x74, 0x65, 0x73, 0x74, 0x2E, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D,
		0xA0, 0x13, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x08, 0x05, 0xA0, 0x07, 0x0C, 0x05, 0x63, 0x40, 0x61, 0x2E, 0x62,
		0xA0, 0x11, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x08, 0x07, 0xA0, 0x05, 0x16, 0x03, 0x61, 0x2E, 0x62
	};
	GBytes *bytes;
	GcrCertificateExtension *ext;
	GcrCertificateExtensionSubjectAltName *san_ext;
	unsigned int n_names;
	GcrGeneralName *name;
	GcrGeneralName *element;
	GcrGeneralNameType type;

	bytes = g_bytes_new_static (extension, sizeof(extension));
	ext = _gcr_certificate_extension_subject_alt_name_parse (GCR_OID_SUBJECT_ALT_NAME,
	                                                         TRUE,
	                                                         g_steal_pointer (&bytes),
	                                                         NULL);
	g_assert_nonnull (ext);

	san_ext = GCR_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (ext);
	n_names = g_list_model_get_n_items (G_LIST_MODEL (san_ext));
	g_assert_cmpint (n_names, ==, 4);

	name = gcr_certificate_extension_subject_alt_name_get_name (san_ext, 0);
	type = _gcr_general_name_get_name_type (name);
	g_assert_cmpint (type, ==, GCR_GENERAL_NAME_IP);
	name = gcr_certificate_extension_subject_alt_name_get_name (san_ext, 1);
	type = _gcr_general_name_get_name_type (name);
	g_assert_cmpint (type, ==, GCR_GENERAL_NAME_DNS);
	name = gcr_certificate_extension_subject_alt_name_get_name (san_ext, 2);
	type = _gcr_general_name_get_name_type (name);
	g_assert_cmpint (type, ==, GCR_GENERAL_NAME_OTHER);
	name = gcr_certificate_extension_subject_alt_name_get_name (san_ext, 3);
	type = _gcr_general_name_get_name_type (name);
	g_assert_cmpint (type, ==, GCR_GENERAL_NAME_OTHER);

	element = g_list_model_get_item (G_LIST_MODEL (san_ext), 3);
	g_assert_nonnull (element);
	g_assert_true (name == element);
	g_assert_null (g_list_model_get_item (G_LIST_MODEL (san_ext), 4));
	g_object_unref (element);

	g_object_unref (ext);
}

static void
test_key_usage (void)
{
	const guint8 usage[] = {
	        // ASN.1 encoded BIT STRING (16 bit) 1000011110000000
	        0x03, 0x03, 0x00, 0x87, 0x80
	};
	GBytes *bytes;
	GcrCertificateExtension *ext;
	gulong key_usage;

	bytes = g_bytes_new_static (usage, sizeof(usage));
	ext = _gcr_certificate_extension_key_usage_parse (GCR_OID_KEY_USAGE,
	                                                  TRUE,
	                                                  g_steal_pointer (&bytes),
	                                                  NULL);
	g_assert_nonnull (ext);
	g_assert_true (GCR_IS_CERTIFICATE_EXTENSION_KEY_USAGE (ext));

	key_usage = gcr_certificate_extension_key_usage_get_usages (GCR_CERTIFICATE_EXTENSION_KEY_USAGE (ext));
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_DIGITAL_SIGNATURE, ==, GCR_KEY_USAGE_DIGITAL_SIGNATURE);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_NON_REPUDIATION, ==, 0);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_KEY_ENCIPHERMENT, ==, 0);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_DATA_ENCIPHERMENT, ==, 0);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_KEY_AGREEMENT, ==, 0);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_KEY_CERT_SIGN, ==, GCR_KEY_USAGE_KEY_CERT_SIGN);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_CRL_SIGN, ==, GCR_KEY_USAGE_CRL_SIGN);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_ENCIPHER_ONLY, ==, GCR_KEY_USAGE_ENCIPHER_ONLY);
	g_assert_cmpint (key_usage & GCR_KEY_USAGE_DECIPHER_ONLY, ==, GCR_KEY_USAGE_DECIPHER_ONLY);

	g_object_unref (ext);
}

static void
test_certificate_policies (void)
{
	const guint8 policies[] = {
		0x30, 0x41, /* Policies is a SEQUENCE of 65 bytes */
		/* Policy 1: DigiCert ANSI Organizational Identifier (2.16.840.1.114412.2.1) */
		0x30, 0x0b,
		0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x86, 0xfd, 0x6c, 0x02, 0x01,
		/* Policy 2: Extended Validation (2.23.140.1.1) */
		0x30, 0x32,
		0x06, 0x05, 0x67, 0x81, 0x0c, 0x01, 0x01,
		/* Policy 2 qualifier 1: Practices Statement (1.3.6.1.5.5.7.2.1) */
		0x30, 0x29, 0x30, 0x27,
		0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x01,
		/* http://www.digicert.com/CPS */
		0x16, 0x1b, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77,
		0x77, 0x77, 0x2e, 0x64, 0x69, 0x67, 0x69, 0x63, 0x65, 0x72,
		0x74, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x43, 0x50, 0x53,
	};
	GBytes *bytes;
	GcrCertificateExtension *ext;
	GcrCertificateExtensionCertificatePolicies *ext_cp;
	GcrCertificatePolicy *policy;
	GcrCertificatePolicyQualifier *qualifier;

	bytes = g_bytes_new_static (policies, sizeof(policies));
	ext = _gcr_certificate_extension_certificate_policies_parse (GCR_OID_CERTIFICATE_POLICIES,
	                                                             TRUE,
	                                                             bytes,
	                                                             NULL);
	g_assert_nonnull (ext);
	g_assert_true (GCR_IS_CERTIFICATE_EXTENSION_CERTIFICATE_POLICIES (ext));

	ext_cp = GCR_CERTIFICATE_EXTENSION_CERTIFICATE_POLICIES (ext);

	g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (ext_cp)), ==, 2);
	g_assert_null (g_list_model_get_item (G_LIST_MODEL (ext_cp), 2));

	policy = gcr_certificate_extension_certificate_policies_get_policy (ext_cp, 0);
	g_assert_nonnull (policy);
	g_assert_true (GCR_IS_CERTIFICATE_POLICY (policy));
	g_assert_cmpstr (gcr_certificate_policy_get_oid (policy), ==, "2.16.840.1.114412.2.1");
	g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (policy)), ==, 0);
	g_assert_null (g_list_model_get_item (G_LIST_MODEL (policy), 0));

	policy = gcr_certificate_extension_certificate_policies_get_policy (ext_cp, 1);
	g_assert_nonnull (policy);
	g_assert_true (GCR_IS_CERTIFICATE_POLICY (policy));
	g_assert_cmpstr (gcr_certificate_policy_get_oid (policy), ==, "2.23.140.1.1");

	g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (policy)), ==, 1);
	qualifier = g_list_model_get_item (G_LIST_MODEL (policy), 0);
	g_assert_true (GCR_IS_CERTIFICATE_POLICY_QUALIFIER (qualifier));
	g_assert_cmpstr (gcr_certificate_policy_qualifier_get_oid (qualifier), ==, "1.3.6.1.5.5.7.2.1");
	g_object_unref (qualifier);
	g_assert_null (g_list_model_get_item (G_LIST_MODEL (policy), 1));

	g_object_unref (ext);
}

static void
test_authority_info_address (void)
{
	const guint8 aia[] = {
		0x30, 0x7a, /* AIA is SEQUENCE of 114 bytes */
		/* 1: "http://ocsp.digicert.com" (method: OCSP) */
		0x30, 0x24,
		0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, /* OCSP */
		0x86, 0x18, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x6f,
		0x63, 0x73, 0x70, 0x2e, 0x64, 0x69, 0x67, 0x69, 0x63, 0x65,
		0x72, 0x74, 0x2e, 0x63, 0x6f, 0x6d,
		/* 2: "http://cacerts.digicert.com/DigiCertSHA2ExtendedValidationServerCA.crt" (method: CA Issuer) */
		0x30, 0x52,
		0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02, /* CA issuer */
		0x86, 0x46, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x63,
		0x61, 0x63, 0x65, 0x72, 0x74, 0x73, 0x2e, 0x64, 0x69, 0x67,
		0x69, 0x63, 0x65, 0x72, 0x74, 0x2e, 0x63, 0x6f, 0x6d, 0x2f,
		0x44, 0x69, 0x67, 0x69, 0x43, 0x65, 0x72, 0x74, 0x53, 0x48,
		0x41, 0x32, 0x45, 0x78, 0x74, 0x65, 0x6e, 0x64, 0x65, 0x64,
		0x56, 0x61, 0x6c, 0x69, 0x64, 0x61, 0x74, 0x69, 0x6f, 0x6e,
		0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x43, 0x41, 0x2e, 0x63,
		0x72, 0x74,
	};
	GBytes *bytes;
	GcrCertificateExtension *ext;
	GcrCertificateExtensionAuthorityInfoAccess *ext_aia;
	GcrAccessDescription *description;
	GcrGeneralName *location;
	const char *method_oid, *location_str;

	bytes = g_bytes_new_static (aia, sizeof(aia));
	ext = _gcr_certificate_extension_authority_info_access_parse (GCR_OID_AUTHORITY_INFO_ACCESS,
	                                                              TRUE,
	                                                              bytes,
	                                                              NULL);
	g_assert_nonnull (ext);
	g_assert_true (GCR_IS_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS (ext));

	ext_aia = GCR_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS (ext);

	g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (ext_aia)), ==, 2);
	g_assert_null (g_list_model_get_item (G_LIST_MODEL (ext_aia), 2));

	description = gcr_certificate_extension_authority_info_access_get_description (ext_aia, 0);
	g_assert_nonnull (description);
	method_oid = gcr_access_description_get_method_oid (description);
	g_assert_cmpstr (method_oid, ==, "1.3.6.1.5.5.7.48.1");
	location = gcr_access_description_get_location (description);
	location_str = gcr_general_name_get_value (location);
	g_assert_cmpstr (location_str, ==, "http://ocsp.digicert.com");

	description = gcr_certificate_extension_authority_info_access_get_description (ext_aia, 1);
	g_assert_nonnull (description);
	method_oid = gcr_access_description_get_method_oid (description);
	g_assert_cmpstr (method_oid, ==, "1.3.6.1.5.5.7.48.2");
	location = gcr_access_description_get_location (description);
	location_str = gcr_general_name_get_value (location);
	g_assert_cmpstr (location_str, ==, "http://cacerts.digicert.com/DigiCertSHA2ExtendedValidationServerCA.crt");

	g_object_unref (ext);
}

static void
test_crl_distribution_points (void)
{
	const guint8 cdp[] = {
		0x30, 0x6c,
		0x30, 0x34,
		0xa0, 0x32, 0xa0, 0x30, 0x86, 0x2e, 0x68, 0x74, 0x74, 0x70,
		0x3a, 0x2f, 0x2f, 0x63, 0x72, 0x6c, 0x33, 0x2e, 0x64, 0x69,
		0x67, 0x69, 0x63, 0x65, 0x72, 0x74, 0x2e, 0x63, 0x6f, 0x6d,
		0x2f, 0x73, 0x68, 0x61, 0x32, 0x2d, 0x65, 0x76, 0x2d, 0x73,
		0x65, 0x72, 0x76, 0x65, 0x72, 0x2d, 0x67, 0x33, 0x2e, 0x63,
		0x72, 0x6c, 0x30, 0x34, 0xa0, 0x32, 0xa0, 0x30, 0x86, 0x2e,
		0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x63, 0x72, 0x6c,
		0x34, 0x2e, 0x64, 0x69, 0x67, 0x69, 0x63, 0x65, 0x72, 0x74,
		0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x73, 0x68, 0x61, 0x32, 0x2d,
		0x65, 0x76, 0x2d, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x2d,
		0x67, 0x33, 0x2e, 0x63, 0x72, 0x6c,
	};
	GBytes *bytes;
	GcrCertificateExtension *ext;
	GcrCertificateExtensionCrlDistributionPoints *ext_cdp;
	GcrDistributionPoint *endpoint;
	GcrGeneralNames *full_name;
	GcrGeneralName *first_full_name;

	bytes = g_bytes_new_static (cdp, sizeof(cdp));
	ext = _gcr_certificate_extension_crl_distribution_points_parse (GCR_OID_CRL_DISTRIBUTION_POINTS,
	                                                                TRUE,
	                                                                bytes,
	                                                                NULL);
	g_assert_nonnull (ext);
	g_assert_true (GCR_IS_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS (ext));

	ext_cdp = GCR_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS (ext);

	g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (ext_cdp)), ==, 2);
	g_assert_null (g_list_model_get_item (G_LIST_MODEL (ext_cdp), 2));

	endpoint = gcr_certificate_extension_crl_distribution_points_get_distribution_point (ext_cdp, 0);
	g_assert_nonnull (endpoint);
	g_assert_true (GCR_IS_DISTRIBUTION_POINT (endpoint));

	full_name = gcr_distribution_point_get_full_name (endpoint);
	g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (full_name)), ==, 1);
	g_assert_null (g_list_model_get_item (G_LIST_MODEL (full_name), 2));

	first_full_name = gcr_general_names_get_name (full_name, 0);
	g_assert_nonnull (first_full_name);
	g_assert_cmpstr (gcr_general_name_get_value (first_full_name), ==, "http://crl3.digicert.com/sha2-ev-server-g3.crl");

	g_object_unref (ext);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-certificate");

	g_test_add ("/gcr/certificate/issuer_cn", Test, NULL, setup, test_issuer_cn, teardown);
	g_test_add ("/gcr/certificate/issuer_dn", Test, NULL, setup, test_issuer_dn, teardown);
	g_test_add ("/gcr/certificate/issuer_part", Test, NULL, setup, test_issuer_part, teardown);
	g_test_add ("/gcr/certificate/issuer_raw", Test, NULL, setup, test_issuer_raw, teardown);
	g_test_add ("/gcr/certificate/subject_cn", Test, NULL, setup, test_subject_cn, teardown);
	g_test_add ("/gcr/certificate/subject_dn", Test, NULL, setup, test_subject_dn, teardown);
	g_test_add ("/gcr/certificate/subject_part", Test, NULL, setup, test_subject_part, teardown);
	g_test_add ("/gcr/certificate/subject_raw", Test, NULL, setup, test_subject_raw, teardown);
	g_test_add ("/gcr/certificate/issued_date", Test, NULL, setup, test_issued_date, teardown);
	g_test_add ("/gcr/certificate/expiry_date", Test, NULL, setup, test_expiry_date, teardown);
	g_test_add ("/gcr/certificate/version", Test, NULL, setup, test_version, teardown);
	g_test_add ("/gcr/certificate/serial_number", Test, NULL, setup, test_serial_number, teardown);
	g_test_add ("/gcr/certificate/fingerprint", Test, NULL, setup, test_fingerprint, teardown);
	g_test_add ("/gcr/certificate/fingerprint_hex", Test, NULL, setup, test_fingerprint_hex, teardown);
	g_test_add ("/gcr/certificate/key_size", Test, NULL, setup, test_certificate_key_size, teardown);
	g_test_add ("/gcr/certificate/is_issuer", Test, NULL, setup, test_certificate_is_issuer, teardown);
	g_test_add ("/gcr/certificate/interface_elements", Test, NULL, setup, test_interface_elements, teardown);
	/* Extensions */
	g_test_add ("/gcr/certificate/list_extensions", Test, NULL, setup, test_list_extensions, teardown);
	g_test_add ("/gcr/certificate/basic_constraints", Test, NULL, setup, test_basic_constraints, teardown);
	g_test_add ("/gcr/certificate/authority_key_identifier", Test, NULL, setup, test_authority_key_identifier, teardown);
	g_test_add_func ("/gcr/certificate/subject_alt_name", test_subject_alt_name);
	g_test_add_func ("/gcr/certificate/key_usage", test_key_usage);
	g_test_add_func ("/gcr/certificate/certificate_policies", test_certificate_policies);
	g_test_add_func ("/gcr/certificate/authority_info_adress", test_authority_info_address);
	g_test_add_func ("/gcr/certificate/crl_distribution_points", test_crl_distribution_points);

	return g_test_run ();
}
