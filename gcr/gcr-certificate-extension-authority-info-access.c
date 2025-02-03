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
 * GcrAccessDescription:
 *
 * Describes a location for fetching extra information from the Certificate Authority.
 *
 * This object is usually part of a
 * [class@Gcr.CertificateExtensionAuthorityInfoAccess] object.
 *
 * Since: 4.3.91
 */

struct _GcrAccessDescription {
	GObject parent_instace;

	GQuark method_oid;
	GcrGeneralName *location;
};

G_DEFINE_TYPE (GcrAccessDescription, gcr_access_description, G_TYPE_OBJECT)

static void
gcr_access_description_finalize (GObject *obj)
{
	GcrAccessDescription *self = GCR_ACCESS_DESCRIPTION (obj);

	g_clear_object (&self->location);

	G_OBJECT_CLASS (gcr_access_description_parent_class)->finalize (obj);
}

static void
gcr_access_description_class_init (GcrAccessDescriptionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_access_description_finalize;
}

static void
gcr_access_description_init (GcrAccessDescription *self)
{
}

/**
 * gcr_access_description_get_method_oid:
 *
 * Returns the OID string that describes the method for accessing the resource.
 *
 * Returns: The method OID
 */
const char *
gcr_access_description_get_method_oid (GcrAccessDescription *self)
{
	g_return_val_if_fail (GCR_IS_ACCESS_DESCRIPTION (self), NULL);
	return g_quark_to_string (self->method_oid);
}

/**
 * gcr_access_description_get_method_name:
 *
 * Returns a user-friendly name of the method for accesssing the resource, if
 * known.
 *
 * Returns: A method name
 */
const char *
gcr_access_description_get_method_name (GcrAccessDescription *self)
{
	g_return_val_if_fail (GCR_IS_ACCESS_DESCRIPTION (self), NULL);
	return egg_oid_get_description (self->method_oid);
}

/**
 * gcr_access_description_get_location:
 *
 * Returns the location, described by a [class@Gcr.GeneralName].
 *
 * Returns: (transfer none): the location
 */
GcrGeneralName *
gcr_access_description_get_location (GcrAccessDescription *self)
{
	g_return_val_if_fail (GCR_IS_ACCESS_DESCRIPTION (self), NULL);
	return self->location;
}


/**
 * GcrCertificateExtensionAuthorityInfoAccess:
 *
 * A certificate extension describing the Authority Information Access (AIA).
 *
 * This extensions specifies a list of resources of a certificate's issuer that
 * one may use to retrieve extra information, such as missing intermediate
 * certificates in a certificate chain, or to determine certifiate revocation
 * status.
 *
 * Each access point is exposed as a [class@Gcr.AccessDescription] object.
 *
 * Since: 4.3.91
 */

struct _GcrCertificateExtensionAuthorityInfoAccess
{
	GcrCertificateExtension parent_instance;

	GPtrArray *descriptions;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateExtensionAuthorityInfoAccess,
                         gcr_certificate_extension_authority_info_access,
                         GCR_TYPE_CERTIFICATE_EXTENSION,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                g_list_model_interface_init))

enum {
	PROP_N_ITEMS = 1,
	N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES];

static GType
gcr_certificate_extension_authority_info_access_get_item_type (GListModel *model)
{
	return GCR_TYPE_ACCESS_DESCRIPTION;
}

static unsigned int
gcr_certificate_extension_authority_info_access_get_n_items (GListModel *model)
{
	GcrCertificateExtensionAuthorityInfoAccess *self =
		GCR_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS (model);
	return self->descriptions->len;
}

static void *
gcr_certificate_extension_authority_info_access_get_item (GListModel   *model,
                                                         unsigned int  position)
{
	GcrCertificateExtensionAuthorityInfoAccess *self =
		GCR_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS (model);

	if (position >= self->descriptions->len)
		return NULL;

	return g_object_ref (g_ptr_array_index (self->descriptions, position));
}

