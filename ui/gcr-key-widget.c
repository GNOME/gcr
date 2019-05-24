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

#include "config.h"

#include "gcr-key-renderer.h"
#include "gcr-key-widget.h"
#include "gcr-renderer.h"
#include "gcr-viewer.h"

#include "gck/gck.h"

#include <gdk/gdk.h>
#include <glib/gi18n-lib.h>

/**
 * SECTION:gcr-key-widget
 * @title: GcrKeyWidget
 * @short_description: Key widget and renderer
 *
 * A #GcrKeyWidget can be used to display a RSA, DSA or EC key. The widget
 * is normally in a collapsed state showing only details, but can be expanded
 * by the user.
 *
 * Use gcr_key_widget_new() to create a new key widget. Only
 * one key can be displayed.  A #GcrKeyWidget contains a
 * #GcrViewer internally and #GcrKeyRenderer is used to render the
 * key to the viewer. To show more than one key in a view,
 * create the viewer and add renderers to it.
 */

/**
 * GcrKeyWidget:
 *
 * A widget that displays a key.
 */

/**
 * GcrKeyWidgetClass:
 *
 * The class for #GcrKeyWidget
 */
enum {
	PROP_0,
	PROP_ATTRIBUTES
};

struct _GcrKeyWidget {
	GtkBin parent;

	/*< private >*/
	GcrKeyWidgetPrivate *pv;
};

struct _GcrKeyWidgetClass {
	/*< private >*/
	GtkBinClass parent_class;
};

struct _GcrKeyWidgetPrivate {
	GcrViewer *viewer;
	GcrKeyRenderer *renderer;
};

G_DEFINE_TYPE_WITH_PRIVATE (GcrKeyWidget, gcr_key_widget, GTK_TYPE_BIN);

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static GObject*
gcr_key_widget_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	GObject *obj = G_OBJECT_CLASS (gcr_key_widget_parent_class)->constructor (type, n_props, props);
	GcrKeyWidget *self = NULL;

	g_return_val_if_fail (obj, NULL);

	self = GCR_KEY_WIDGET (obj);

	self->pv->viewer = gcr_viewer_new_scrolled ();
	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->pv->viewer));
	gtk_widget_show (GTK_WIDGET (self->pv->viewer));

	gcr_viewer_add_renderer (self->pv->viewer, GCR_RENDERER (self->pv->renderer));
	return obj;
}

static void
gcr_key_widget_init (GcrKeyWidget *self)
{
	self->pv = gcr_key_widget_get_instance_private (self);
	self->pv->renderer = gcr_key_renderer_new (NULL, NULL);
}

static void
gcr_key_widget_finalize (GObject *obj)
{
	GcrKeyWidget *self = GCR_KEY_WIDGET (obj);

	g_assert (self->pv->renderer);
	g_object_unref (self->pv->renderer);
	self->pv->renderer = NULL;

	g_assert (self->pv->viewer);
	self->pv->viewer = NULL;

	G_OBJECT_CLASS (gcr_key_widget_parent_class)->finalize (obj);
}

static void
gcr_key_widget_set_property (GObject *obj, guint prop_id, const GValue *value,
                                     GParamSpec *pspec)
{
	GcrKeyWidget *self = GCR_KEY_WIDGET (obj);

	switch (prop_id) {
	case PROP_ATTRIBUTES:
		gcr_key_widget_set_attributes (self, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_key_widget_get_property (GObject *obj, guint prop_id, GValue *value,
                                     GParamSpec *pspec)
{
	GcrKeyWidget *self = GCR_KEY_WIDGET (obj);

	switch (prop_id) {
	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, gcr_key_widget_get_attributes (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_key_widget_class_init (GcrKeyWidgetClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gcr_key_widget_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructor = gcr_key_widget_constructor;
	gobject_class->finalize = gcr_key_widget_finalize;
	gobject_class->set_property = gcr_key_widget_set_property;
	gobject_class->get_property = gcr_key_widget_get_property;

	g_object_class_install_property (gobject_class, PROP_ATTRIBUTES,
	         g_param_spec_boxed ("attributes", "Attributes", "The data displayed in the widget",
	                             GCK_TYPE_ATTRIBUTES, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gcr_key_widget_new:
 * @attrs: (allow-none): key to display, or %NULL
 *
 * Create a new key widget which displays a given key in the attributes.
 *
 * Returns: A newly allocated #GcrKeyWidget, which should be freed
 *     with g_object_unref().
 */
GcrKeyWidget*
gcr_key_widget_new (GckAttributes *attrs)
{
	return g_object_new (GCR_TYPE_KEY_WIDGET, "attributes", attrs, NULL);
}

/**
 * gcr_key_widget_set_attributes:
 * @self: The key widget
 * @attrs: (allow-none): the attributes to display
 *
 * Get the attributes displayed in the widget. The attributes should represent
 * either an RSA, DSA or EC key in PKCS\#11 style.
 */
void
gcr_key_widget_set_attributes (GcrKeyWidget *self, GckAttributes *attrs)
{
	g_return_if_fail (GCR_IS_KEY_WIDGET (self));
	gcr_key_renderer_set_attributes (self->pv->renderer, attrs);
}

/**
 * gcr_key_widget_get_attributes:
 * @self: The key widget
 *
 * Get the attributes displayed in the widget.
 *
 * Returns: (allow-none) (transfer none): The attributes, owned by the widget.
 */
GckAttributes*
gcr_key_widget_get_attributes (GcrKeyWidget *self)
{
	g_return_val_if_fail (GCR_IS_KEY_WIDGET (self), NULL);
	return gcr_key_renderer_get_attributes (self->pv->renderer);
}
