/*
 * Copyright 2021 Collabora Ltd.
 * Copyright 2010 Stefan Walter
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#ifndef __GCR_CERTIFICATE_WIDGET_H__
#define __GCR_CERTIFICATE_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gcr/gcr-certificate.h>

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_WIDGET gcr_certificate_widget_get_type ()
G_DECLARE_FINAL_TYPE (GcrCertificateWidget, gcr_certificate_widget, GCR, CERTIFICATE_WIDGET, GtkBox)

GtkWidget      *gcr_certificate_widget_new             (GcrCertificate       *certificate);

GcrCertificate *gcr_certificate_widget_get_certificate (GcrCertificateWidget *self);

void            gcr_certificate_widget_set_certificate (GcrCertificateWidget *self,
                                                        GcrCertificate       *certificate);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_WIDGET_H__ */
