/*
 * gcr
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#ifndef GCR_CERTIFICATE_EXTENSIONS_H
#define GCR_CERTIFICATE_EXTENSIONS_H

#include <glib.h>
#include "gcr-certificate-extension.h"

G_BEGIN_DECLS

/* Basic Constraints */

#define GCR_TYPE_CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS (gcr_certificate_extension_basic_constraints_get_type ())
G_DECLARE_FINAL_TYPE (GcrCertificateExtensionBasicConstraints,
                      gcr_certificate_extension_basic_constraints,
                      GCR, CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS,
                      GcrCertificateExtension)

gboolean   gcr_certificate_extension_basic_constraints_is_ca                    (GcrCertificateExtensionBasicConstraints *self);

int        gcr_certificate_extension_basic_constraints_get_path_len_constraint  (GcrCertificateExtensionBasicConstraints *self);


/*  Key Usage */

#define GCR_TYPE_CERTIFICATE_EXTENSION_KEY_USAGE (gcr_certificate_extension_key_usage_get_type ())
G_DECLARE_FINAL_TYPE (GcrCertificateExtensionKeyUsage,
                      gcr_certificate_extension_key_usage,
                      GCR, CERTIFICATE_EXTENSION_KEY_USAGE,
                      GcrCertificateExtension)

unsigned long   gcr_certificate_extension_key_usage_get_usages           (GcrCertificateExtensionKeyUsage *self);

GStrv           gcr_certificate_extension_key_usage_get_descriptions     (GcrCertificateExtensionKeyUsage *self);

/* Extended Key Usage */

#define GCR_TYPE_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE (gcr_certificate_extension_extended_key_usage_get_type ())
G_DECLARE_FINAL_TYPE (GcrCertificateExtensionExtendedKeyUsage,
                      gcr_certificate_extension_extended_key_usage,
                      GCR, CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE,
                      GcrCertificateExtension)

GStrv   gcr_certificate_extension_extended_key_usage_get_oids             (GcrCertificateExtensionExtendedKeyUsage *self);

GStrv   gcr_certificate_extension_extended_key_usage_get_descriptions     (GcrCertificateExtensionExtendedKeyUsage *self);

/* Subject Key Identifier */

#define GCR_TYPE_CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER (gcr_certificate_extension_subject_key_identifier_get_type ())
G_DECLARE_FINAL_TYPE (GcrCertificateExtensionSubjectKeyIdentifier,
                      gcr_certificate_extension_subject_key_identifier,
                      GCR, CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER,
                      GcrCertificateExtension)

GBytes *   gcr_certificate_extension_subject_key_identifier_get_key_id             (GcrCertificateExtensionSubjectKeyIdentifier *self);

/* Subject Alt Name */

#define GCR_TYPE_GENERAL_NAME (gcr_general_name_get_type ())
G_DECLARE_FINAL_TYPE (GcrGeneralName,
                      gcr_general_name,
                      GCR, GENERAL_NAME,
                      GObject)

const char *   gcr_general_name_get_description   (GcrGeneralName  *self);

const char *   gcr_general_name_get_value         (GcrGeneralName  *self);

GBytes *       gcr_general_name_get_value_raw     (GcrGeneralName  *self);


#define GCR_TYPE_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (gcr_certificate_extension_subject_alt_name_get_type ())
G_DECLARE_FINAL_TYPE (GcrCertificateExtensionSubjectAltName,
                      gcr_certificate_extension_subject_alt_name,
                      GCR, CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME,
                      GcrCertificateExtension)

GcrGeneralName * gcr_certificate_extension_subject_alt_name_get_name          (GcrCertificateExtensionSubjectAltName  *self,
                                                                               unsigned int                            position);


G_END_DECLS

#endif /* GCR_CERTIFICATE_EXTENSIONS_H */
