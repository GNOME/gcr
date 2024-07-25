/*
 * libgcr
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
#include "gcr-types.h"

#include "gcr/gcr-oids.h"
#include "egg/egg-oid.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"

struct _GcrCertificateExtension {
	GQuark oid;
	GBytes *value;
	gboolean critical;
};

static GcrCertificateExtension *
gcr_certificate_extension_copy (GcrCertificateExtension *self)
{
	GcrCertificateExtension *copy;

	copy = g_new0 (GcrCertificateExtension, 1);
	copy->oid = copy->oid;
	copy->value = g_bytes_ref (self->value);
	copy->critical = copy->critical;

	return copy;
}

void
gcr_certificate_extension_free (GcrCertificateExtension *self)
{
	if (self == NULL)
		return;

	g_bytes_unref (self->value);
	g_free (self);
}

G_DEFINE_BOXED_TYPE (GcrCertificateExtension, gcr_certificate_extension,
                     gcr_certificate_extension_copy,
                     gcr_certificate_extension_free)

/**
 * gcr_certificate_extension_get_oid:
 *
 * Returns the OID that identifies the extension
 */
const char *
gcr_certificate_extension_get_oid (GcrCertificateExtension *self)
{
	g_return_val_if_fail (self, NULL);

	return g_quark_to_string (self->oid);
}

/**
 * gcr_certificate_extension_get_oid_description:
 *
 * Returns a displayable description of the OID that identifies the extension
 */
const char *
gcr_certificate_extension_get_description (GcrCertificateExtension *self)
{
	g_return_val_if_fail (self, NULL);

	return egg_oid_get_description (self->oid);
}

/**
 * gcr_certificate_extension_get_value:
 *
 * Returns the raw value in bytes of the extension.
 *
 * Returns: (transfer none): The raw data of the public key
 */
GBytes *
gcr_certificate_extension_get_value (GcrCertificateExtension *self)
{
	g_return_val_if_fail (self, NULL);

	return self->value;
}

/**
 * gcr_certificate_extension_is_critical:
 *
 * Returns wether the certificate extension is marked critical.
 *
 * Returns: `true` if the extension is marked critical
 */
gboolean
gcr_certificate_extension_is_critical (GcrCertificateExtension *self)
{
	g_return_val_if_fail (self, FALSE);

	return self->critical;
}

GcrCertificateExtension *
_gcr_certificate_extension_new (GNode *extension_node)
{
	GcrCertificateExtension *self;
	GNode *node;
	gboolean critical;

	g_return_val_if_fail (extension_node, NULL);

	self = g_new0 (GcrCertificateExtension, 1);

	node = egg_asn1x_node (extension_node, "extnID", NULL);
	self->oid = egg_asn1x_get_oid_as_quark (node);
	g_return_val_if_fail (self->oid != 0, NULL);

	node = egg_asn1x_node (extension_node, "extnValue", NULL);
	self->value = egg_asn1x_get_string_as_bytes (node);

	node = egg_asn1x_node (extension_node, "critical", NULL);
	if (egg_asn1x_get_boolean (node, &critical))
		self->critical = critical;

	return self;
}
