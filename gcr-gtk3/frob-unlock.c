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

#include "gcr-gtk3/gcr-unlock-renderer.h"
#include "gcr-gtk3/gcr-viewer-window.h"

#include <gtk/gtk.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

static gboolean
delete_event(GtkWidget *widget,
             GdkEvent *event,
             gpointer data)
{
	gtk_main_quit ();
	return FALSE;
}

static void
on_parser_parsed (GcrParser *parser, gpointer unused)
{

}

static void
on_unlock_clicked (GcrUnlockRenderer *renderer,
                   GcrParser *parser)
{
	const gchar *password;
	password = _gcr_unlock_renderer_get_password (renderer);
	gcr_parser_add_password (parser, password);
	gtk_main_quit ();
}

static void
on_parser_authenticate (GcrParser *parser,
                        gint count,
                        gpointer user_data)
{
	GcrUnlockRenderer *renderer;
	GtkWindow *window;

	window = GTK_WINDOW (gcr_viewer_window_new ());
	g_object_ref_sink (window);

	renderer = _gcr_unlock_renderer_new_for_parsed (parser);
	g_signal_connect (renderer, "unlock-clicked", G_CALLBACK (on_unlock_clicked), parser);
	gcr_viewer_add_renderer (gcr_viewer_window_get_viewer (GCR_VIEWER_WINDOW (window)), GCR_RENDERER (renderer));
	g_object_unref (renderer);

	gtk_window_set_default_size (window, 550, 400);
	gtk_container_set_border_width (GTK_CONTAINER (window), 20);

	g_signal_connect (window, "delete-event", G_CALLBACK (delete_event), NULL);
	gtk_widget_show (GTK_WIDGET (window));

	gtk_main ();

	g_object_unref (window);
}

static void
test_key (const gchar *path)
{
	GcrParser *parser;
	GError *err = NULL;
	guchar *data;
	gsize n_data;
	GBytes *bytes;

	if (!g_file_get_contents (path, (gchar**)&data, &n_data, NULL))
		g_error ("couldn't read file: %s", path);

	parser = gcr_parser_new ();
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), NULL);
	g_signal_connect (parser, "authenticate", G_CALLBACK (on_parser_authenticate), NULL);
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
	g_set_prgname ("frob-unlock");

	if (argc > 1) {
		test_key (argv[1]);
	} else {
		test_key (SRCDIR "/gcr-gtk3/fixtures/email.p12");
	}

	return 0;
}
