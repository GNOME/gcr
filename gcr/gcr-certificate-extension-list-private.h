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

#ifndef GCR_CERTIFICATE_EXTENSION_LIST_PRIVATE_H
#define GCR_CERTIFICATE_EXTENSION_LIST_PRIVATE_H

#include "gcr-certificate-extension-list.h"
#include "gcr-certificate.h"

G_BEGIN_DECLS

GcrCertificateExtensionList *     _gcr_certificate_extension_list_new_for_asn1    (GNode *asn1);

G_END_DECLS

#endif /* GCR_CERTIFICATE_EXTENSION_PRIVATE_H */
