/*
 * gcr
 *
 * Copyright (C) 2024 Niels De Graef
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
#include "gcr-certificate-extension-list-private.h"
#include "gcr-types.h"

#include "gcr/gcr-oids.h"
#include "egg/egg-oid.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"

/**
 * GcrCertificateExtensionList:
 *
 * A wrapper type for a list of [class@CertificateExtension]s.
 *
 * Since: 4.3.90
 */

struct _GcrCertificateExtensionList {
	GObject parent_instance;

	GPtrArray *extensions;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateExtensionList,
                         gcr_certificate_extension_list,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                g_list_model_interface_init))

enum {
	PROP_N_ITEMS = 1,
	N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES];

static GType
gcr_certificate_extension_list_get_item_type (GListModel *model)
{
	return GCR_TYPE_CERTIFICATE_EXTENSION;
}

static guint
gcr_certificate_extension_list_get_n_items (GListModel *model)
{
	GcrCertificateExtensionList *self = GCR_CERTIFICATE_EXTENSION_LIST (model);

	return self->extensions->len;
}

static gpointer
gcr_certificate_extension_list_get_item (GListModel *model,
                                         guint       position)
{
	GcrCertificateExtensionList *self = GCR_CERTIFICATE_EXTENSION_LIST (model);

	if (position >= self->extensions->len)
		return NULL;

	return g_object_ref (g_ptr_array_index (self->extensions, position));
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
	iface->get_item_type = gcr_certificate_extension_list_get_item_type;
	iface->get_n_items = gcr_certificate_extension_list_get_n_items;
	iface->get_item = gcr_certificate_extension_list_get_item;
}

static void
gcr_certificate_extension_list_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
	GcrCertificateExtensionList *self = GCR_CERTIFICATE_EXTENSION_LIST (object);

	switch (prop_id)
	{
	case PROP_N_ITEMS:
		g_value_set_uint (value,
		                  self->extensions->len);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_extension_list_init (GcrCertificateExtensionList *self)
{
	self->extensions = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
gcr_certificate_extension_list_finalize (GObject *obj)
{
	GcrCertificateExtensionList *self = GCR_CERTIFICATE_EXTENSION_LIST (obj);

	G_OBJECT_CLASS (gcr_certificate_extension_list_parent_class)->finalize (obj);

	g_ptr_array_unref (self->extensions);
}

static void
gcr_certificate_extension_list_class_init (GcrCertificateExtensionListClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_certificate_extension_list_get_property;
	gobject_class->finalize = gcr_certificate_extension_list_finalize;

	obj_properties[PROP_N_ITEMS] =
		g_param_spec_uint ("n-items", NULL, NULL,
		                   0, G_MAXUINT, 0,
		                   G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

	g_object_class_install_properties (gobject_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

/**
 * gcr_certificate_extension_list_get_extension:
 * @self: A #GcrCertificateExtensionList
 * @position: The position of the extension in the list
 *
 * Returns the extension at the given position.
 *
 * It is illegal to call this function with an invalid position.
 *
 * Returns: (transfer none): The certificate extension with the given OID
 */
GcrCertificateExtension *
gcr_certificate_extension_list_get_extension (GcrCertificateExtensionList *self,
                                              unsigned int                position)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_LIST (self), NULL);
	g_return_val_if_fail (position < self->extensions->len, NULL);

	return g_ptr_array_index (self->extensions, position);
}

/**
 * gcr_certificate_extension_list_find_by_oid:
 * @self: A #GcrCertificateExtensionList
 * @oid: The OID of the certificate extension
 *
 * Looks for an extension with the given OID.
 *
 * Returns: (transfer none) (nullable): The certificate extension with
 *  the given OID, or %NULL if not found.
 *
 * Since: 4.3.90
 */
GcrCertificateExtension *
gcr_certificate_extension_list_find_by_oid (GcrCertificateExtensionList *self,
                                            const char                  *oid)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_LIST (self), NULL);
	g_return_val_if_fail (oid && *oid, NULL);

	for (guint i = 0; i < self->extensions->len; i++) {
		GcrCertificateExtension *extension;
		const char *extension_oid;

		extension = g_ptr_array_index (self->extensions, i);
		extension_oid = gcr_certificate_extension_get_oid (extension);

		if (g_strcmp0 (oid, extension_oid) == 0) {
			return extension;
		}
	}

	return NULL;
}

GcrCertificateExtensionList *
_gcr_certificate_extension_list_new_for_asn1 (GNode *asn1)
{
	GcrCertificateExtensionList *self;

	g_return_val_if_fail (asn1 != NULL, NULL);

	self = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_LIST, NULL);

	for (unsigned int extension_num = 1; TRUE; ++extension_num) {
		GNode *extension_node;
		GcrCertificateExtension *extension;

		extension_node = egg_asn1x_node (asn1, "tbsCertificate", "extensions", extension_num, NULL);
		if (extension_node == NULL)
			break;

		extension = _gcr_certificate_extension_parse (extension_node);
		if (extension == NULL) {
			g_critical ("Unrecognized certificate extension");
			continue;
		}

		g_ptr_array_add (self->extensions, extension);
	}

	return self;
}
