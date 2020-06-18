/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */
#include "config.h"

#include "gcr-certificate-extensions.h"

#include "gcr/gcr-oids.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"

#include <glib/gi18n-lib.h>

GBytes *
_gcr_certificate_extension_find (GNode *cert,
                                 GQuark oid,
                                 gboolean *critical)
{
	GNode *node;
	gint index;

	g_return_val_if_fail (cert != NULL, NULL);

	/* Extensions */
	for (index = 1; TRUE; ++index) {
		node = egg_asn1x_node (cert, "tbsCertificate", "extensions", index, NULL);
		if (node == NULL)
			return NULL;

		/* Dig out the OID */
		if (egg_asn1x_get_oid_as_quark (egg_asn1x_node (node, "extnID", NULL)) == oid) {

			if (critical) {
				if (!egg_asn1x_get_boolean (egg_asn1x_node (node, "critical", NULL), critical))
					g_return_val_if_reached (NULL);
			}

			/* Extension value */
			return egg_asn1x_get_string_as_bytes (egg_asn1x_node (node, "extnValue", NULL));
		}
	}

	g_assert_not_reached ();
}

gboolean
_gcr_certificate_extension_basic_constraints (GBytes *data,
                                              gboolean *is_ca,
                                              gint *path_len)
{
	gboolean ret = TRUE;
	GNode *asn = NULL;
	GNode *node;
	gulong value;

	g_return_val_if_fail (data != NULL, FALSE);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "BasicConstraints", data);
	if (asn == NULL)
		return FALSE;

	if (path_len) {
		node = egg_asn1x_node (asn, "pathLenConstraint", NULL);
		if (!egg_asn1x_have (node))
			*path_len = -1;
		else if (!egg_asn1x_get_integer_as_ulong (node, &value))
			ret = FALSE;
		else
			*path_len = value;
	}

	if (is_ca) {
		node = egg_asn1x_node (asn, "cA", NULL);
		if (!egg_asn1x_have (node))
			*is_ca = FALSE;
		else if (!egg_asn1x_get_boolean (node, is_ca))
			ret = FALSE;
	}

	egg_asn1x_destroy (asn);
	return ret;
}

GQuark *
_gcr_certificate_extension_extended_key_usage (GBytes *data)
{
	GNode *asn = NULL;
	GNode *node;
	GArray *array;
	GQuark oid;
	int i;

	g_return_val_if_fail (data != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "ExtKeyUsageSyntax", data);
	if (asn == NULL)
		return NULL;

	array = g_array_new (TRUE, TRUE, sizeof (GQuark));
	for (i = 0; TRUE; ++i) {
		node = egg_asn1x_node (asn, i + 1, NULL);
		if (node == NULL)
			break;
		oid = egg_asn1x_get_oid_as_quark (node);
		g_array_append_val (array, oid);
	}

	egg_asn1x_destroy (asn);
	return (GQuark*)g_array_free (array, FALSE);
}

gpointer
_gcr_certificate_extension_subject_key_identifier (GBytes *data,
                                                   gsize *n_keyid)
{
	GNode *asn = NULL;
	gpointer result;

	g_return_val_if_fail (data != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SubjectKeyIdentifier", data);
	if (asn == NULL)
		return NULL;

	result = egg_asn1x_get_string_as_raw (asn, g_realloc, n_keyid);
	egg_asn1x_destroy (asn);

	return result;
}

static gulong
_gcr_reverse_bits(gulong num, guint n_bits)
{
    gulong reverse_num = 0;
    guint i;
    for (i = 0; i < n_bits; i++) {
        if ((num & (1 << i)))
            reverse_num |= 1 << ((n_bits - 1) - i);
    }
    return reverse_num;
}

gboolean
_gcr_certificate_extension_key_usage (GBytes *data,
                                      gulong *key_usage)
{
	GNode *asn = NULL;
	gboolean ret = TRUE;
	guint n_bits;

	g_return_val_if_fail (data != NULL, FALSE);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "KeyUsage", data);
	if (asn == NULL)
		return FALSE;

	ret = egg_asn1x_get_bits_as_ulong (asn, key_usage, &n_bits);
	egg_asn1x_destroy (asn);
	*key_usage = _gcr_reverse_bits(*key_usage, n_bits);
	return ret;
}

