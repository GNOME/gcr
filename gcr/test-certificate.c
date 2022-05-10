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
#include "gcr/gcr-certificate-extensions.h"

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
	g_assert (test->certificate);
	g_free (contents);

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/der-certificate-dsa.cer", &contents, &n_contents, NULL))
		g_assert_not_reached ();
	test->dsa_cert = gcr_simple_certificate_new ((const guchar *)contents, n_contents);
	g_assert (test->dsa_cert);
	g_free (contents);

	if (!g_file_get_contents (SRCDIR "/gcr/fixtures/dhansak-collabora.cer", &contents, &n_contents, NULL))
		g_assert_not_reached ();
	test->dhansak_cert = gcr_simple_certificate_new ((const guchar *)contents, n_contents);
	g_assert (test->dhansak_cert);
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
	g_assert (cn);
	g_assert_cmpstr (cn, ==, "http://www.valicert.com/");
	g_free (cn);
}

static void
test_issuer_dn (Test *test, gconstpointer unused)
{
	gchar *dn = gcr_certificate_get_issuer_dn (test->certificate);
	g_assert (dn);
	g_assert_cmpstr (dn, ==, "L=ValiCert Validation Network, O=ValiCert, Inc., OU=ValiCert Class 3 Policy Validation Authority, CN=http://www.valicert.com/, EMAIL=info@valicert.com");
	g_free (dn);
}

static void
test_issuer_part (Test *test, gconstpointer unused)
{
	gchar *part = gcr_certificate_get_issuer_part (test->certificate, "l");
	g_assert (part);
	g_assert_cmpstr (part, ==, "ValiCert Validation Network");
	g_free (part);
}

static void
test_issuer_raw (Test *test, gconstpointer unused)
{
	gpointer der;
	gsize n_der;

	der = gcr_certificate_get_issuer_raw (test->certificate, &n_der);
	g_assert (der);
	egg_assert_cmpsize (n_der, ==, 190);
	g_free (der);
}

static void
test_subject_cn (Test *test, gconstpointer unused)
{
	gchar *cn = gcr_certificate_get_subject_cn (test->certificate);
	g_assert (cn);
	g_assert_cmpstr (cn, ==, "http://www.valicert.com/");
	g_free (cn);

	cn = gcr_certificate_get_subject_cn (test->dhansak_cert);
	g_assert (cn);
	g_assert_cmpstr (cn, ==, "dhansak.collabora.co.uk");
	g_free (cn);
}

static void
test_subject_dn (Test *test, gconstpointer unused)
{
	gchar *dn = gcr_certificate_get_subject_dn (test->certificate);
	g_assert (dn);
	g_assert_cmpstr (dn, ==, "L=ValiCert Validation Network, O=ValiCert, Inc., OU=ValiCert Class 3 Policy Validation Authority, CN=http://www.valicert.com/, EMAIL=info@valicert.com");
	g_free (dn);

	dn = gcr_certificate_get_subject_dn (test->dhansak_cert);
	g_assert (dn);
	g_assert_cmpstr (dn, ==, "CN=dhansak.collabora.co.uk, EMAIL=sysadmin@collabora.co.uk");
	g_free (dn);

}

static void
test_subject_part (Test *test, gconstpointer unused)
{
	gchar *part = gcr_certificate_get_subject_part (test->certificate, "OU");
	g_assert (part);
	g_assert_cmpstr (part, ==, "ValiCert Class 3 Policy Validation Authority");
	g_free (part);

	part = gcr_certificate_get_subject_part (test->dhansak_cert, "EMAIL");
	g_assert (part);
	g_assert_cmpstr (part, ==, "sysadmin@collabora.co.uk");
	g_free (part);

}

static void
test_subject_raw (Test *test, gconstpointer unused)
{
	gpointer der;
	gsize n_der;

	der = gcr_certificate_get_subject_raw (test->certificate, &n_der);
	g_assert (der);
	egg_assert_cmpsize (n_der, ==, 190);
	g_free (der);

	der = gcr_certificate_get_subject_raw (test->dhansak_cert, &n_der);
	g_assert (der);
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
	g_assert (date);
	g_assert_cmpuint (g_date_time_get_year (date), ==, 2019);
	g_assert_cmpuint (g_date_time_get_month (date), ==, 6);
	g_assert_cmpuint (g_date_time_get_day_of_month (date), ==, 26);
	g_date_time_unref (date);
}

static void
test_serial_number (Test *test, gconstpointer unused)
{
	gsize n_serial;
	guchar *serial;
	gchar *hex;

	serial = gcr_certificate_get_serial_number (test->certificate, &n_serial);
	g_assert (serial);
	g_assert_cmpuint (n_serial, ==, 1);
	g_assert (memcmp (serial, "\1", n_serial) == 0);
	g_free (serial);

	hex = gcr_certificate_get_serial_number_hex (test->certificate);
	g_assert (hex);
	g_assert_cmpstr (hex, ==, "01");
	g_free (hex);
}

static void
test_fingerprint (Test *test, gconstpointer unused)
{
	gsize n_print;
	guchar *print = gcr_certificate_get_fingerprint (test->certificate, G_CHECKSUM_MD5, &n_print);
	g_assert (print);
	g_assert_cmpuint (n_print, ==, g_checksum_type_get_length (G_CHECKSUM_MD5));
	g_assert (memcmp (print, "\xa2\x6f\x53\xb7\xee\x40\xdb\x4a\x68\xe7\xfa\x18\xd9\x10\x4b\x72", n_print) == 0);
	g_free (print);
}

