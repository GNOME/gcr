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
 * GcrDistributionPoint:
 *
 * An object describing a CRL distribution point.
 *
 * A certificate user can use such a Certifiate Revocation List (CLR)
 * distribution point to check if the certificate has been revoked.
 *
 * These distribution poitns are for example listed as part of a
 * [class@Gcr.CertificateExtensionCrlDistributionPoints] object.
 *
 * Since: 4.3.91
 */

struct _GcrDistributionPoint {
	GObject parent_instance;

	GcrGeneralNames *full_name;
	GHashTable *relative_name; /* OID (GQuark) to display value (char*) */
};

G_DEFINE_TYPE (GcrDistributionPoint, gcr_distribution_point, G_TYPE_OBJECT)

static void
gcr_distribution_point_finalize (GObject *obj)
{
	GcrDistributionPoint *self = GCR_DISTRIBUTION_POINT (obj);

	g_clear_object (&self->full_name);
	g_clear_pointer (&self->relative_name, g_hash_table_unref);

	G_OBJECT_CLASS (gcr_distribution_point_parent_class)->finalize (obj);
}

static void
gcr_distribution_point_class_init (GcrDistributionPointClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_distribution_point_finalize;
}

static void
gcr_distribution_point_init (GcrDistributionPoint *self)
{
	self->relative_name = g_hash_table_new_full (g_direct_hash,
	                                             g_direct_equal,
	                                             NULL,
	                                             g_free);
}

/**
 * gcr_distribution_point_get_full_name:
 *
 * Returns the full name of the CRL distribution point, if set.
 *
 * Returns: (transfer none) (nullable): The full name of the distribution point
 */
GcrGeneralNames *
gcr_distribution_point_get_full_name (GcrDistributionPoint *self)
{
	g_return_val_if_fail (GCR_IS_DISTRIBUTION_POINT (self), NULL);

	return self->full_name;
}

/**
 * gcr_distribution_point_get_relative_name_part:
 * @part: a RDN type string or OID.
 *
 * Returns a part of the relative name of @self, if set.
 *
 * Note that the relative name might not be set, nor the specific part.
 *
 * Examples of a @part might be the 'OU' (organizational unit) or the 'CN'
 * (common name). Only the value of that part of the RDN is returned.
 *
 * Returns: (transfer full) (nullable): The relative name part if set, or NULL
 */
char *
gcr_distribution_point_get_relative_name_part (GcrDistributionPoint *self,
                                               const char           *part)
{
	GHashTableIter iter;
	void *key, *value;

	g_return_val_if_fail (GCR_IS_DISTRIBUTION_POINT (self), NULL);
	g_return_val_if_fail (part && *part, NULL);

	g_hash_table_iter_init (&iter, self->relative_name);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GQuark oid = GPOINTER_TO_INT (oid);
		const char *display = (const char *) value;

		if ((g_ascii_strcasecmp (g_quark_to_string (oid), part) == 0) ||
		    (g_ascii_strcasecmp (egg_oid_get_name (oid), part) == 0)) {
			return g_strdup (display);
		}
	}

	return NULL;
}

static void
on_parsed_dn_part (unsigned int index,
                   GQuark       oid,
                   GNode       *value,
                   void        *user_data)
{
	GcrDistributionPoint *self = GCR_DISTRIBUTION_POINT (user_data);
	char *label;

	label = egg_dn_print_value (oid, value);
	if (label == NULL)
		return;

	g_hash_table_insert (self->relative_name, GINT_TO_POINTER (oid), label);
}

static GcrDistributionPoint *
_gcr_distribution_point_parse (GNode   *node,
                               GError **error)
{
	GNode *choice;
	const char *node_name;
	GcrDistributionPoint *distr = NULL;

	choice = egg_asn1x_get_choice (node);
	if (choice == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Invalid distributionPoint field, not a choice");
		goto out;
	}

	node_name = egg_asn1x_name (choice);
	g_return_val_if_fail (node_name, NULL);

	distr = g_object_new (GCR_TYPE_DISTRIBUTION_POINT, NULL);

	if (g_strcmp0 (node_name, "fullName") == 0) {
		distr->full_name = _gcr_general_names_parse (choice, error);
		if (distr->full_name == NULL)
			goto out;
	} else if (g_strcmp0 (node_name, "nameRelativeToCRLIssuer") == 0) {
		if (!egg_dn_parse (choice, on_parsed_dn_part, distr)) {
			g_set_error_literal (error,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
				             "Invalid relative names for CRL distribution point");
			goto out;
		}
	} else {
		g_set_error (error,
		             GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		             GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
			     "Invalid distributionPoint choice '%s'",
			     node_name);
		goto out;
	}

out:
	if (error && *error)
		g_clear_object (&distr);
	return distr;
}