static void
gcr_certificate_extension_authority_info_access_get_property (GObject      *object,
                                                             unsigned int  prop_id,
                                                             GValue       *value,
                                                             GParamSpec   *pspec)
{
	GcrCertificateExtensionAuthorityInfoAccess *self =
		GCR_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS (object);

	switch (prop_id)
	{
	case PROP_N_ITEMS:
		g_value_set_uint (value, self->descriptions->len);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
	iface->get_item_type = gcr_certificate_extension_authority_info_access_get_item_type;
	iface->get_n_items = gcr_certificate_extension_authority_info_access_get_n_items;
	iface->get_item = gcr_certificate_extension_authority_info_access_get_item;
}

static void
gcr_certificate_extension_authority_info_access_finalize (GObject *obj)
{
	GcrCertificateExtensionAuthorityInfoAccess *self = GCR_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS (obj);

	g_ptr_array_unref (self->descriptions);

	G_OBJECT_CLASS (gcr_certificate_extension_authority_info_access_parent_class)->finalize (obj);
}

static void
gcr_certificate_extension_authority_info_access_class_init (GcrCertificateExtensionAuthorityInfoAccessClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_certificate_extension_authority_info_access_get_property;
	gobject_class->finalize = gcr_certificate_extension_authority_info_access_finalize;

	/**
	 * GcrCertificateExtensionAuthorityInfoAccess:n-items:
	 *
	 * The number of items. See [method@Gio.ListModel.get_n_items].
	 */
	obj_properties[PROP_N_ITEMS] =
		g_param_spec_uint ("n-items", NULL, NULL,
		                   0, G_MAXUINT, 0,
		                   G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

	g_object_class_install_properties (gobject_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

static void
gcr_certificate_extension_authority_info_access_init (GcrCertificateExtensionAuthorityInfoAccess *self)
{
	self->descriptions = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * gcr_certificate_extension_authority_info_access_get_description:
 *
 * Returns the description at a given position
 *
 * Returns: (transfer none): The description at position @position
 */
GcrAccessDescription *
gcr_certificate_extension_authority_info_access_get_description (GcrCertificateExtensionAuthorityInfoAccess *self,
                                                                 unsigned int                                position)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS (self), NULL);
	g_return_val_if_fail (position < self->descriptions->len, NULL);

	return g_ptr_array_index (self->descriptions, position);
}

GcrCertificateExtension *
_gcr_certificate_extension_authority_info_access_parse (GQuark oid,
                                                        gboolean critical,
                                                        GBytes *value,
                                                        GError **error)
{
	GcrCertificateExtensionAuthorityInfoAccess *ret = NULL;
	GNode *asn = NULL;
	GPtrArray *descriptions;
	unsigned int n_descriptions;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "AuthorityInfoAccessSyntax", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode AuthorityInfoAccessSyntax");
		goto out;
	}

	n_descriptions = egg_asn1x_count (asn);
	descriptions = g_ptr_array_new_full (n_descriptions, g_object_unref);
	for (unsigned int i = 0; i < n_descriptions; i++) {
		GNode *node;
		GQuark oid;
		GcrAccessDescription *description;

		node = egg_asn1x_node (asn, i + 1, "accessMethod", NULL);
		if (node == NULL)
			break;

		description = g_object_new (GCR_TYPE_ACCESS_DESCRIPTION, NULL);
		g_ptr_array_add (descriptions, description);

		oid = egg_asn1x_get_oid_as_quark (node);
		if (oid == 0) {
			g_set_error_literal (error,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
			                     "Invalid accessMethod for access description in AIA");
			goto out;
		}
		description->method_oid = oid;

		node = egg_asn1x_node (asn, i + 1, "accessLocation", NULL);
		if (node == NULL) {
			g_set_error_literal (error,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
			                     "Missing accessLocation for access description in AIA");
			goto out;
		}

		description->location = _gcr_general_name_parse (node, error);
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_AUTHORITY_INFO_ACCESS,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	g_ptr_array_extend_and_steal (ret->descriptions,
	                              g_steal_pointer (&descriptions));

out:
	if (descriptions != NULL)
		g_ptr_array_unref (descriptions);
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
