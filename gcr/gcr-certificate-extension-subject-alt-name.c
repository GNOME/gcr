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
 * GcrCertificateExtensionSubjectAltName:
 *
 * A certificate extension describing the Subject Alternative Name (SAN).
 *
 * This kind of extension is used for example to specify multiple domains for
 * the same certificate.
 *
 * The object exposes the different names with the [iface@Gio.ListModel] API.
 *
 * Since: 4.3.90
 */

struct _GcrCertificateExtensionSubjectAltName
{
	GcrCertificateExtension parent_instance;

	GPtrArray *names;
};

static void san_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateExtensionSubjectAltName,
                         gcr_certificate_extension_subject_alt_name,
                         GCR_TYPE_CERTIFICATE_EXTENSION,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                san_list_model_interface_init))

static GType
gcr_certificate_extension_subject_alt_name_get_item_type (GListModel *model)
{
	return GCR_TYPE_GENERAL_NAME;
}

static unsigned int
gcr_certificate_extension_subject_alt_name_get_n_items (GListModel *model)
{
	GcrCertificateExtensionSubjectAltName *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (model);

	return self->names->len;
}

static void *
gcr_certificate_extension_subject_alt_name_get_item (GListModel   *model,
                                                     unsigned int  position)
{
	GcrCertificateExtensionSubjectAltName *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (model);

	if (position >= self->names->len)
		return NULL;

	return g_object_ref (g_ptr_array_index (self->names, position));
}

static void
san_list_model_interface_init (GListModelInterface *iface)
{
	iface->get_item_type = gcr_certificate_extension_subject_alt_name_get_item_type;
	iface->get_n_items = gcr_certificate_extension_subject_alt_name_get_n_items;
	iface->get_item = gcr_certificate_extension_subject_alt_name_get_item;
}

static void
gcr_certificate_extension_subject_alt_name_finalize (GObject *obj)
{
	GcrCertificateExtensionSubjectAltName *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (obj);

	G_OBJECT_CLASS (gcr_certificate_extension_subject_alt_name_parent_class)->finalize (obj);

	g_ptr_array_unref (self->names);
}

static void
gcr_certificate_extension_subject_alt_name_class_init (GcrCertificateExtensionSubjectAltNameClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_certificate_extension_subject_alt_name_finalize;
}

static void
gcr_certificate_extension_subject_alt_name_init (GcrCertificateExtensionSubjectAltName *self)
{
	self->names = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * gcr_certificate_extension_subject_alt_name_get_name:
 * @self: A #GcrCertificateExtensionSubjectAltName
 * @position: The position of the name
 *
 * Returns the name at the given position.
 *
 * Returns: (transfer none): The name at position @position
 */
GcrGeneralName *
gcr_certificate_extension_subject_alt_name_get_name (GcrCertificateExtensionSubjectAltName *self,
                                                     unsigned int position)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (self), NULL);
	g_return_val_if_fail (position < self->names->len, NULL);

	return g_ptr_array_index (self->names, position);
}


GcrCertificateExtension *
_gcr_certificate_extension_subject_alt_name_parse (GQuark oid,
                                                   gboolean critical,
                                                   GBytes *value,
                                                   GError **error)
{
	GcrCertificateExtensionSubjectAltName *ret = NULL;
	GNode *asn = NULL;
	GcrGeneralNames *names;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SubjectAltName", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode SubjectAltName");
		goto out;
	}

	names = _gcr_general_names_parse (asn, error);
	if (names == NULL)
		goto out;

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	g_ptr_array_extend_and_steal (ret->names, _gcr_general_names_steal (names));

out:
	g_clear_object (&names);
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
