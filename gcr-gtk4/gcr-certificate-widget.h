/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GCR_CERTIFICATE_WIDGET_H__
#define __GCR_CERTIFICATE_WIDGET_H__

#include <gck/gck.h>
#include <gcr/gcr.h>

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_WIDGET gcr_certificate_widget_get_type ()
G_DECLARE_FINAL_TYPE (GcrCertificateWidget, gcr_certificate_widget, GCR, CERTIFICATE_WIDGET, GtkWidget)

GtkWidget      *gcr_certificate_widget_new             (GcrCertificate       *certificate);

GcrCertificate *gcr_certificate_widget_get_certificate (GcrCertificateWidget *self);

void            gcr_certificate_widget_set_certificate (GcrCertificateWidget *self,
                                                        GcrCertificate       *certificate);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_WIDGET_H__ */
