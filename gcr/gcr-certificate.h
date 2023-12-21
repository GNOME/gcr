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

#ifndef __GCR_CERTIFICATE_H__
#define __GCR_CERTIFICATE_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-types.h"

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE                    (gcr_certificate_get_type ())
#define GCR_CERTIFICATE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_CERTIFICATE, GcrCertificate))
#define GCR_IS_CERTIFICATE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_CERTIFICATE))
#define GCR_CERTIFICATE_GET_INTERFACE(inst)     (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GCR_TYPE_CERTIFICATE, GcrCertificateIface))

typedef struct _GcrCertificate          GcrCertificate;
typedef struct _GcrCertificateIface     GcrCertificateIface;

struct _GcrCertificateIface {
	GTypeInterface parent;


	/**
	 * GcrCertificateIface::get_der_data:
	 * @self: a #GcrCertificate
	 * @n_data: (out): a location to store the size of the resulting DER data.
	 *
	 * Implemented to return the raw DER data for an X.509 certificate. The data
	 * should be owned by the #GcrCertificate object.
	 *
	 * Returns: (transfer none) (array length=n_data): raw DER data of the X.509 certificate
	 */
	const guint8 * (* get_der_data) (GcrCertificate *self,
	                                 gsize *n_data);

	/*< private >*/
	gpointer dummy1;
	gpointer dummy2;
	gpointer dummy3;
	gpointer dummy5;
	gpointer dummy6;
	gpointer dummy7;
	gpointer dummy8;
};

GType               gcr_certificate_get_type               (void);

const guint8 *      gcr_certificate_get_der_data           (GcrCertificate *self,
                                                            gsize *n_data);

gchar *             gcr_certificate_get_issuer_name        (GcrCertificate *self);

gchar*              gcr_certificate_get_issuer_cn          (GcrCertificate *self);

gchar*              gcr_certificate_get_issuer_dn          (GcrCertificate *self);

gchar*              gcr_certificate_get_issuer_part        (GcrCertificate *self,
                                                            const gchar *part);

guchar *            gcr_certificate_get_issuer_raw         (GcrCertificate *self,
                                                            gsize *n_data);

gboolean            gcr_certificate_is_issuer              (GcrCertificate *self,
                                                            GcrCertificate *issuer);

gchar *             gcr_certificate_get_subject_name       (GcrCertificate *self);

gchar*              gcr_certificate_get_subject_cn         (GcrCertificate *self);

gchar*              gcr_certificate_get_subject_dn         (GcrCertificate *self);

gchar*              gcr_certificate_get_subject_part       (GcrCertificate *self,
                                                            const gchar *part);

guchar *            gcr_certificate_get_subject_raw        (GcrCertificate *self,
                                                            gsize *n_data);

GDateTime *         gcr_certificate_get_issued_date        (GcrCertificate *self);

GDateTime *         gcr_certificate_get_expiry_date        (GcrCertificate *self);

gulong              gcr_certificate_get_version            (GcrCertificate *self);

guchar*             gcr_certificate_get_serial_number      (GcrCertificate *self,
                                                            gsize *n_length);

gchar*              gcr_certificate_get_serial_number_hex  (GcrCertificate *self);

guint               gcr_certificate_get_key_size           (GcrCertificate *self);

guchar*             gcr_certificate_get_fingerprint        (GcrCertificate *self,
                                                            GChecksumType type,
                                                            gsize *n_length);

gchar*              gcr_certificate_get_fingerprint_hex    (GcrCertificate *self,
                                                            GChecksumType type);

gboolean            gcr_certificate_get_basic_constraints  (GcrCertificate *self,
                                                            gboolean *is_ca,
                                                            gint *path_len);

GList*              gcr_certificate_get_interface_elements (GcrCertificate *self);

void                gcr_certificate_mixin_emit_notify      (GcrCertificate *self);

void                gcr_certificate_mixin_class_init       (GObjectClass *object_class);

void                gcr_certificate_mixin_get_property     (GObject *obj,
                                                            guint prop_id,
                                                            GValue *value,
                                                            GParamSpec *pspec);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrCertificate, g_object_unref)

G_END_DECLS

#endif /* __GCR_CERTIFICATE_H__ */
