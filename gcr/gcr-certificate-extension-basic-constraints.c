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
 * GcrCertificateExtensionBasicConstraints:
 *
 * A certificate extension that can be used to identify the type of the
 * certificate subject (whether it is a certificate authority or not).
 *
 * Since: 4.3.90
 */

struct _GcrCertificateExtensionBasicConstraints
{
	GcrCertificateExtension parent_instance;

	gboolean is_ca;
	int path_len_constraint;
};

G_DEFINE_TYPE (GcrCertificateExtensionBasicConstraints,
               gcr_certificate_extension_basic_constraints,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static void
gcr_certificate_extension_basic_constraints_class_init (GcrCertificateExtensionBasicConstraintsClass *klass)
{
}

static void
gcr_certificate_extension_basic_constraints_init (GcrCertificateExtensionBasicConstraints *self)
{
}

/**
 * gcr_certificate_extension_basic_constraints_is_ca:
 *
 * Returns whether the certificate us a certificate authority (CA) certificate
 * or an end entity certificate.
 *
 * Returns: The value of "cA".
 */
gboolean
gcr_certificate_extension_basic_constraints_is_ca (GcrCertificateExtensionBasicConstraints *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS (self), FALSE);

	return self->is_ca;
}

/**
 * gcr_certificate_extension_basic_constraints_get_path_len_constraint:
 *
 * Returns the maximum number of CAs that are allowed in the chain below this
 * certificate.
 *
 * If this is not set, this method returns -1.
 *
 * Note that this field doesn't really make sense if
 * [method@Gcr.CertificateExtensionBasicConstraints.is_ca] is false.
 *
 * Returns: The value of "pathLenConstraint", or -1 if not set.
 */
int
gcr_certificate_extension_basic_constraints_get_path_len_constraint (GcrCertificateExtensionBasicConstraints *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS (self), -1);

	return self->path_len_constraint;
}

GcrCertificateExtension *
_gcr_certificate_extension_basic_constraints_parse (GQuark oid,
                                                    gboolean critical,
                                                    GBytes *value,
                                                    GError **error)
{
	GcrCertificateExtensionBasicConstraints *ret = NULL;
	GNode *asn = NULL;
	GNode *node;
	unsigned long temp;
	gboolean is_ca;
	int path_len;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "BasicConstraints", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode BasicConstraints");
		goto out;
	}

	node = egg_asn1x_node (asn, "pathLenConstraint", NULL);
	if (!egg_asn1x_have (node)) {
		path_len = -1;
	} else if (!egg_asn1x_get_integer_as_ulong (node, &temp)) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode pathLenConstraint as integer");
		goto out;
	} else {
		path_len = temp;
	}

	node = egg_asn1x_node (asn, "cA", NULL);
	if (!egg_asn1x_have (node)) {
		is_ca = FALSE;
	} else if (!egg_asn1x_get_boolean (node, &is_ca)) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode cA as boolean");
		goto out;
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->is_ca = is_ca;
	ret->path_len_constraint = path_len;

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