/**
 * GcrCertificateExtensionCrlDistributionPoints:
 *
 * A certificate extension that lists CRL distribution points.
 *
 * Each distribution point is exposed as a [class@Gcr.DistributionPoint]
 * object.
 *
 * Since: 4.3.91
 */

struct _GcrCertificateExtensionCrlDistributionPoints
{
	GcrCertificateExtension parent_instance;

	GPtrArray *distrpoints;
};

static void g_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateExtensionCrlDistributionPoints,
                         gcr_certificate_extension_crl_distribution_points,
                         GCR_TYPE_CERTIFICATE_EXTENSION,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                g_list_model_interface_init))

enum {
	PROP_N_ITEMS = 1,
	N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES];

static GType
gcr_certificate_extension_crl_distribution_points_get_item_type (GListModel *model)
{
	return GCR_TYPE_DISTRIBUTION_POINT;
}

static unsigned int
gcr_certificate_extension_crl_distribution_points_get_n_items (GListModel *model)
{
	GcrCertificateExtensionCrlDistributionPoints *self =
		GCR_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS (model);
	return self->distrpoints->len;
}

static void *
gcr_certificate_extension_crl_distribution_points_get_item (GListModel   *model,
                                                            unsigned int  position)
{
	GcrCertificateExtensionCrlDistributionPoints *self =
		GCR_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS (model);

	if (position >= self->distrpoints->len)
		return NULL;

	return g_object_ref (g_ptr_array_index (self->distrpoints, position));
}

static void
gcr_certificate_extension_crl_distribution_points_get_property (GObject      *object,
                                                                unsigned int  prop_id,
                                                                GValue       *value,
                                                                GParamSpec   *pspec)
{
	GcrCertificateExtensionCrlDistributionPoints *self =
		GCR_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS (object);

	switch (prop_id)
	{
	case PROP_N_ITEMS:
		g_value_set_uint (value, self->distrpoints->len);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
g_list_model_interface_init (GListModelInterface *iface)
{
	iface->get_item_type = gcr_certificate_extension_crl_distribution_points_get_item_type;
	iface->get_n_items = gcr_certificate_extension_crl_distribution_points_get_n_items;
	iface->get_item = gcr_certificate_extension_crl_distribution_points_get_item;
}

static void
gcr_certificate_extension_crl_distribution_points_finalize (GObject *obj)
{
	GcrCertificateExtensionCrlDistributionPoints *self = GCR_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS (obj);

	g_ptr_array_unref (self->distrpoints);

	G_OBJECT_CLASS (gcr_certificate_extension_crl_distribution_points_parent_class)->finalize (obj);
}

static void
gcr_certificate_extension_crl_distribution_points_class_init (GcrCertificateExtensionCrlDistributionPointsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_certificate_extension_crl_distribution_points_get_property;
	gobject_class->finalize = gcr_certificate_extension_crl_distribution_points_finalize;

	/**
	 * GcrCertificateExtensionCrlDistributionPoints:n-items:
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
gcr_certificate_extension_crl_distribution_points_init (GcrCertificateExtensionCrlDistributionPoints *self)
{
	self->distrpoints = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * gcr_certificate_extension_crl_distribution_points_get_distribution_point:
 *
 * Returns the CRL distribution point at a given position.
 *
 * It is illegal to call this on an invalid position.
 *
 * Returns: (transfer none): The distribution point at position @position
 */
GcrDistributionPoint *
gcr_certificate_extension_crl_distribution_points_get_distribution_point (GcrCertificateExtensionCrlDistributionPoints *self,
                                                                          unsigned int                                position)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS (self), NULL);
	g_return_val_if_fail (position < self->distrpoints->len, NULL);

	return g_ptr_array_index (self->distrpoints, position);
}

GcrCertificateExtension *
_gcr_certificate_extension_crl_distribution_points_parse (GQuark oid,
                                                          gboolean critical,
                                                          GBytes *value,
                                                          GError **error)
{
	GcrCertificateExtensionCrlDistributionPoints *ret = NULL;
	GNode *asn = NULL;
	GPtrArray *distrpoints;
	unsigned int n_points;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "CRLDistributionPoints", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode CRLDistributionPoints");
		goto out;
	}

	n_points = egg_asn1x_count (asn);
	distrpoints = g_ptr_array_new_full (n_points, g_object_unref);
	for (unsigned int i = 0; i < n_points; i++) {
		GNode *node;
		GcrDistributionPoint *distr;

		node = egg_asn1x_node (asn, i + 1, "distributionPoint", NULL);
		if (node == NULL)
			break;

		distr = _gcr_distribution_point_parse (node, error);
		if (distr == NULL) {
			goto out;
		}

		g_ptr_array_add (distrpoints, distr);
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_CRL_DISTRIBUTION_POINTS,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	g_ptr_array_extend_and_steal (ret->distrpoints, g_steal_pointer (&distrpoints));

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
