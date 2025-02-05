/*
 * gcr
 *
 * Copyright (C) 2025 Niels De Graef
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
 * Author: Niels De Graef <nielsdegraef@gmail.com>
 */
#include "config.h"

#include <gio/gio.h>

#include "gcr-certificate-extension-private.h"
#include "gcr-certificate-extensions-private.h"

#include "gcr/gcr-oids.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"
#include "egg/egg-oid.h"

#include <glib/gi18n-lib.h>

/**
 * GcrGeneralName:
 *
 * An object describing a name as part of the Subject Alternative Name (SAN)
 * extension.
 *
 * Since: 4.3.90
 */

struct _GcrGeneralName {
	GObject parent_instace;

	GcrGeneralNameType type;
	const char *description;
	char *display;
	GBytes *raw;
};

G_DEFINE_TYPE (GcrGeneralName, gcr_general_name, G_TYPE_OBJECT)

static void
gcr_general_name_finalize (GObject *obj)
{
	GcrGeneralName *self = GCR_GENERAL_NAME (obj);

	G_OBJECT_CLASS (gcr_general_name_parent_class)->finalize (obj);

	g_free (self->display);
	g_bytes_unref (self->raw);
}

static void
gcr_general_name_class_init (GcrGeneralNameClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_general_name_finalize;
}

static void
gcr_general_name_init (GcrGeneralName *self)
{
}

/**
 * gcr_general_name_get_description:
 *
 * Returns a user-friendly string describing the name.
 */
const char *
gcr_general_name_get_description (GcrGeneralName  *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), NULL);

	return self->description;
}

/**
 * gcr_general_name_get_value:
 *
 * Returns the actual value of the name.
 */
const char *
gcr_general_name_get_value (GcrGeneralName *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), NULL);

	return self->display;
}

/**
 * gcr_general_name_get_value_raw:
 *
 * Returns the raw bytes describing the value of the name.
 */
GBytes *
gcr_general_name_get_value_raw (GcrGeneralName *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), NULL);
	return self->raw;
}

GcrGeneralNameType
_gcr_general_name_get_name_type (GcrGeneralName *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), 0);
	return self->type;
}

static GcrGeneralName *
general_name_parse_other (GNode *node, GError **error)
{
	GcrGeneralName *general;
	GNode *decode = NULL;
	GQuark oid;
	GNode *any;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_OTHER;
	general->description = _("Other Name");
	general->display = NULL;

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (node, "type-id", NULL));
	any = egg_asn1x_node (node, "value", NULL);

	if (any == NULL) {
		g_clear_object (&general);
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Missing \"value\" in Other Name");
		return NULL;
	}

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

	return general;
}

static GcrGeneralName *
general_name_parse_rfc822 (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_RFC822;
	general->description = _("Email");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
	return general;
}

static GcrGeneralName *
general_name_parse_dns (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_DNS;
	general->description = _("DNS");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
	return general;
}

static GcrGeneralName *
general_name_parse_x400 (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_X400;
	general->description = _("X400 Address");
	return general;
}

static GcrGeneralName *
general_name_parse_dn (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_DNS;
	general->description = _("Directory Name");
	general->display = egg_dn_read (node);
	return general;
}

static GcrGeneralName *
general_name_parse_edi (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_EDI;
	general->description = _("EDI Party Name");
	return general;
}

static GcrGeneralName *
general_name_parse_uri (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_URI;
	general->description = _("URI");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
	return general;
}

static GcrGeneralName *
general_name_parse_ip (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_IP;
	general->description = _("IP Address");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
	return general;
}

static GcrGeneralName *
general_name_parse_registered (GNode *node, GError **error)
{
	GcrGeneralName *general;

	general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);
	general->type = GCR_GENERAL_NAME_REGISTERED_ID;
	general->description = _("Registered ID");
	general->display = egg_asn1x_get_oid_as_string (node);
	return general;
}

struct {
	const char *name_type;
	GcrGeneralName * (*parse_func) (GNode *node, GError **error);
} NAME_PARSE_FUNCS[] = {
	{ "otherName", general_name_parse_other },
	{ "rfc822Name", general_name_parse_rfc822 },
	{ "dNSName", general_name_parse_dns },
	{ "x400Address", general_name_parse_x400 },
	{ "directoryName", general_name_parse_dn },
	{ "ediPartyName", general_name_parse_edi },
	{ "uniformResourceIdentifier", general_name_parse_uri },
	{ "iPAddress", general_name_parse_ip },
	{ "registeredID", general_name_parse_registered },
};

GcrGeneralName *
_gcr_general_name_parse (GNode *node,
                         GError **error)
{
	GNode *choice;
	const char *node_name;
	GcrGeneralName *name = NULL;

	choice = egg_asn1x_get_choice (node);
	g_return_val_if_fail (choice, NULL);

	node_name = egg_asn1x_name (choice);
	g_return_val_if_fail (node_name, NULL);

	for (size_t j = 0; j < G_N_ELEMENTS (NAME_PARSE_FUNCS); j++) {
		if (g_str_equal (node_name, NAME_PARSE_FUNCS[j].name_type)) {
			name = NAME_PARSE_FUNCS[j].parse_func (choice, error);
			break;
		}
	}

	if (name == NULL) {
		g_set_error (error,
			     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
			     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
			     "Unknown type of GeneralName '%s'",
			     node_name);
		return NULL;
	}

	name->raw = egg_asn1x_get_element_raw (choice);
	return name;
}
