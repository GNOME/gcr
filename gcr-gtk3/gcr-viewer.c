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

#include "config.h"

#include "gcr-display-scrolled.h"
#include "gcr-display-view.h"
#include "gcr-renderer.h"
#include "gcr-viewer.h"

/**
 * GcrViewer:
 *
 * An abstract interface that represents a widget that can hold
 * various renderers and display their contents.
 *
 * The interaction between [iface@Renderer] and [iface@Viewer] is not stable
 * yet, and so viewers cannot be implemented outside the Gcr library at this
 * time.
 *
 * Use the [func@Viewer.new] and [func@Viewer.new_scrolled] to get default
 * implementations of viewers.
 */

/**
 * GcrViewerIface:
 * @parent: The parent interface
 * @add_renderer: Virtual method to add a renderer
 * @insert_renderer: Virtual method to insert a renderer
 * @remove_renderer: Virtual method to remove a renderer
 * @count_renderers: Virtual method to count renderers
 * @get_renderer: Virtual method to get a renderer
 *
 * The interface for #GcrViewer
 */

typedef GcrViewerIface GcrViewerInterface;

G_DEFINE_INTERFACE (GcrViewer, gcr_viewer, GTK_TYPE_WIDGET);

static void
gcr_viewer_default_init (GcrViewerIface *iface)
{

}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gcr_viewer_new:
 *
 * Get an implementation of #GcrViewer that supports a view
 * of multiple renderers.
 *
 * Returns: (transfer full): a newly allocated #GcrViewer, which should be
 *          released with g_object_unref()
 */
GcrViewer *
gcr_viewer_new (void)
{
	return GCR_VIEWER (_gcr_display_view_new ());
}

/**
 * gcr_viewer_new_scrolled:
 *
 * Get an implementation of #GcrViewer that supports a scrolled view
 * of multiple renderers.
 *
 * Returns: (transfer floating): a #GcrViewer which is also a #GtkWidget
 */
GcrViewer*
gcr_viewer_new_scrolled (void)
{
	return GCR_VIEWER (_gcr_display_scrolled_new ());
}

/**
 * gcr_viewer_add_renderer: (virtual add_renderer)
 * @viewer: The viewer
 * @renderer: The renderer to add
 *
 * Add a renderer to this viewer.
 */
void
gcr_viewer_add_renderer (GcrViewer *viewer,
                         GcrRenderer *renderer)
{
	g_return_if_fail (GCR_IS_VIEWER (viewer));
	g_return_if_fail (GCR_IS_RENDERER (renderer));
	g_return_if_fail (GCR_VIEWER_GET_INTERFACE (viewer)->add_renderer);
	GCR_VIEWER_GET_INTERFACE (viewer)->add_renderer (viewer, renderer);
}

/**
 * gcr_viewer_insert_renderer: (virtual insert_renderer)
 * @viewer: the viewer
 * @renderer: the renderer to insert
 * @before: (nullable): the renderer to insert before
 *
 * Insert a renderer at a specific point in the viewer
 */
void
gcr_viewer_insert_renderer (GcrViewer *viewer,
                            GcrRenderer *renderer,
                            GcrRenderer *before)
{
	g_return_if_fail (GCR_IS_VIEWER (viewer));
	g_return_if_fail (GCR_IS_RENDERER (renderer));
	g_return_if_fail (!before || GCR_IS_RENDERER (before));
	g_return_if_fail (GCR_VIEWER_GET_INTERFACE (viewer)->insert_renderer);
	GCR_VIEWER_GET_INTERFACE (viewer)->insert_renderer (viewer, renderer, before);
}

/**
 * gcr_viewer_remove_renderer: (virtual remove_renderer)
 * @viewer: The viewer
 * @renderer: The renderer to remove
 *
 * Remove a renderer from this viewer.
 */
void
gcr_viewer_remove_renderer (GcrViewer *viewer,
                            GcrRenderer *renderer)
{
	g_return_if_fail (GCR_IS_VIEWER (viewer));
	g_return_if_fail (GCR_IS_RENDERER (renderer));
	g_return_if_fail (GCR_VIEWER_GET_INTERFACE (viewer)->remove_renderer);
	GCR_VIEWER_GET_INTERFACE (viewer)->remove_renderer (viewer, renderer);
}

/**
 * gcr_viewer_count_renderers: (virtual count_renderers)
 * @viewer: The viewer
 *
 * Get the number of renderers present in the viewer.
 *
 * Returns: The number of renderers.
 */
guint
gcr_viewer_count_renderers (GcrViewer *viewer)
{
	g_return_val_if_fail (GCR_IS_VIEWER (viewer), 0);
	g_return_val_if_fail (GCR_VIEWER_GET_INTERFACE (viewer)->count_renderers, 0);
	return GCR_VIEWER_GET_INTERFACE (viewer)->count_renderers (viewer);
}

/**
 * gcr_viewer_get_renderer: (virtual get_renderer)
 * @viewer: The viewer
 * @index_: The index of the renderer to get
 *
 * Get a pointer to the renderer at the given index. It is an error to request
 * an index that is out of bounds.
 *
 * Returns: (transfer none): the render, owned by the viewer
 */
GcrRenderer*
gcr_viewer_get_renderer (GcrViewer *viewer,
                         guint index_)
{
	g_return_val_if_fail (GCR_IS_VIEWER (viewer), NULL);
	g_return_val_if_fail (GCR_VIEWER_GET_INTERFACE (viewer)->get_renderer, NULL);
	return GCR_VIEWER_GET_INTERFACE (viewer)->get_renderer (viewer, index_);
}
