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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#ifndef GCR_CERTIFICATE_EXTENSIONS_PRIVATE_H
#define GCR_CERTIFICATE_EXTENSIONS_PRIVATE_H

#include "gcr-certificate-extensions.h"

G_BEGIN_DECLS

/* Parse functions */

GcrCertificateExtension * _gcr_certificate_extension_generic_parse                (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_basic_constraints_parse      (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_key_usage_parse              (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_extended_key_usage_parse     (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_subject_key_identifier_parse (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_authority_key_identifier_parse
                                                                                  (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_subject_alt_name_parse       (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_certificate_policies_parse   (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_authority_info_access_parse  (GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);
GcrCertificateExtension * _gcr_certificate_extension_crl_distribution_points_parse(GQuark oid,
                                                                                   gboolean critical,
                                                                                   GBytes *value,
                                                                                   GError **error);

typedef enum {
	GCR_KEY_USAGE_DIGITAL_SIGNATURE = 1 << 0,
	GCR_KEY_USAGE_NON_REPUDIATION = 1 << 1,
	GCR_KEY_USAGE_KEY_ENCIPHERMENT = 1 << 2,
	GCR_KEY_USAGE_DATA_ENCIPHERMENT = 1 << 3,
	GCR_KEY_USAGE_KEY_AGREEMENT = 1 << 4,
	GCR_KEY_USAGE_KEY_CERT_SIGN = 1 << 5,
	GCR_KEY_USAGE_CRL_SIGN = 1 << 6,
	GCR_KEY_USAGE_ENCIPHER_ONLY = 1 << 7,
	GCR_KEY_USAGE_DECIPHER_ONLY = 1 << 8,
} GcrCertificateExtensionKeyUsageBits;

typedef enum {
	GCR_GENERAL_NAME_OTHER,
	GCR_GENERAL_NAME_RFC822,
	GCR_GENERAL_NAME_DNS,
	GCR_GENERAL_NAME_X400,
	GCR_GENERAL_NAME_DN,
	GCR_GENERAL_NAME_EDI,
	GCR_GENERAL_NAME_URI,
	GCR_GENERAL_NAME_IP,
	GCR_GENERAL_NAME_REGISTERED_ID,
} GcrGeneralNameType;

GcrGeneralNames *     _gcr_general_names_parse          (GNode   *node,
                                                         GError **error);

GPtrArray *           _gcr_general_names_steal          (GcrGeneralNames *self);

GcrGeneralName *      _gcr_general_name_parse           (GNode   *node,
                                                         GError **error);

GcrGeneralNameType    _gcr_general_name_get_name_type   (GcrGeneralName  *self);

G_END_DECLS

#endif /* GCR_CERTIFICATE_EXTENSIONS_PRIVATE_H */
