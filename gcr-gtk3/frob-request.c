/*
 * gnome-keyring
 *
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

#include "config.h"

#include "gcr/gcr.h"

#include "gcr-gtk3/gcr-certificate-request-renderer.h"

#include <gtk/gtk.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

static void
on_parser_parsed (GcrParser *parser,
                  gpointer user_data)
{
	GcrViewer *viewer;
	GcrRenderer *renderer;
	GtkDialog *dialog = GTK_DIALOG (user_data);

	viewer = gcr_viewer_new_scrolled ();
	renderer = _gcr_certificate_request_renderer_new_for_attributes (gcr_parser_get_parsed_label (parser),
	                                                             gcr_parser_get_parsed_attributes (parser));
	gcr_viewer_add_renderer (viewer, renderer);
	g_object_unref (renderer);
	gtk_widget_show (GTK_WIDGET (viewer));
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (dialog)), GTK_WIDGET (viewer));

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 20);
}

static void
test_request (const gchar *path)
{
	GcrParser *parser;
	GError *err = NULL;
	guchar *data;
	gsize n_data;
	GtkWidget *dialog;
	GBytes *bytes;

	if (!g_file_get_contents (path, (gchar**)&data, &n_data, NULL))
		g_error ("couldn't read file: %s", path);

	dialog = gtk_dialog_new ();
	g_object_ref_sink (dialog);

	parser = gcr_parser_new ();
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), dialog);
	bytes = g_bytes_new_take (data, n_data);
	if (!gcr_parser_parse_data (parser, data, n_data, &err))
		g_error ("couldn't parse data: %s", err->message);

	g_object_unref (parser);
	g_bytes_unref (bytes);

	gtk_widget_show (dialog);
	g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
	gtk_main ();

	g_object_unref (dialog);
}

int
main(int argc, char *argv[])
{
	gtk_init (&argc, &argv);
	g_set_prgname ("frob-request");

	if (argc > 1)
		test_request (argv[1]);
	else
		test_request (SRCDIR "/gcr-gtk3/fixtures/der-rsa-2048.p10");

	return 0;
}
