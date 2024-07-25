/*
 * gnome-keyring
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

#ifndef GCR_CERTIFICATE_EXTENSION_H
#define GCR_CERTIFICATE_EXTENSION_H

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GcrCertificateExtension GcrCertificateExtension;

#define GCR_TYPE_CERTIFICATE_EXTENSION (gcr_certificate_extension_get_type ())
GType gcr_certificate_extension_get_type (void) G_GNUC_CONST;


const char *    gcr_certificate_extension_get_oid          (GcrCertificateExtension *self);

const char *    gcr_certificate_extension_get_description  (GcrCertificateExtension *self);

GBytes *        gcr_certificate_extension_get_value        (GcrCertificateExtension *self);

gboolean        gcr_certificate_extension_is_critical      (GcrCertificateExtension *self);

void            gcr_certificate_extension_free             (GcrCertificateExtension *self);

G_END_DECLS

#endif /* GCR_CERTIFICATE_EXTENSION_H */
