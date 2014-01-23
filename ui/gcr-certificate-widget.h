/*
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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_CERTIFICATE_WIDGET_H__
#define __GCR_CERTIFICATE_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcr/gcr-certificate.h"
#include "gcr/gcr-types.h"

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_WIDGET               (gcr_certificate_widget_get_type ())
#define GCR_CERTIFICATE_WIDGET(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_CERTIFICATE_WIDGET, GcrCertificateWidget))
#define GCR_CERTIFICATE_WIDGET_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_WIDGET, GcrCertificateWidgetClass))
#define GCR_IS_CERTIFICATE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_CERTIFICATE_WIDGET))
#define GCR_IS_CERTIFICATE_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_WIDGET))
#define GCR_CERTIFICATE_WIDGET_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_WIDGET, GcrCertificateWidgetClass))

typedef struct _GcrCertificateWidget GcrCertificateWidget;
typedef struct _GcrCertificateWidgetClass GcrCertificateWidgetClass;
typedef struct _GcrCertificateWidgetPrivate GcrCertificateWidgetPrivate;

struct _GcrCertificateWidget {
	/*< private >*/
	GtkBin parent;
	GcrCertificateWidgetPrivate *pv;
};

struct _GcrCertificateWidgetClass {
	/*< private >*/
	GtkBinClass parent_class;
};

GType                   gcr_certificate_widget_get_type               (void);

GcrCertificateWidget*   gcr_certificate_widget_new                    (GcrCertificate *certificate);

GcrCertificate*         gcr_certificate_widget_get_certificate        (GcrCertificateWidget *self);

void                    gcr_certificate_widget_set_certificate        (GcrCertificateWidget *self,
                                                                       GcrCertificate *certificate);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_WIDGET_H__ */
