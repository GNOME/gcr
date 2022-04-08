/*
 * gnome-keyring
 *
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

#include "config.h"

#include "gcr-deprecated.h"
#include "gcr-renderer.h"

#include "gcr-certificate-renderer.h"
#include "gcr-certificate-request-renderer.h"
#include "gcr-gnupg-renderer.h"
#include "gcr-key-renderer.h"

#include "gck/gck.h"

#include <gtk/gtk.h>

/**
 * GcrRenderer:
 *
 * An interface that's implemented by renderers which wish to render data to a
 * [iface@Viewer].
 *
 * The interaction between [iface@Renderer] and [iface@Viewer] is not stable
 * yet, and so new renderers cannot be implemented outside the Gcr library at
 * this time.
 *
 * To lookup a renderer for a given set of attributes, use the gcr_renderer_create()
 * function. This will create and initialize a renderer that's capable of viewing
 * the data in those attributes.
 */

/**
 * GcrRendererIface:
 * @parent: the parent interface type
 * @data_changed: signal emitted when data being rendered changes
 * @render_view: method invoked to render the data into a viewer
 * @populate_popup: method invoked to populate a popup menu with additional
 *                  renderer options
 *
 * The interface for #GcrRenderer
 */

enum {
	DATA_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _GcrRegistered {
	GckAttributes *attrs;
	GType renderer_type;
} GcrRegistered;

static GArray *registered_renderers = NULL;
static gboolean registered_sorted = FALSE;

static void
gcr_renderer_default_init (GcrRendererIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {

		/**
		 * GcrRenderer:label:
		 *
		 * The label to display.
		 */
		g_object_interface_install_property (iface,
		         g_param_spec_string ("label", "Label", "The label for the renderer",
		                              "",
		                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrRenderer:attributes:
		 *
		 * The attributes to display.
		 */
		g_object_interface_install_property (iface,
		         g_param_spec_boxed ("attributes", "Attributes", "The data displayed in the renderer",
		                             GCK_TYPE_ATTRIBUTES,
		                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrRenderer::data-changed:
		 *
		 * A signal that is emitted by the renderer when it's data
		 * changed and should be rerendered.
		 */
		signals[DATA_CHANGED] = g_signal_new ("data-changed", GCR_TYPE_RENDERER, G_SIGNAL_RUN_LAST,
		                                      G_STRUCT_OFFSET (GcrRendererIface, data_changed),
		                                      NULL, NULL, NULL, G_TYPE_NONE, 0);

		initialized = TRUE;
	}
}

typedef GcrRendererIface GcrRendererInterface;

G_DEFINE_INTERFACE (GcrRenderer, gcr_renderer, G_TYPE_OBJECT);

/**
 * gcr_renderer_render_view:
 * @self: The renderer
 * @viewer: The viewer to render to.
 *
 * Render the contents of the renderer to the given viewer.
 */
void
gcr_renderer_render_view (GcrRenderer *self, GcrViewer *viewer)
{
	g_return_if_fail (GCR_IS_RENDERER (self));
	g_return_if_fail (GCR_RENDERER_GET_INTERFACE (self)->render_view);
	GCR_RENDERER_GET_INTERFACE (self)->render_view (self, viewer);
}

/**
 * gcr_renderer_render:
 * @self: the renderer
 * @viewer: the viewer to render to
 *
 * Render a renderer to the viewer.
 *
 * Deprecated: 3.2: Use gcr_renderer_render_view() instead
 */
void
gcr_renderer_render (GcrRenderer *self,
                     GcrViewer *viewer)
{
	gcr_renderer_render_view (self, viewer);
}

/**
 * gcr_renderer_popuplate_popup:
 * @self: The renderer
 * @viewer: The viewer that is displaying a popup
 * @menu: The popup menu being displayed
 *
 * Called by #GcrViewer when about to display a popup menu for the content
 * displayed by the renderer. The renderer can add a menu item if desired.
 */
void
gcr_renderer_popuplate_popup (GcrRenderer *self, GcrViewer *viewer,
                              GtkMenu *menu)
{
	g_return_if_fail (GCR_IS_RENDERER (self));
	if (GCR_RENDERER_GET_INTERFACE (self)->populate_popup)
		GCR_RENDERER_GET_INTERFACE (self)->populate_popup (self, viewer, menu);
}

/**
 * gcr_renderer_emit_data_changed:
 * @self: The renderer
 *
 * Emit the #GcrRenderer::data-changed signal on the renderer. This is used by
 * renderer implementations.
 */
void
gcr_renderer_emit_data_changed (GcrRenderer *self)
{
	g_return_if_fail (GCR_IS_RENDERER (self));
	g_signal_emit (self, signals[DATA_CHANGED], 0);
}

/**
 * gcr_renderer_get_attributes:
 * @self: The renderer
 *
 * Get the PKCS#11 attributes, if any, set for this renderer to display.
 *
 * Returns: (nullable) (transfer none): the attributes, owned by the renderer
 */
GckAttributes *
gcr_renderer_get_attributes (GcrRenderer *self)
{
	GckAttributes *attrs;

	g_return_val_if_fail (GCR_IS_RENDERER (self), NULL);

	g_object_get (self, "attributes", &attrs, NULL);
	if (attrs != NULL)
		gck_attributes_unref (attrs);
	return attrs;
}

/**
 * gcr_renderer_set_attributes:
 * @self: The renderer
 * @attrs: (nullable): attributes to set
 *
 * Set the PKCS#11 attributes for this renderer to display.
 */
void
gcr_renderer_set_attributes (GcrRenderer *self,
                             GckAttributes *attrs)
{
	g_return_if_fail (GCR_IS_RENDERER (self));

	g_object_set (self, "attributes", attrs, NULL);
}

static gint
sort_registered_by_n_attrs (gconstpointer a, gconstpointer b)
{
	const GcrRegistered *ra = a;
	const GcrRegistered *rb = b;
	gulong na, nb;

	g_assert (a);
	g_assert (b);

	na = gck_attributes_count (ra->attrs);
	nb = gck_attributes_count (rb->attrs);

	/* Note we're sorting in reverse order */
	if (na < nb)
		return 1;
	return (na == nb) ? 0 : -1;
}

/**
 * gcr_renderer_create:
 * @label: (nullable): The label for the renderer
 * @attrs: The attributes to render
 *
 * Create and initialize a renderer for the given attributes and label. These
 * renderers should have been preregistered via gcr_renderer_register().
 *
 * Returns: (transfer full) (nullable): a new renderer, or %NULL if no renderer
 *          matched the attributes; the render should be released with g_object_unref()
 */
GcrRenderer *
gcr_renderer_create (const gchar *label, GckAttributes *attrs)
{
	GcrRegistered *registered;
	gboolean matched;
	gulong n_attrs;
	gulong j;
	gsize i;

	g_return_val_if_fail (attrs, NULL);

	gcr_renderer_register_well_known ();

	if (!registered_renderers)
		return NULL;

	if (!registered_sorted) {
		g_array_sort (registered_renderers, sort_registered_by_n_attrs);
		registered_sorted = TRUE;
	}

	for (i = 0; i < registered_renderers->len; ++i) {
		registered = &(g_array_index (registered_renderers, GcrRegistered, i));
		n_attrs = gck_attributes_count (registered->attrs);

		matched = TRUE;

		for (j = 0; j < n_attrs; ++j) {
			if (!gck_attributes_contains (attrs, gck_attributes_at (registered->attrs, j))) {
				matched = FALSE;
				break;
			}
		}

		if (matched)
			return g_object_new (registered->renderer_type, "label", label,
			                     "attributes", attrs, NULL);
	}

	return NULL;
}

/**
 * gcr_renderer_register:
 * @renderer_type: The renderer class type
 * @attrs: The attributes to match
 *
 * Register a renderer to be created when matching attributes are passed to
 * gcr_renderer_create().
 */
void
gcr_renderer_register (GType renderer_type, GckAttributes *attrs)
{
	GcrRegistered registered;

	if (!registered_renderers)
		registered_renderers = g_array_new (FALSE, FALSE, sizeof (GcrRegistered));

	registered.renderer_type = renderer_type;
	registered.attrs = gck_attributes_ref_sink (attrs);
	g_array_append_val (registered_renderers, registered);
	registered_sorted = FALSE;
}

/**
 * gcr_renderer_register_well_known:
 *
 * Register all the well known renderers for certificates and keys known to the
 * Gcr library.
 */
void
gcr_renderer_register_well_known (void)
{
	g_type_class_unref (g_type_class_ref (GCR_TYPE_CERTIFICATE_RENDERER));
	g_type_class_unref (g_type_class_ref (GCR_TYPE_CERTIFICATE_REQUEST_RENDERER));
	g_type_class_unref (g_type_class_ref (GCR_TYPE_KEY_RENDERER));
	g_type_class_unref (g_type_class_ref (GCR_TYPE_GNUPG_RENDERER));
}
