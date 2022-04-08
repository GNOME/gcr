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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_FAILURE_RENDERER_H__
#define __GCR_FAILURE_RENDERER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcr-renderer.h"

G_BEGIN_DECLS

#define GCR_TYPE_FAILURE_RENDERER               (gcr_failure_renderer_get_type ())
#define GCR_FAILURE_RENDERER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_FAILURE_RENDERER, GcrFailureRenderer))
#define GCR_FAILURE_RENDERER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_FAILURE_RENDERER, GcrFailureRendererClass))
#define GCR_IS_FAILURE_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_FAILURE_RENDERER))
#define GCR_IS_FAILURE_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_FAILURE_RENDERER))
#define GCR_FAILURE_RENDERER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_FAILURE_RENDERER, GcrFailureRendererClass))

typedef struct _GcrFailureRenderer GcrFailureRenderer;
typedef struct _GcrFailureRendererClass GcrFailureRendererClass;
typedef struct _GcrFailureRendererPrivate GcrFailureRendererPrivate;

struct _GcrFailureRenderer {
	/*< private >*/
	GObject parent;
	GcrFailureRendererPrivate *pv;
};

struct _GcrFailureRendererClass {
	/*< private >*/
	GObjectClass parent_class;
};

GType                  gcr_failure_renderer_get_type         (void);

GcrRenderer *          gcr_failure_renderer_new              (const gchar *label,
                                                              GError *error);

GcrRenderer *          gcr_failure_renderer_new_unsupported  (const gchar *label);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrFailureRenderer, g_object_unref)

G_END_DECLS

#endif /* __GCR_FAILURE_RENDERER_H__ */
