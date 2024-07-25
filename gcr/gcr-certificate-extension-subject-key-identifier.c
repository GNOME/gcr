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
 * GcrCertificateExtensionSubjectKeyIdentifier:
 *
 * A certificate extension that contains the subject key identifier (SKI).
 *
 * Since: 4.3.90
 */

struct _GcrCertificateExtensionSubjectKeyIdentifier
{
	GcrCertificateExtension parent_instance;

	GBytes *keyid;
};

G_DEFINE_TYPE (GcrCertificateExtensionSubjectKeyIdentifier,
               gcr_certificate_extension_subject_key_identifier,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static void
gcr_certificate_extension_subject_key_identifier_finalize (GObject *obj)
{
	GcrCertificateExtensionSubjectKeyIdentifier *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER (obj);

	G_OBJECT_CLASS (gcr_certificate_extension_subject_key_identifier_parent_class)->finalize (obj);

	g_bytes_unref (self->keyid);
}

static void
gcr_certificate_extension_subject_key_identifier_class_init (GcrCertificateExtensionSubjectKeyIdentifierClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_certificate_extension_subject_key_identifier_finalize;
}

static void
gcr_certificate_extension_subject_key_identifier_init (GcrCertificateExtensionSubjectKeyIdentifier *self)
{
	self->keyid = NULL;
}

/**
 * gcr_certificate_extension_subject_key_identifier_get_key_id:
 *
 * Returns the raw bytes containing the subject key identifier.
 *
 * Returns: (transfer none): The subject key identifier.
 */
GBytes *
gcr_certificate_extension_subject_key_identifier_get_key_id (GcrCertificateExtensionSubjectKeyIdentifier *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER (self), NULL);

	return self->keyid;
}

GcrCertificateExtension *
_gcr_certificate_extension_subject_key_identifier_parse (GQuark oid,
                                                         gboolean critical,
                                                         GBytes *value,
                                                         GError **error)
{
	GcrCertificateExtensionSubjectKeyIdentifier *ret = NULL;
	GNode *asn = NULL;
	void *data = NULL;
	size_t data_len;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SubjectKeyIdentifier", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode SubjectKeyIdentifier");
		goto out;
	}

	data = egg_asn1x_get_string_as_raw (asn, g_realloc, &data_len);

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->keyid = g_bytes_new_take (data, data_len);

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
