/*
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

#ifndef __GCR_CERTIFICATE_BASICS_WIDGET_H__
#define __GCR_CERTIFICATE_BASICS_WIDGET_H__

/*
 * GcrCertificateBasicsWidget has been replaced with GcrCertificateWidget.
 * These functions are stubs for GcrCertificateWidget.
 */

#ifndef GCR_DISABLE_DEPRECATED

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcr/gcr-certificate.h"
#include "gcr/gcr-types.h"

#include "gcr-certificate-widget.h"

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_BASICS_WIDGET               (gcr_certificate_basics_widget_get_type ())
#define GCR_CERTIFICATE_BASICS_WIDGET(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_CERTIFICATE_BASICS_WIDGET, GcrCertificateBasicsWidget))
#define GCR_CERTIFICATE_BASICS_WIDGET_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_BASICS_WIDGET, GcrCertificateBasicsWidgetClass))
#define GCR_IS_CERTIFICATE_BASICS_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_CERTIFICATE_BASICS_WIDGET))
#define GCR_IS_CERTIFICATE_BASICS_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_BASICS_WIDGET))
#define GCR_CERTIFICATE_BASICS_WIDGET_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_BASICS_WIDGET, GcrCertificateBasicsWidgetClass))

typedef GcrCertificateWidget GcrCertificateBasicsWidget;
typedef GcrCertificateWidgetClass GcrCertificateBasicsWidgetClass;

G_DEPRECATED_FOR(gcr_certificate_widget_get_type)
GType                        gcr_certificate_basics_widget_get_type               (void);

G_DEPRECATED_FOR(gcr_certificate_widget_new)
GcrCertificateBasicsWidget*  gcr_certificate_basics_widget_new                    (GcrCertificate *cert);

G_DEPRECATED_FOR(gcr_certificate_widget_get_certificate)
GcrCertificate*              gcr_certificate_basics_widget_get_certificate        (GcrCertificateBasicsWidget *basics);

G_DEPRECATED_FOR(gcr_certificate_widget_set_certificate)
void                         gcr_certificate_basics_widget_set_certificate        (GcrCertificateBasicsWidget *basics,
                                                                                   GcrCertificate *cert);

G_END_DECLS

#endif /* GCR_DISABLE_DEPRECATED */

#endif /* __GCR_CERTIFICATE_BASICS_WIDGET_H__ */
