/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Stefan Walter
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

#ifndef __GCR_TRUST_H__
#define __GCR_TRUST_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-certificate.h"
#include "gcr-types.h"

G_BEGIN_DECLS

#define GCR_PURPOSE_SERVER_AUTH "1.3.6.1.5.5.7.3.1"
#define GCR_PURPOSE_CLIENT_AUTH "1.3.6.1.5.5.7.3.2"
#define GCR_PURPOSE_CODE_SIGNING "1.3.6.1.5.5.7.3.3"
#define GCR_PURPOSE_EMAIL "1.3.6.1.5.5.7.3.4"

gboolean       gcr_trust_is_certificate_pinned                 (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                const gchar *peer,
                                                                GCancellable *cancellable,
                                                                GError **error);

void           gcr_trust_is_certificate_pinned_async           (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                const gchar *peer,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

gboolean       gcr_trust_is_certificate_pinned_finish          (GAsyncResult *result,
                                                                GError **error);

gboolean       gcr_trust_add_pinned_certificate                (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                const gchar *peer,
                                                                GCancellable *cancellable,
                                                                GError **error);

void           gcr_trust_add_pinned_certificate_async          (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                const gchar *peer,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

gboolean       gcr_trust_add_pinned_certificate_finish         (GAsyncResult *result,
                                                                GError **error);

gboolean       gcr_trust_remove_pinned_certificate             (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                const gchar *peer,
                                                                GCancellable *cancellable,
                                                                GError **error);

void           gcr_trust_remove_pinned_certificate_async       (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                const gchar *peer,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

gboolean       gcr_trust_remove_pinned_certificate_finish      (GAsyncResult *result,
                                                                GError **error);

gboolean       gcr_trust_is_certificate_anchored               (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                GCancellable *cancellable,
                                                                GError **error);

void           gcr_trust_is_certificate_anchored_async         (GcrCertificate *certificate,
                                                                const gchar *purpose,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

gboolean       gcr_trust_is_certificate_anchored_finish        (GAsyncResult *result,
                                                                GError **error);

gboolean       gcr_trust_is_certificate_distrusted             (unsigned char *serial_nr,
                                                                size_t         serial_nr_len,
                                                                unsigned char *issuer,
                                                                size_t         issuer_len,
                                                                GCancellable  *cancellable,
                                                                GError       **error);

void           gcr_trust_is_certificate_distrusted_async      (unsigned char      *serial_nr,
                                                               size_t              serial_nr_len,
                                                               unsigned char      *issuer,
                                                               size_t              issuer_len,
                                                               GCancellable       *cancellable,
                                                               GAsyncReadyCallback callback,
                                                               void               *user_data);

gboolean       gcr_trust_is_certificate_distrusted_finish     (GAsyncResult *result,
                                                               GError      **error);

G_END_DECLS

#endif /* __GCR_TOKEN_MANAGER_H__ */
