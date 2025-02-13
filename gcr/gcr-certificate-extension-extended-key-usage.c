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
 * GcrCertificateExtensionExtendedKeyUsage:
 *
 * A certificate extension that can be used to restrict an extended set of
 * usages of a given certificate.
 *
 * Similar to [class@Gcr.CertificateExtensionKeyUsage], this extension defined
 * an additional set of purposes for which this certificate may be used.
 *
 * Since: 4.3.90
 */

struct _GcrCertificateExtensionExtendedKeyUsage
{
	GcrCertificateExtension parent_instance;

	GQuark *oids;
};

G_DEFINE_TYPE (GcrCertificateExtensionExtendedKeyUsage,
               gcr_certificate_extension_extended_key_usage,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static void
gcr_certificate_extension_extended_key_usage_finalize (GObject *obj)
{
	GcrCertificateExtensionExtendedKeyUsage *self = GCR_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE (obj);

	G_OBJECT_CLASS (gcr_certificate_extension_extended_key_usage_parent_class)->finalize (obj);

	g_free (self->oids);
}

static void
gcr_certificate_extension_extended_key_usage_class_init (GcrCertificateExtensionExtendedKeyUsageClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_certificate_extension_extended_key_usage_finalize;
}

static void
gcr_certificate_extension_extended_key_usage_init (GcrCertificateExtensionExtendedKeyUsage *self)
{
	self->oids = NULL;
}

/**
 * gcr_certificate_extension_extended_key_usage_get_oids:
 *
 * Returns the list of OIDs of the extended key usages.
 *
 * Returns: (transfer full): The OIDs.
 */
GStrv
gcr_certificate_extension_extended_key_usage_get_oids (GcrCertificateExtensionExtendedKeyUsage *self)
{
	GPtrArray *values;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE (self), NULL);

	values = g_ptr_array_new_with_free_func (g_free);
	for (size_t i = 0; self->oids[i] != 0; i++) {
		const char *value;
		value = g_quark_to_string (self->oids[i]);
		g_ptr_array_add (values, g_strdup (value));
	}

	g_ptr_array_add (values, NULL); /* Add NULL terminator */
	return (GStrv) g_ptr_array_free (values, FALSE);
}

/**
 * gcr_certificate_extension_extended_key_usage_get_descriptions:
 *
 * Returns a user-friendly list of description of the key usages.
 *
 * Returns: (transfer full): The descriptions.
 */
GStrv
gcr_certificate_extension_extended_key_usage_get_descriptions (GcrCertificateExtensionExtendedKeyUsage *self)
{
	GPtrArray *values;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE (self), NULL);

	values = g_ptr_array_new_with_free_func (g_free);
	for (size_t i = 0; self->oids[i] != 0; i++) {
		const char *value;
		value = egg_oid_get_description (self->oids[i]);
		g_ptr_array_add (values, g_strdup (value));
	}

	g_ptr_array_add (values, NULL); /* Add NULL terminator */
	return (GStrv) g_ptr_array_free (values, FALSE);
}

GcrCertificateExtension *
_gcr_certificate_extension_extended_key_usage_parse (GQuark oid,
                                                     gboolean critical,
                                                     GBytes *value,
                                                     GError **error)
{
	GcrCertificateExtensionExtendedKeyUsage *ret = NULL;
	GNode *asn = NULL;
	GArray *array;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "ExtKeyUsageSyntax", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode ExtKeyUsageSyntax");
		goto out;
	}

	array = g_array_new (TRUE, TRUE, sizeof (GQuark));
	for (int i = 0; TRUE; ++i) {
		GNode *node;
		GQuark oid;

		node = egg_asn1x_node (asn, i + 1, NULL);
		if (node == NULL)
			break;
		oid = egg_asn1x_get_oid_as_quark (node);
		g_array_append_val (array, oid);
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->oids = (GQuark*) g_array_free (array, FALSE);

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
