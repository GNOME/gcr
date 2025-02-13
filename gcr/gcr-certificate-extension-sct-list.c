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
 * GcrCertificateExtensionSctList:
 *
 * A certificate extension describing a list of Signed Certificate Timestamps (SCTs).
 *
 * Since: 4.3.92
 */

struct _GcrCertificateExtensionSctList
{
	GcrCertificateExtension parent_instance;

	GPtrArray *policies;
};

static void cert_policies_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateExtensionSctList,
                         gcr_certificate_extension_sct_list,
                         GCR_TYPE_CERTIFICATE_EXTENSION,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                cert_policies_list_model_interface_init))

enum {
	PROP_N_ITEMS = 1,
	N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES];

static GType
gcr_certificate_extension_sct_list_get_item_type (GListModel *model)
{
	return GCR_TYPE_CERTIFICATE_POLICY;
}

static unsigned int
gcr_certificate_extension_sct_list_get_n_items (GListModel *model)
{
	GcrCertificateExtensionSctList *self =
		GCR_CERTIFICATE_EXTENSION_SCT_LIST (model);
	return self->policies->len;
}

static void *
gcr_certificate_extension_sct_list_get_item (GListModel   *model,
                                                         unsigned int  position)
{
	GcrCertificateExtensionSctList *self =
		GCR_CERTIFICATE_EXTENSION_SCT_LIST (model);

	if (position >= self->policies->len)
		return NULL;

	return g_object_ref (g_ptr_array_index (self->policies, position));
}

static void
gcr_certificate_extension_sct_list_get_property (GObject      *object,
                                                             unsigned int  prop_id,
                                                             GValue       *value,
                                                             GParamSpec   *pspec)
{
	GcrCertificateExtensionSctList *self =
		GCR_CERTIFICATE_EXTENSION_SCT_LIST (object);

	switch (prop_id)
	{
	case PROP_N_ITEMS:
		g_value_set_uint (value, self->policies->len);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
cert_policies_list_model_interface_init (GListModelInterface *iface)
{
	iface->get_item_type = gcr_certificate_extension_sct_list_get_item_type;
	iface->get_n_items = gcr_certificate_extension_sct_list_get_n_items;
	iface->get_item = gcr_certificate_extension_sct_list_get_item;
}

static void
gcr_certificate_extension_sct_list_finalize (GObject *obj)
{
	GcrCertificateExtensionSctList *self = GCR_CERTIFICATE_EXTENSION_SCT_LIST (obj);

	g_ptr_array_unref (self->policies);

	G_OBJECT_CLASS (gcr_certificate_extension_sct_list_parent_class)->finalize (obj);
}

static void
gcr_certificate_extension_sct_list_class_init (GcrCertificateExtensionSctListClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_certificate_extension_sct_list_get_property;
	gobject_class->finalize = gcr_certificate_extension_sct_list_finalize;

	/**
	 * GcrCertificateExtensionSctList:n-items:
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
gcr_certificate_extension_sct_list_init (GcrCertificateExtensionSctList *self)
{
	self->policies = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * gcr_certificate_extension_sct_list_get_policy:
 *
 * Returns the policy at a given position
 *
 * Returns: (transfer none): The policy at position @position
 */
GcrSignedCertificateTimestamp *
gcr_certificate_extension_sct_list_get_sct (GcrCertificateExtensionSctList *self,
                                            unsigned int                    position)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_SCT_LIST (self), NULL);
	g_return_val_if_fail (position < self->policies->len, NULL);

	return g_ptr_array_index (self->policies, position);
}

GcrCertificateExtension *
_gcr_certificate_extension_sct_list_parse (GQuark oid,
                                           gboolean critical,
                                           GBytes *value,
                                           GError **error)
{
	GcrCertificateExtensionSctList *ret = NULL;
	GNode *asn = NULL;
	GPtrArray *policies;
	unsigned int n_policies;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SctList", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode SctList");
		goto out;
	}

	n_policies = egg_asn1x_count (asn);
	policies = g_ptr_array_new_full (n_policies, g_object_unref);
	for (unsigned int i = 0; i < n_policies; i++) {
		GNode *node;
		GQuark oid;
		GcrCertificatePolicy *policy;

		node = egg_asn1x_node (asn, i + 1, "policyIdentifier", NULL);
		if (node == NULL)
			break;

		oid = egg_asn1x_get_oid_as_quark (node);
		if (oid == 0) {
			g_set_error_literal (error,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
			                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
			                     "Invalid policyIdentifier for cert policy");
			goto out;
		}

		policy = g_object_new (GCR_TYPE_CERTIFICATE_POLICY, NULL);
		policy->oid = oid;
		g_ptr_array_add (policies, policy);

		node = egg_asn1x_node (asn, i + 1, "policyQualifiers", NULL);
		if (node != NULL) {
			unsigned int n_qualifiers;
			GPtrArray *qualifiers;

			n_qualifiers = egg_asn1x_count (node);
			qualifiers = g_ptr_array_new_full (n_qualifiers, g_object_unref);
			for (unsigned int j = 0; j < n_qualifiers; j++) {
				GNode *q_node;
				GQuark q_oid;
				GcrCertificatePolicyQualifier *qualifier;

				q_node = egg_asn1x_node (node, j + 1, "policyQualifierId", NULL);
				if (q_node == NULL)
					break;
				q_oid = egg_asn1x_get_oid_as_quark (q_node);

				qualifier = g_object_new (GCR_TYPE_CERTIFICATE_POLICY_QUALIFIER, NULL);
				qualifier->oid = q_oid;
				g_ptr_array_add (qualifiers, qualifier);
			}
			g_ptr_array_extend_and_steal (policy->qualifiers,
			                              g_steal_pointer (&qualifiers));
		}
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_SCT_LIST,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	g_ptr_array_extend_and_steal (ret->policies, g_steal_pointer (&policies));

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
