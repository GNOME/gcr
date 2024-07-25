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
 * Author: Niels De Graef <nielsdegraef@gmail.com>
 */

#ifndef GCR_CERTIFICATE_EXTENSION_PRIVATE_H
#define GCR_CERTIFICATE_EXTENSION_PRIVATE_H

#include "gcr-certificate-extension.h"
#include "gcr-certificate-extensions.h"

G_BEGIN_DECLS

GcrCertificateExtension * _gcr_certificate_extension_parse   (GNode *extension);

GQuark        _gcr_certificate_extension_get_oid_as_quark    (GcrCertificateExtension *self);

void          _gcr_certificate_extension_set_oid             (GcrCertificateExtension *self,
                                                              GQuark oid);

void          _gcr_certificate_extension_initialize          (GcrCertificateExtension *self,
                                                              GNode                   *extension_node);

/* We keep the parsing logic internal also */

#define GCR_CERTIFICATE_EXTENSION_PARSE_ERROR (gcr_certificate_extension_parse_error_quark ())
GQuark gcr_certificate_extension_parse_error_quark (void);

typedef enum _GcrCertificateExtensionParseError
{
	GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
} GcrCertificateExtensionParseError;

typedef GcrCertificateExtension *
           (*GcrCertificateExtensionParseFunc)            (GQuark oid,
                                                           gboolean critical,
                                                           GBytes *data,
                                                           GError **error);

G_END_DECLS

#endif /* GCR_CERTIFICATE_EXTENSION_PRIVATE_H */