static void
test_fingerprint_hex (Test *test, gconstpointer unused)
{
	gchar *print = gcr_certificate_get_fingerprint_hex (test->certificate, G_CHECKSUM_MD5);
	g_assert (print);
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
	g_assert (ret == TRUE);

	ret = gcr_certificate_is_issuer (test->certificate, test->dsa_cert);
	g_assert (ret == FALSE);
}

static void
test_basic_constraints (Test *test,
                        gconstpointer unused)
{
	gboolean is_ca = TRUE;
	gint path_len = 0;

	if (!gcr_certificate_get_basic_constraints (test->dsa_cert, &is_ca, &path_len))
		g_assert_not_reached ();

	g_assert (is_ca == FALSE);
	g_assert (path_len == -1);
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
		g_assert (gcr_certificate_section_get_label (section) != NULL);
		fields = gcr_certificate_section_get_fields (section);
		g_assert (fields != NULL);
		g_assert (g_list_model_get_item_type (fields) == GCR_TYPE_CERTIFICATE_FIELD);
		for (guint i = 0; i < g_list_model_get_n_items (fields); i++) {
			GValue val = G_VALUE_INIT;
			GType value_type;
			GcrCertificateField *field = g_list_model_get_item (fields, i);
			g_assert (gcr_certificate_field_get_label (field) != NULL);
			value_type = gcr_certificate_field_get_value_type (field);
			g_value_init (&val, value_type);
			g_assert (gcr_certificate_field_get_value (field, &val));
			g_value_unset (&val);
			g_assert (gcr_certificate_field_get_section (field) == section);
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
	GArray *result;
	GcrGeneralName *general_name;

	bytes = g_bytes_new_static (extension, sizeof(extension));
	result = _gcr_certificate_extension_subject_alt_name (bytes);
	g_bytes_unref (bytes);

	g_assert_nonnull (result);
	g_assert_cmpint (result->len, ==, 4);
	general_name = &g_array_index (result, GcrGeneralName, 0);
	g_assert_cmpint (general_name->type, ==, GCR_GENERAL_NAME_IP);
	general_name = &g_array_index (result, GcrGeneralName, 1);
	g_assert_cmpint (general_name->type, ==, GCR_GENERAL_NAME_DNS);
	general_name = &g_array_index (result, GcrGeneralName, 2);
	g_assert_cmpint (general_name->type, ==, GCR_GENERAL_NAME_OTHER);
	general_name = &g_array_index (result, GcrGeneralName, 3);
	g_assert_cmpint (general_name->type, ==, GCR_GENERAL_NAME_OTHER);
	_gcr_general_names_free (result);
}

static void
test_key_usage (void)
{
    const guint8 usage[] = {
            // ASN.1 encoded BIT STRING (16 bit) 1000011110000000
            0x03, 0x03, 0x00, 0x87, 0x80
    };
    GBytes *bytes;
    gboolean ret;
    gulong key_usage;

    bytes = g_bytes_new_static (usage, sizeof(usage));
    ret = _gcr_certificate_extension_key_usage (bytes, &key_usage);
    g_bytes_unref (bytes);

    g_assert (ret == TRUE);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_DIGITAL_SIGNATURE, ==, GCR_KEY_USAGE_DIGITAL_SIGNATURE);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_NON_REPUDIATION, ==, 0);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_KEY_ENCIPHERMENT, ==, 0);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_DATA_ENCIPHERMENT, ==, 0);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_KEY_AGREEMENT, ==, 0);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_KEY_CERT_SIGN, ==, GCR_KEY_USAGE_KEY_CERT_SIGN);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_CRL_SIGN, ==, GCR_KEY_USAGE_CRL_SIGN);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_ENCIPHER_ONLY, ==, GCR_KEY_USAGE_ENCIPHER_ONLY);
    g_assert_cmpint (key_usage & GCR_KEY_USAGE_DECIPHER_ONLY, ==, GCR_KEY_USAGE_DECIPHER_ONLY);
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
	g_test_add ("/gcr/certificate/serial_number", Test, NULL, setup, test_serial_number, teardown);
	g_test_add ("/gcr/certificate/fingerprint", Test, NULL, setup, test_fingerprint, teardown);
	g_test_add ("/gcr/certificate/fingerprint_hex", Test, NULL, setup, test_fingerprint_hex, teardown);
	g_test_add ("/gcr/certificate/key_size", Test, NULL, setup, test_certificate_key_size, teardown);
	g_test_add ("/gcr/certificate/is_issuer", Test, NULL, setup, test_certificate_is_issuer, teardown);
	g_test_add ("/gcr/certificate/basic_constraints", Test, NULL, setup, test_basic_constraints, teardown);
	g_test_add ("/gcr/certificate/interface_elements", Test, NULL, setup, test_interface_elements, teardown);
	g_test_add_func ("/gcr/certificate/subject_alt_name", test_subject_alt_name);
	g_test_add_func ("/gcr/certificate/key_usage", test_key_usage);

	return g_test_run ();
}
