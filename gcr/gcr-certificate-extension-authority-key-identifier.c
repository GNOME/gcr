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
 * GcrCertificateExtensionAuthorityKeyIdentifier:
 *
 * A certificate extension that contains the authority key identifier (SKI).
 *
 * This extension may expose the authority key identifier directly, which
 * should match the subject key identifier of the parent certificate.
 *
 * It _may_ also expose a combination of issuer name and serial number of the
 * used certificate instead. This is rare however.
 *
 * Since: 4.3.91
 */

struct _GcrCertificateExtensionAuthorityKeyIdentifier
{
	GcrCertificateExtension parent_instance;

	GBytes *keyid;

	GcrGeneralNames *issuer_names;
	GBytes *serial_nr;
};

G_DEFINE_TYPE (GcrCertificateExtensionAuthorityKeyIdentifier,
               gcr_certificate_extension_authority_key_identifier,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static void
gcr_certificate_extension_authority_key_identifier_finalize (GObject *obj)
{
	GcrCertificateExtensionAuthorityKeyIdentifier *self = GCR_CERTIFICATE_EXTENSION_AUTHORITY_KEY_IDENTIFIER (obj);

	g_clear_pointer (&self->keyid, g_bytes_unref);
	g_clear_object (&self->issuer_names);
	g_clear_pointer (&self->serial_nr, g_bytes_unref);

	G_OBJECT_CLASS (gcr_certificate_extension_authority_key_identifier_parent_class)->finalize (obj);
}

static void
gcr_certificate_extension_authority_key_identifier_class_init (GcrCertificateExtensionAuthorityKeyIdentifierClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_certificate_extension_authority_key_identifier_finalize;
}

static void
gcr_certificate_extension_authority_key_identifier_init (GcrCertificateExtensionAuthorityKeyIdentifier *self)
{
	self->keyid = NULL;
	self->issuer_names = NULL;
	self->serial_nr = NULL;
}

/**
 * gcr_certificate_extension_authority_key_identifier_get_key_id:
 *
 * Returns the raw bytes containing the authority key identifier, if present.
 *
 * Returns: (transfer none) (nullable): The authority key identifier if present.
 */
GBytes *
gcr_certificate_extension_authority_key_identifier_get_key_id (GcrCertificateExtensionAuthorityKeyIdentifier *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_AUTHORITY_KEY_IDENTIFIER (self), NULL);

	return self->keyid;
}

/**
 * gcr_certificate_extension_authority_key_identifier_get_authority_cert_issuer:
 *
 * Returns the issuer, described by a list of [class@Gcr.GeneralName]s.
 *
 * Returns: (transfer none) (nullable): The names of issuer, if set
 */
GcrGeneralNames *
gcr_certificate_extension_authority_key_identifier_get_authority_cert_issuer (GcrCertificateExtensionAuthorityKeyIdentifier *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_AUTHORITY_KEY_IDENTIFIER (self), NULL);

	return self->issuer_names;
}

/**
 * gcr_certificate_extension_authority_key_identifier_get_authority_cert_serial_number:
 *
 * Returns the serial number of the certificate that was used to sign this
 * certificate.
 *
 * Returns: (transfer none) (nullable): The serial number, if set
 */
GBytes *
gcr_certificate_extension_authority_key_identifier_get_authority_cert_serial_number (GcrCertificateExtensionAuthorityKeyIdentifier *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_AUTHORITY_KEY_IDENTIFIER (self), NULL);

	return self->serial_nr;
}

GcrCertificateExtension *
_gcr_certificate_extension_authority_key_identifier_parse (GQuark oid,
                                                           gboolean critical,
                                                           GBytes *value,
                                                           GError **error)
{
	GcrCertificateExtensionAuthorityKeyIdentifier *ret = NULL;
	GNode *asn = NULL, *node = NULL;
	GBytes *keyid = NULL, *serial_nr = NULL;
	GcrGeneralNames *issuer_names;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "AuthorityKeyIdentifier", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode AuthorityKeyIdentifier");
		goto out;
	}

	node = egg_asn1x_node (asn, "keyIdentifier", NULL);
	if (node != NULL)
		keyid = egg_asn1x_get_string_as_bytes (node);

	node = egg_asn1x_node (asn, "authorityCertIssuer", NULL);
	if (node != NULL) {
		issuer_names = _gcr_general_names_parse (node, error);
		if (issuer_names == NULL)
			goto out;

		if (g_list_model_get_n_items (G_LIST_MODEL (issuer_names) )== 0)
			g_clear_object (&issuer_names);
	}

	node = egg_asn1x_node (asn, "authorityCertSerialNumber", NULL);
	if (node != NULL) {
		serial_nr = egg_asn1x_get_integer_as_raw (node);
	}

	if ((issuer_names == NULL) != (serial_nr == NULL)) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Authority Cert Issuer and Serial Nr should either both be set or absent");
		goto out;
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_AUTHORITY_KEY_IDENTIFIER,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	if (keyid != NULL)
		ret->keyid = g_steal_pointer (&keyid);
	if (issuer_names != NULL) {
		ret->issuer_names = g_steal_pointer (&issuer_names);
		ret->serial_nr = g_steal_pointer (&serial_nr);
	}

out:
	g_clear_pointer (&keyid, g_bytes_unref);
	g_clear_object (&issuer_names);
	g_clear_pointer (&serial_nr, g_bytes_unref);
	g_clear_pointer (&asn, egg_asn1x_destroy);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
