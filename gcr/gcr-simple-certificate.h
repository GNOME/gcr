/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
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
 */

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_SIMPLE_CERTIFICATE_H__
#define __GCR_SIMPLE_CERTIFICATE_H__

#include <glib-object.h>

#include "gcr-certificate.h"

G_BEGIN_DECLS


#define GCR_TYPE_SIMPLE_CERTIFICATE gcr_simple_certificate_get_type ()
G_DECLARE_FINAL_TYPE (GcrSimpleCertificate, gcr_simple_certificate, GCR, SIMPLE_CERTIFICATE, GObject)

GcrCertificate *gcr_simple_certificate_new        (const guchar *data,
                                                   gsize n_data);

GcrCertificate *gcr_simple_certificate_new_static (const guchar *data,
                                                   gsize n_data);

G_END_DECLS

#endif /* __GCR_SIMPLE_CERTIFICATE_H__ */
