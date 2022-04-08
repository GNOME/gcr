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

#ifndef __GCR_CERTIFICATE_RENDERER_H__
#define __GCR_CERTIFICATE_RENDERER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcr/gcr-certificate.h"
#include "gcr/gcr-types.h"

#include "gcr-renderer.h"

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_RENDERER               (gcr_certificate_renderer_get_type ())
#define GCR_CERTIFICATE_RENDERER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_CERTIFICATE_RENDERER, GcrCertificateRenderer))
#define GCR_CERTIFICATE_RENDERER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_RENDERER, GcrCertificateRendererClass))
#define GCR_IS_CERTIFICATE_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_CERTIFICATE_RENDERER))
#define GCR_IS_CERTIFICATE_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_RENDERER))
#define GCR_CERTIFICATE_RENDERER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_RENDERER, GcrCertificateRendererClass))

typedef struct _GcrCertificateRenderer GcrCertificateRenderer;
typedef struct _GcrCertificateRendererClass GcrCertificateRendererClass;
typedef struct _GcrCertificateRendererPrivate GcrCertificateRendererPrivate;

struct _GcrCertificateRenderer {
	GObject parent;

	/*< private >*/
	GcrCertificateRendererPrivate *pv;
};

struct _GcrCertificateRendererClass {
	GObjectClass parent_class;
};

GType                     gcr_certificate_renderer_get_type           (void);

GcrCertificateRenderer*   gcr_certificate_renderer_new                (GcrCertificate *certificate);

GcrCertificateRenderer*   gcr_certificate_renderer_new_for_attributes (const gchar *label,
                                                                       struct _GckAttributes *attrs);

GcrCertificate*           gcr_certificate_renderer_get_certificate    (GcrCertificateRenderer *self);

void                      gcr_certificate_renderer_set_certificate    (GcrCertificateRenderer *self,
                                                                       GcrCertificate *certificate);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrCertificateRenderer, g_object_unref)

G_END_DECLS

#endif /* __GCR_CERTIFICATE_RENDERER_H__ */
