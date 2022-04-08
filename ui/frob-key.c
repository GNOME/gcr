/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Collabora Ltd.
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
#include "ui/gcr-ui.h"

#include <gtk/gtk.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

static void
on_parser_parsed (GcrParser *parser, gpointer unused)
{
	GcrKeyWidget *details;
	GtkDialog *dialog;

	dialog = GTK_DIALOG (gtk_dialog_new ());
	g_object_ref_sink (dialog);

	details = gcr_key_widget_new (gcr_parser_get_parsed_attributes (parser));
	gtk_widget_show (GTK_WIDGET (details));
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (dialog)), GTK_WIDGET (details));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 550, 400);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 20);
	gtk_dialog_run (dialog);

	g_object_unref (dialog);
	g_object_unref (details);
}

static void
test_key (const gchar *path)
{
	GcrParser *parser;
	GError *err = NULL;
	GBytes *bytes;
	guchar *data;
	gsize n_data;

	if (!g_file_get_contents (path, (gchar**)&data, &n_data, NULL))
		g_error ("couldn't read file: %s", path);

	parser = gcr_parser_new ();
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), NULL);
	bytes = g_bytes_new_take (data, n_data);
	if (!gcr_parser_parse_bytes (parser, bytes, &err))
		g_error ("couldn't parse data: %s", err->message);

	g_object_unref (parser);
	g_bytes_unref (bytes);
}

int
main(int argc, char *argv[])
{
	gtk_init (&argc, &argv);
	g_set_prgname ("frob-key");

	if (argc > 1)
		test_key (argv[1]);
	else
		test_key (SRCDIR "/ui/fixtures/pem-dsa-1024.key");

	return 0;
}
