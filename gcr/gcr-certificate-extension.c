/*
 * gcr
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
 * Author: Niels De Graef <nielsdegraef@gmail.com>
 */
#include "config.h"

#include "gcr-certificate-extension-private.h"
#include "gcr-certificate-extensions-private.h"
#include "gcr-types.h"

#include "gcr/gcr-oids.h"
#include "egg/egg-oid.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"

/**
 * GcrCertificateExtension:
 *
 * An object that describes a certificate extension.
 *
 * By default, a certificate extension exposes 3 things: an OID,
 * whether it's marked as critical, and the raw value.
 *
 * For known extensions, gcr tries to provide subclasses with the appropriate
 * API.
 *
 * See also [method@Certificate.list_extensions].
 *
 * Since: 4.3.90
 */

typedef struct _GcrCertificateExtensionPrivate {
	GQuark oid;
	GBytes *value;
	gboolean critical;
} GcrCertificateExtensionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GcrCertificateExtension,
                            gcr_certificate_extension,
                            G_TYPE_OBJECT)

enum {
	PROP_OID = 1,
	PROP_CRITICAL,
	PROP_VALUE,
	N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES];

static void
gcr_certificate_extension_get_property (GObject      *object,
                                        unsigned int  prop_id,
                                        GValue       *value,
                                        GParamSpec   *pspec)
{
	GcrCertificateExtension *self = GCR_CERTIFICATE_EXTENSION (object);

	switch (prop_id)
	{
	case PROP_OID:
		g_value_set_string (value,
		                    gcr_certificate_extension_get_oid (self));
		break;
	case PROP_CRITICAL:
		g_value_set_boolean (value,
		                     gcr_certificate_extension_is_critical (self));
		break;
	case PROP_VALUE:
		g_value_set_boxed (value,
		                   gcr_certificate_extension_get_value (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_extension_set_property (GObject      *object,
                                        unsigned int  prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	GcrCertificateExtension *self = GCR_CERTIFICATE_EXTENSION (object);
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	switch (prop_id)
	{
	case PROP_CRITICAL:
		priv->critical = g_value_get_boolean (value);
		break;
	case PROP_VALUE:
		/* Note that we take ownership here */
		priv->value = g_value_get_boxed (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_extension_init (GcrCertificateExtension *self)
{
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	priv->oid = 0;
	priv->value = NULL;
	priv->critical = FALSE;
}

static void
gcr_certificate_extension_finalize (GObject *obj)
{
	GcrCertificateExtension *self = GCR_CERTIFICATE_EXTENSION (obj);
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	g_bytes_unref (priv->value);

	G_OBJECT_CLASS (gcr_certificate_extension_parent_class)->finalize (obj);
}

static void
gcr_certificate_extension_class_init (GcrCertificateExtensionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_certificate_extension_get_property;
	gobject_class->set_property = gcr_certificate_extension_set_property;
	gobject_class->finalize = gcr_certificate_extension_finalize;

	/**
	 * GcrCertificateExtension:oid:
	 *
	 * The Object Identifier (OID) that identifies the extension.
	 */
	obj_properties[PROP_OID] =
		g_param_spec_string ("oid", NULL, NULL,
		                     NULL,
		                     G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

	/**
	 * GcrCertificateExtension:critical: (getter is_critical)
	 *
	 * Whether this certificate is critical.
	 */
	obj_properties[PROP_CRITICAL] =
		g_param_spec_boolean ("critical", NULL, NULL,
		                      FALSE,
		                      G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

	/**
	 * GcrCertificateExtension:value:
	 *
	 * The raw value in bytes of the extension.
	 */
	obj_properties[PROP_VALUE] =
		g_param_spec_boxed ("value", NULL, NULL,
		                    G_TYPE_BYTES,
		                    G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

	g_object_class_install_properties (gobject_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

/**
 * gcr_certificate_extension_get_oid:
 *
 * Returns the OID that identifies the extension
 *
 * Since: 4.3.90
 */
const char *
gcr_certificate_extension_get_oid (GcrCertificateExtension *self)
{
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION (self), NULL);

	return g_quark_to_string (priv->oid);
}

void
_gcr_certificate_extension_set_oid (GcrCertificateExtension *self,
                                    GQuark oid)
{
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	g_return_if_fail (GCR_IS_CERTIFICATE_EXTENSION (self));
	g_return_if_fail (oid != 0);

	priv->oid = oid;
}

/*
 * gcr_certificate_extension_get_oid_as_quark:
 *
 * Returns the OID that identifies the extension as a #GQuark
 *
 * Since: 4.3.90
 */
GQuark
_gcr_certificate_extension_get_oid_as_quark (GcrCertificateExtension *self)
{
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION (self), 0);

	return priv->oid;
}

/**
 * gcr_certificate_extension_get_description:
 *
 * Returns a user-displayable description of the OID that identifies the
 * extension, if known.
 *
 * Returns: (transfer none) (nullable): A user-facing description, or `null` if
 *   unknown
 *
 * Since: 4.3.90
 */
const char *
gcr_certificate_extension_get_description (GcrCertificateExtension *self)
{
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION (self), NULL);

	return egg_oid_get_description (priv->oid);
}

/**
 * gcr_certificate_extension_get_value:
 *
 * Returns the raw value in bytes of the extension.
 *
 * Returns: (transfer none): The raw value date
 *
 * Since: 4.3.90
 */
GBytes *
gcr_certificate_extension_get_value (GcrCertificateExtension *self)
{
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION (self), NULL);

	return priv->value;
}

/**
 * gcr_certificate_extension_is_critical: (get-property critical)
 *
 * Returns wether the certificate extension is marked critical.
 *
 * Returns: `true` if the extension is marked critical
 */
gboolean
gcr_certificate_extension_is_critical (GcrCertificateExtension *self)
{
	GcrCertificateExtensionPrivate *priv =
		gcr_certificate_extension_get_instance_private (self);

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION (self), FALSE);

	return priv->critical;
}


/* Parsing code */

G_DEFINE_QUARK (gcr-certificate-extension-parse-error-quark, gcr_certificate_extension_parse_error)

GcrCertificateExtension *
_gcr_certificate_extension_generic_parse (GQuark oid,
                                          gboolean critical,
                                          GBytes *value,
                                          GError **error)
{
	GcrCertificateExtension *ret;

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (ret, oid);
	return ret;
}

GcrCertificateExtension *
_gcr_certificate_extension_parse (GNode *extension_node)
{
	GNode *node;
	GQuark oid;
	gboolean critical;
	GBytes *value;
	GcrCertificateExtensionParseFunc parse_func = NULL;
	GcrCertificateExtension *ext = NULL;
	GError *error = NULL;

	g_return_val_if_fail (extension_node != NULL, NULL);

	/* Parse the 3 common values: OID, critical and value */
	node = egg_asn1x_node (extension_node, "extnID", NULL);
	oid = egg_asn1x_get_oid_as_quark (node);
	g_return_val_if_fail (oid != 0, NULL);

	node = egg_asn1x_node (extension_node, "critical", NULL);
	if (egg_asn1x_get_boolean (node, &critical))
		critical = critical;

	node = egg_asn1x_node (extension_node, "extnValue", NULL);
	value = egg_asn1x_get_string_as_bytes (node);

	/* Check if we have a known subclass, otherwise use the generic one */
	if (oid == GCR_OID_BASIC_CONSTRAINTS) {
		parse_func = _gcr_certificate_extension_basic_constraints_parse;
	} else if (oid == GCR_OID_KEY_USAGE) {
		parse_func = _gcr_certificate_extension_key_usage_parse;
	} else if (oid == GCR_OID_EXTENDED_KEY_USAGE) {
		parse_func = _gcr_certificate_extension_extended_key_usage_parse;
	} else if (oid == GCR_OID_SUBJECT_KEY_IDENTIFIER) {
		parse_func = _gcr_certificate_extension_subject_key_identifier_parse;
	} else if (oid == GCR_OID_AUTHORITY_KEY_IDENTIFIER) {
		parse_func = _gcr_certificate_extension_authority_key_identifier_parse;
	} else if (oid == GCR_OID_SUBJECT_ALT_NAME) {
		parse_func = _gcr_certificate_extension_subject_alt_name_parse;
	} else if (oid == GCR_OID_CERTIFICATE_POLICIES) {
		parse_func = _gcr_certificate_extension_certificate_policies_parse;
	} else if (oid == GCR_OID_AUTHORITY_INFO_ACCESS) {
		parse_func = _gcr_certificate_extension_authority_info_access_parse;
	} else if (oid == GCR_OID_CRL_DISTRIBUTION_POINTS) {
		parse_func = _gcr_certificate_extension_crl_distribution_points_parse;
	} else {
		parse_func = _gcr_certificate_extension_generic_parse;
	}

	ext = parse_func (oid, critical, value, &error);

	/* At this point, we don't do anything with the error, except for logging it */
	if (error != NULL) {
		g_debug ("Couldn't parse certificate extension: %s", error->message);
		g_clear_error (&error);
	}

	return ext;
}
