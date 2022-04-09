/*
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

#ifndef __GCR_CERTIFICATE_REQUEST_H__
#define __GCR_CERTIFICATE_REQUEST_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <glib-object.h>

#include "gcr-types.h"

G_BEGIN_DECLS

typedef enum {
	GCR_CERTIFICATE_REQUEST_PKCS10 = 1,
} GcrCertificateRequestFormat;

#define GCR_TYPE_CERTIFICATE_REQUEST (gcr_certificate_request_get_type ())
G_DECLARE_FINAL_TYPE (GcrCertificateRequest, gcr_certificate_request,
                      GCR, CERTIFICATE_REQUEST,
                      GObject)

gboolean                     gcr_certificate_request_capable          (GckObject *private_key,
                                                                       GCancellable *cancellable,
                                                                       GError **error);

void                         gcr_certificate_request_capable_async    (GckObject *private_key,
                                                                       GCancellable *cancellable,
                                                                       GAsyncReadyCallback callback,
                                                                       gpointer user_data);


gboolean                     gcr_certificate_request_capable_finish   (GAsyncResult *result,
                                                                       GError **error);

GcrCertificateRequest *      gcr_certificate_request_prepare          (GcrCertificateRequestFormat format,
                                                                       GckObject *private_key);

GckObject *                  gcr_certificate_request_get_private_key  (GcrCertificateRequest *self);

GcrCertificateRequestFormat  gcr_certificate_request_get_format       (GcrCertificateRequest *self);

void                         gcr_certificate_request_set_cn           (GcrCertificateRequest *self,
                                                                       const gchar *cn);

gboolean                     gcr_certificate_request_complete         (GcrCertificateRequest *self,
                                                                       GCancellable *cancellable,
                                                                       GError **error);

void                         gcr_certificate_request_complete_async   (GcrCertificateRequest *self,
                                                                       GCancellable *cancellable,
                                                                       GAsyncReadyCallback callback,
                                                                       gpointer user_data);

gboolean                     gcr_certificate_request_complete_finish  (GcrCertificateRequest *self,
                                                                       GAsyncResult *result,
                                                                       GError **error);

guchar *                     gcr_certificate_request_encode           (GcrCertificateRequest *self,
                                                                       gboolean textual,
                                                                       gsize *length);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_REQUEST_H__ */