static void
general_name_parse_other (GNode *node, GcrGeneralName *general)
{
	GNode *decode = NULL;
	GQuark oid;
	GNode *any;

	general->type = GCR_GENERAL_NAME_OTHER;
	general->description = _("Other Name");
	general->display = NULL;

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (node, "type-id", NULL));
	any = egg_asn1x_node (node, "value", NULL);

	if (any == NULL)
		return;

	if (oid == GCR_OID_ALT_NAME_XMPP_ADDR) {
		general->description = _("XMPP Addr");
		decode = egg_asn1x_get_any_as_string (any, EGG_ASN1X_UTF8_STRING);
		general->display = egg_asn1x_get_string_as_utf8 (decode, g_realloc);
	} else if (oid == GCR_OID_ALT_NAME_DNS_SRV) {
		general->description = _("DNS SRV");
		decode = egg_asn1x_get_any_as_string (any, EGG_ASN1X_IA5_STRING);
		general->display = egg_asn1x_get_string_as_utf8 (decode, g_realloc);
	}

	egg_asn1x_destroy (decode);
}

static void
general_name_parse_rfc822 (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_RFC822;
	general->description = _("Email");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_dns (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_DNS;
	general->description = _("DNS");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_x400 (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_X400;
	general->description = _("X400 Address");
}

static void
general_name_parse_dn (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_DNS;
	general->description = _("Directory Name");
	general->display = egg_dn_read (node);
}

static void
general_name_parse_edi (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_EDI;
	general->description = _("EDI Party Name");
}

static void
general_name_parse_uri (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_URI;
	general->description = _("URI");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_ip (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_IP;
	general->description = _("IP Address");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_registered (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_REGISTERED_ID;
	general->description = _("Registered ID");
	general->display = egg_asn1x_get_oid_as_string (node);
}

GArray*
_gcr_certificate_extension_subject_alt_name (GBytes *data)
{
	GNode *asn = NULL;
	guint count, i;
	const gchar *node_name;
	GArray *names;
	GcrGeneralName general;
	GNode *choice;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SubjectAltName", data);
	if (asn == NULL)
		return NULL;

	names = g_array_new (FALSE, TRUE, sizeof (GcrGeneralName));
	count = egg_asn1x_count (asn);

	for (i = 0; i < count; i++) {
		choice = egg_asn1x_get_choice (egg_asn1x_node (asn, i + 1, NULL));
		g_return_val_if_fail (choice, NULL);

		node_name = egg_asn1x_name (choice);
		g_return_val_if_fail (node_name, NULL);

		memset (&general, 0, sizeof (general));

		if (g_str_equal (node_name, "otherName"))
			general_name_parse_other (choice, &general);

		else if (g_str_equal (node_name, "rfc822Name"))
			general_name_parse_rfc822 (choice, &general);

		else if (g_str_equal (node_name, "dNSName"))
			general_name_parse_dns (choice, &general);

		else if (g_str_equal (node_name, "x400Address"))
			general_name_parse_x400 (choice, &general);

		else if (g_str_equal (node_name, "directoryName"))
			general_name_parse_dn (choice, &general);

		else if (g_str_equal (node_name, "ediPartyName"))
			general_name_parse_edi (choice, &general);

		else if (g_str_equal (node_name, "uniformResourceIdentifier"))
			general_name_parse_uri (choice, &general);

		else if (g_str_equal (node_name, "iPAddress"))
			general_name_parse_ip (choice, &general);

		else if (g_str_equal (node_name, "registeredID"))
			general_name_parse_registered (choice, &general);

		general.raw = egg_asn1x_get_element_raw (choice);
		g_array_append_val (names, general);
	}

	egg_asn1x_destroy (asn);
	return names;
}

void
_gcr_general_names_free (GArray *names)
{
	GcrGeneralName *name;
	guint i;

	for (i = 0; names && i < names->len; i++) {
		name = &g_array_index (names, GcrGeneralName, i);
		g_free (name->display);
		g_bytes_unref (name->raw);
	}
	g_array_free (names, TRUE);
}
