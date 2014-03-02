/*
 * Copyright (C) 2014 Stef Walter
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
 * Author: Stef Walter <stefw@redhat.com>
 */

#include "config.h"

#include "ui/gcr-ui.h"

#include "gcr/gcr-importer.h"

#include <gtk/gtk.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

typedef GObject MockImporter;
typedef GObjectClass MockImporterClass;

enum {
	PROP_0,
	PROP_LABEL,
	PROP_ICON,
	PROP_URI,
	PROP_INTERACTION
};

GType mock_importer_get_type (void) G_GNUC_CONST;

static GList *
mock_importer_create_for_parsed (GcrParsed *parsed)
{
	GcrImporter *self;
	GIcon *icon;

	icon = g_themed_icon_new ("dialog-warning");
	self = g_object_new (mock_importer_get_type (),
	                     "label", gcr_parsed_get_label (parsed),
	                     "icon", icon,
	                     NULL);
	g_object_unref (icon);
	return g_list_append (NULL, self);
}

static gboolean
mock_importer_queue_for_parsed (GcrImporter *importer,
                                GcrParsed *parsed)
{
	return TRUE;
}

static void
mock_importer_import_async (GcrImporter *importer,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (importer), callback, user_data,
	                                 mock_importer_import_async);

	g_printerr ("Import %p\n", importer);
	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}


static gboolean
mock_importer_import_finish (GcrImporter *importer,
                             GAsyncResult *result,
                             GError **error)
{
	return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);
}


static void
mock_importer_iface (GcrImporterIface *iface)
{
	iface->create_for_parsed = mock_importer_create_for_parsed;
	iface->queue_for_parsed = mock_importer_queue_for_parsed;
	iface->import_async = mock_importer_import_async;
	iface->import_finish = mock_importer_import_finish;
}

G_DEFINE_TYPE_WITH_CODE (MockImporter, mock_importer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_IMPORTER, mock_importer_iface);
);

static void
value_free (gpointer value)
{
	g_boxed_free (G_TYPE_VALUE, value);
}

static void
mock_importer_init (MockImporter *self)
{

}

static void
mock_importer_set_property (GObject *obj,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	g_object_set_data_full (obj, pspec->name, g_boxed_copy (G_TYPE_VALUE, value), value_free);
}

static void
mock_importer_get_property (GObject *obj,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	GValue *val = g_object_get_data (obj, pspec->name);
	g_return_if_fail (val != NULL);
	g_value_copy (val, value);
}

static void
mock_importer_class_init (MockImporterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->set_property = mock_importer_set_property;
	gobject_class->get_property = mock_importer_get_property;

	g_object_class_install_property (gobject_class, PROP_LABEL,
	           g_param_spec_string ("label", "", "",
	                                NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_ICON,
	           g_param_spec_object ("icon", "", "",
	                                G_TYPE_ICON, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_override_property (gobject_class, PROP_URI, "uri");
	g_object_class_override_property (gobject_class, PROP_INTERACTION, "interaction");
}

typedef MockImporter MockImporterTwo;
typedef MockImporterClass MockImporterTwoClass;

GType mock_importer_two_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MockImporterTwo, mock_importer_two, mock_importer_get_type ());

static void
mock_importer_two_init (MockImporterTwo *self)
{

}

static void
mock_importer_two_class_init (MockImporterTwoClass *klass)
{

}


static void
on_parser_parsed (GcrParser *parser,
                  gpointer user_data)
{
	gcr_import_button_add_parsed (user_data,
	                              gcr_parser_get_parsed (parser));
}

static void
parse_file (GcrParser *parser,
            const gchar *path)
{
	GError *err = NULL;
	guchar *data;
	gsize n_data;
	GBytes *bytes;

	if (!g_file_get_contents (path, (gchar**)&data, &n_data, NULL))
		g_error ("couldn't read file: %s", path);

	bytes = g_bytes_new_take (data, n_data);
	if (!gcr_parser_parse_bytes (parser, bytes, &err))
		g_error ("couldn't parse data: %s", err->message);

	g_bytes_unref (bytes);
}

int
main (int argc, char *argv[])
{
	GtkDialog *dialog;
	GcrParser *parser;
	GtkWidget *button;
	GtkWidget *align;
	int i;

	gtk_init (&argc, &argv);

	gcr_importer_register (mock_importer_get_type (), gck_attributes_new (0));
	gcr_importer_register (mock_importer_two_get_type (), gck_attributes_new (0));

	dialog = GTK_DIALOG (gtk_dialog_new ());
	g_object_ref_sink (dialog);

	button = GTK_WIDGET (gcr_import_button_new ("Import Button"));
	gtk_widget_show (button);

	align = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER (align), button);
	gtk_widget_show (align);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (dialog)), align);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 200, 300);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 20);

	parser = gcr_parser_new ();
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), button);

	if (argc == 1) {
		parse_file (parser, SRCDIR "/ui/fixtures/ca-certificates.crt");
	} else {
		for (i = 1; i < argc; ++i)
			parse_file (parser, argv[i]);
	}

	g_object_unref (parser);

	gtk_dialog_run (dialog);

	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_object_unref (dialog);

	return 0;
}
