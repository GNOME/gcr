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

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include <unistd.h>
#include <string.h>
#include <errno.h>

static void
run_prompt (GcrPrompt *prompt)
{
	const char *password;
	GError *error = NULL;

	password = gcr_prompt_password_run (GCR_PROMPT (prompt), NULL, &error);
	if (error != NULL) {
		g_warning ("couldn't prompt for password: %s", error->message);
		g_error_free (error);
		g_object_unref (prompt);
		return;
	}

	g_print ("password: %s\n", password);
	g_object_unref (prompt);
}

static void
on_gdk_wl_window_exported (GdkWindow *window,
                           const char *handle,
                           gpointer user_data)
{
	GcrPrompt *prompt = GCR_PROMPT (user_data);

	g_return_if_fail (handle);

	gcr_prompt_set_caller_window (prompt, handle);
	run_prompt (prompt);
}

static void
on_prompt_clicked (GtkToolButton *button,
                   gpointer user_data)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	GtkWidget *parent = user_data;
	GdkWindow *window;
	gchar *caller_id;

	prompt = gcr_system_prompt_open (-1, NULL, &error);
	if (error != NULL) {
		g_warning ("couldn't open prompt: %s", error->message);
		g_error_free (error);
		return;
	}

	gcr_prompt_set_title (GCR_PROMPT (prompt), "This is the title");
	gcr_prompt_set_message (GCR_PROMPT (prompt), "This is the message");
	gcr_prompt_set_description (GCR_PROMPT (prompt), "This is the description");

	window = gtk_widget_get_window (parent);
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_WINDOW (window)) {
		caller_id = g_strdup_printf ("%lu", (gulong)GDK_WINDOW_XID (window));
		gcr_prompt_set_caller_window (prompt, caller_id);
		g_free (caller_id);
	}
#endif
#ifdef GDK_WINDOWING_WAYLAND
	if (GDK_IS_WAYLAND_WINDOW (window)) {
		if (!gdk_wayland_window_export_handle (window, on_gdk_wl_window_exported, prompt, NULL)) {
			g_warning ("Couldn't export Wayland window handle");
		} else {
			return; /* Don't run the prompt before the async method finished */
		}
	}
#endif

	run_prompt (prompt);
}

static gboolean
on_window_delete (GtkWidget *widget,
                  GdkEvent *event,
                  gpointer user_data)
{
	gtk_main_quit ();
	return FALSE;
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	GtkToolbar *toolbar;
	GtkToolItem *item;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (window, "delete-event", G_CALLBACK (on_window_delete), NULL);

	toolbar = GTK_TOOLBAR (gtk_toolbar_new ());
	gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_TEXT);
	item = gtk_tool_button_new (NULL, "Prompt");
	g_signal_connect (item, "clicked", G_CALLBACK (on_prompt_clicked), window);
	gtk_toolbar_insert (toolbar, item, 0);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (toolbar));

	gtk_window_set_default_size (GTK_WINDOW (window), 400, 80);
	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
