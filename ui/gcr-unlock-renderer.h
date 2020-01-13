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

#ifndef __GCR_UNLOCK_RENDERER_H__
#define __GCR_UNLOCK_RENDERER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcr/gcr-types.h"

#include "gcr-renderer.h"

G_BEGIN_DECLS

#define GCR_TYPE_UNLOCK_RENDERER               (_gcr_unlock_renderer_get_type ())
#define GCR_UNLOCK_RENDERER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_UNLOCK_RENDERER, GcrUnlockRenderer))
#define GCR_UNLOCK_RENDERER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_UNLOCK_RENDERER, GcrUnlockRendererClass))
#define GCR_IS_UNLOCK_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_UNLOCK_RENDERER))
#define GCR_IS_UNLOCK_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_UNLOCK_RENDERER))
#define GCR_UNLOCK_RENDERER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_UNLOCK_RENDERER, GcrUnlockRendererClass))

typedef struct _GcrUnlockRenderer GcrUnlockRenderer;
typedef struct _GcrUnlockRendererClass GcrUnlockRendererClass;
typedef struct _GcrUnlockRendererPrivate GcrUnlockRendererPrivate;

struct _GcrUnlockRenderer {
	/*< private >*/
	GtkBin parent;
	GcrUnlockRendererPrivate *pv;
};

struct _GcrUnlockRendererClass {
	GtkBinClass parent_class;

	/* signals */
	void       (*unlock_clicked)        (GcrUnlockRenderer *unlock);
};

GType                  _gcr_unlock_renderer_get_type          (void);

GcrUnlockRenderer *    _gcr_unlock_renderer_new               (const gchar *label,
                                                               GBytes *locked_data);

GcrUnlockRenderer *    _gcr_unlock_renderer_new_for_parsed    (GcrParser *parser);

const gchar *          _gcr_unlock_renderer_get_password      (GcrUnlockRenderer *self);

void                   _gcr_unlock_renderer_set_password      (GcrUnlockRenderer *self,
                                                               const gchar *text);

void                   _gcr_unlock_renderer_focus_password    (GcrUnlockRenderer *self);

void                   _gcr_unlock_renderer_show_warning      (GcrUnlockRenderer *self,
                                                               const gchar *message);

GBytes *               _gcr_unlock_renderer_get_locked_data   (GcrUnlockRenderer *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrUnlockRenderer, g_object_unref)

G_END_DECLS

#endif /* __GCR_UNLOCK_RENDERER_H__ */
