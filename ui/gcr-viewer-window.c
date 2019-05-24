/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-viewer-window.c: Window for viewer

   Copyright (C) 2011 Collabora Ltd.

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr-viewer-window.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <locale.h>
#include <string.h>

struct _GcrViewerWindowPrivate {
	GcrViewerWidget *viewer;
	GcrImportButton *import;
};

G_DEFINE_TYPE_WITH_PRIVATE (GcrViewerWindow, gcr_viewer_window, GTK_TYPE_WINDOW);

static void
on_viewer_renderer_added (GcrViewerWidget *viewer,
                          GcrRenderer *renderer,
                          GcrParsed *parsed,
                          gpointer user_data)
{
	GcrViewerWindow *self = GCR_VIEWER_WINDOW (user_data);
	gcr_import_button_add_parsed (self->pv->import, parsed);
}

static void
gcr_viewer_window_init (GcrViewerWindow *self)
{
	self->pv = gcr_viewer_window_get_instance_private (self);
}

static void
on_import_button_importing (GcrImportButton *button,
                            GcrImporter *importer,
                            gpointer user_data)
{
	GcrViewerWindow *self = GCR_VIEWER_WINDOW (user_data);
	gcr_viewer_widget_clear_error (self->pv->viewer);
}

static void
on_import_button_imported (GcrImportButton *button,
                           GcrImporter *importer,
                           GError *error,
                           gpointer user_data)
{
	GcrViewerWindow *self = GCR_VIEWER_WINDOW (user_data);

	if (error == NULL) {
		g_object_set (button, "label", _("Imported"), NULL);

	} else {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			gcr_viewer_widget_show_error (self->pv->viewer, _("Import failed"), error);
	}
}

static void
on_close_clicked (GtkButton *button,
                  gpointer user_data)
{
	GcrViewerWindow *self = GCR_VIEWER_WINDOW (user_data);
	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
gcr_viewer_window_constructed (GObject *obj)
{
	GcrViewerWindow *self = GCR_VIEWER_WINDOW (obj);
	GtkWidget *bbox;
	GtkWidget *box;
	GtkWidget *button;

	G_OBJECT_CLASS (gcr_viewer_window_parent_class)->constructed (obj);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing (GTK_BOX (bbox), 12);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
	gtk_widget_show (bbox);

	self->pv->import = gcr_import_button_new (_("Import"));
	g_signal_connect_object (self->pv->import, "importing",
	                         G_CALLBACK (on_import_button_importing),
	                         self, 0);
	g_signal_connect_object (self->pv->import, "imported",
	                         G_CALLBACK (on_import_button_imported),
	                         self, 0);
	gtk_widget_show (GTK_WIDGET (self->pv->import));

	button = gtk_button_new_with_mnemonic (_("_Close"));
	g_signal_connect_object  (button, "clicked",
	                          G_CALLBACK (on_close_clicked),
	                          self, 0);
	gtk_widget_show (button);

	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (bbox), GTK_WIDGET (self->pv->import), FALSE, TRUE, 0);

	gtk_widget_set_halign (bbox, 0.5);
	gtk_widget_set_valign (bbox, 0.5);
#if GTK_CHECK_VERSION (3, 12, 0)
	gtk_widget_set_margin_end (bbox, 12);
#else
	gtk_widget_set_margin_right (bbox, 12);
#endif

	self->pv->viewer = gcr_viewer_widget_new ();
	g_object_bind_property (self->pv->viewer, "display-name",
	                        self, "title", G_BINDING_SYNC_CREATE);
	g_signal_connect_object (self->pv->viewer, "added",
	                         G_CALLBACK (on_viewer_renderer_added),
	                         self, 0);
	gtk_widget_show (GTK_WIDGET (self->pv->viewer));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (box);

	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (self->pv->viewer), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), bbox, FALSE, FALSE, 6);

	gtk_container_add (GTK_CONTAINER (self), box);

	gtk_window_set_default_size (GTK_WINDOW (self), 250, 400);
}

static void
gcr_viewer_window_class_init (GcrViewerWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = gcr_viewer_window_constructed;
}

/**
 * gcr_viewer_window_new:
 *
 * Create a new viewer window.
 *
 * Returns: (transfer full) (type GcrUi.ViewerWindow): a new viewer window
 */
GtkWindow *
gcr_viewer_window_new (void)
{
	return g_object_new (GCR_TYPE_VIEWER_WINDOW, NULL);
}

/**
 * gcr_viewer_window_load:
 * @self: a viewer window
 * @file: file to load
 *
 * Load a file into a viewer window. It may not appear immediately.
 */
void
gcr_viewer_window_load (GcrViewerWindow *self,
                        GFile *file)
{
	g_return_if_fail (GCR_IS_VIEWER_WINDOW (self));
	g_return_if_fail (G_IS_FILE (file));

	return gcr_viewer_widget_load_file (self->pv->viewer, file);
}

/**
 * gcr_viewer_window_get_viewer:
 * @self: a viewer window
 *
 * Get the actual viewer showing information in the window.
 *
 * Returns: the viewer
 */
GcrViewer *
gcr_viewer_window_get_viewer (GcrViewerWindow *self)
{
	g_return_val_if_fail (GCR_IS_VIEWER_WINDOW (self), NULL);
	return gcr_viewer_widget_get_viewer (self->pv->viewer);
}
