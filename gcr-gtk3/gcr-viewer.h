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

#ifndef __GCR_VIEWER_H__
#define __GCR_VIEWER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcr/gcr-types.h"

G_BEGIN_DECLS

#define GCR_TYPE_VIEWER                 (gcr_viewer_get_type())
#define GCR_VIEWER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_VIEWER, GcrViewer))
#define GCR_IS_VIEWER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_VIEWER))
#define GCR_VIEWER_GET_INTERFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GCR_TYPE_VIEWER, GcrViewerIface))

typedef struct _GcrRenderer    GcrRenderer;
typedef struct _GcrViewer      GcrViewer;
typedef struct _GcrViewerIface GcrViewerIface;

struct _GcrViewerIface {
	GTypeInterface parent;

	void (*add_renderer) (GcrViewer *viewer,
	                      GcrRenderer *renderer);

	void (*insert_renderer) (GcrViewer *viewer,
	                         GcrRenderer *renderer,
	                         GcrRenderer *before);

	void (*remove_renderer) (GcrViewer *viewer,
	                         GcrRenderer *renderer);

	guint (*count_renderers) (GcrViewer *viewer);

	GcrRenderer* (*get_renderer) (GcrViewer *viewer,
	                              guint index_);

	/*< private >*/
	gpointer dummy1;
	gpointer dummy2;
	gpointer dummy3;
	gpointer dummy4;
};

GType                   gcr_viewer_get_type               (void);

GcrViewer*              gcr_viewer_new                    (void);

GcrViewer*              gcr_viewer_new_scrolled           (void);

void                    gcr_viewer_add_renderer           (GcrViewer *viewer,
                                                           GcrRenderer *renderer);

void                    gcr_viewer_insert_renderer        (GcrViewer *viewer,
                                                           GcrRenderer *renderer,
                                                           GcrRenderer *before);

void                    gcr_viewer_remove_renderer        (GcrViewer *viewer,
                                                           GcrRenderer *renderer);

guint                   gcr_viewer_count_renderers        (GcrViewer *viewer);

GcrRenderer*            gcr_viewer_get_renderer           (GcrViewer *viewer,
                                                           guint index_);

G_END_DECLS

#endif /* __GCR_VIEWER_H__ */
