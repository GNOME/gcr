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
 * GcrCertificateExtensionKeyUsage:
 *
 * A certificate extension that can be used to restrict the usages of a given
 * certificate.
 *
 * See also [class@Gcr.CertificateExtensionExtendedKeyUsage] for an additional
 * set of usages.
 *
 * Since: 4.3.90
 */

struct _GcrCertificateExtensionKeyUsage
{
	GcrCertificateExtension parent_instance;

	unsigned long usages;
};

G_DEFINE_TYPE (GcrCertificateExtensionKeyUsage,
               gcr_certificate_extension_key_usage,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static const struct {
	unsigned int usage;
	const char *description;
} key_usage_descriptions[] = {
	{ GCR_KEY_USAGE_DIGITAL_SIGNATURE, N_("Digital signature") },
	{ GCR_KEY_USAGE_NON_REPUDIATION, N_("Non repudiation") },
	{ GCR_KEY_USAGE_KEY_ENCIPHERMENT, N_("Key encipherment") },
	{ GCR_KEY_USAGE_DATA_ENCIPHERMENT, N_("Data encipherment") },
	{ GCR_KEY_USAGE_KEY_AGREEMENT, N_("Key agreement") },
	{ GCR_KEY_USAGE_KEY_CERT_SIGN, N_("Certificate signature") },
	{ GCR_KEY_USAGE_CRL_SIGN, N_("Revocation list signature") },
	{ GCR_KEY_USAGE_ENCIPHER_ONLY, N_("Encipher only") },
	{ GCR_KEY_USAGE_DECIPHER_ONLY, N_("Decipher only") }
};

static void
gcr_certificate_extension_key_usage_class_init (GcrCertificateExtensionKeyUsageClass *klass)
{
}

static void
gcr_certificate_extension_key_usage_init (GcrCertificateExtensionKeyUsage *self)
{
}

/**
 * gcr_certificate_extension_key_usage_get_usages:
 *
 * Returns the bit string describing the usages.
 */
unsigned long
gcr_certificate_extension_key_usage_get_usages (GcrCertificateExtensionKeyUsage *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_KEY_USAGE (self), 0);

	return self->usages;
}

/**
 * gcr_certificate_extension_key_usage_get_descriptions:
 *
 * Returns a user-friendly list of description of the key usages.
 *
 * Returns: (transfer full): The descriptions.
 */
GStrv
gcr_certificate_extension_key_usage_get_descriptions (GcrCertificateExtensionKeyUsage *self)
{
	GPtrArray *values;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_KEY_USAGE (self), NULL);

	values = g_ptr_array_new_with_free_func (g_free);
	for (size_t i = 0; i < G_N_ELEMENTS (key_usage_descriptions); i++) {
		if (self->usages & key_usage_descriptions[i].usage) {
			const char *value;
			value = _(key_usage_descriptions[i].description);
			g_ptr_array_add (values, g_strdup (value));
		}
	}

	g_ptr_array_add (values, NULL); /* Add NULL terminator */
	return (GStrv) g_ptr_array_free (values, FALSE);
}

static unsigned long
_gcr_reverse_bits (unsigned long num, unsigned int n_bits)
{
    unsigned long reverse_num = 0;
    for (unsigned int i = 0; i < n_bits; i++) {
        if ((num & (1 << i)))
            reverse_num |= 1 << ((n_bits - 1) - i);
    }
    return reverse_num;
}

GcrCertificateExtension *
_gcr_certificate_extension_key_usage_parse (GQuark oid,
                                            gboolean critical,
                                            GBytes *value,
                                            GError **error)
{
	GcrCertificateExtensionKeyUsage *ret = NULL;
	GNode *asn = NULL;
	unsigned long key_usage;
	unsigned int n_bits;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "KeyUsage", value);
	if (asn == NULL) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't decode KeyUsage");
		goto out;
	}

	if (!egg_asn1x_get_bits_as_ulong (asn, &key_usage, &n_bits)) {
		g_set_error_literal (error,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
		                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
		                     "Couldn't parse KeyUsage as bits");
		goto out;
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_KEY_USAGE,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->usages = _gcr_reverse_bits (key_usage, n_bits);

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
