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
#include "gcr/gcr-base.h"

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

static const gchar *file_name = NULL;
static gchar *prompt_type = NULL;
static gint prompt_delay = 0;
static gboolean prompt_window = FALSE;

static gboolean
on_delay_timeout (gpointer data)
{
	GMainLoop *loop = data;
	g_main_loop_quit (loop);
	return FALSE;
}

#ifdef GDK_WINDOWING_X11
static void
set_caller_window_x11 (GcrPrompt *prompt, GdkWindow *window)
{
	gchar *caller_id = NULL;

	if (!GDK_IS_X11_WINDOW (window))
		return;

	caller_id = g_strdup_printf ("%lu", (gulong)GDK_WINDOW_XID (window));
	gcr_prompt_set_caller_window (prompt, caller_id);
	g_free (caller_id);
}
#endif

#ifdef GDK_WINDOWING_WAYLAND
static void
on_window_wl_export_handle (GdkWindow  *window,
                            const char *handle,
                            gpointer    user_data)
{
	GcrPrompt *prompt = GCR_PROMPT (user_data);

	g_return_if_fail (handle);

	gcr_prompt_set_caller_window (prompt, handle);
}

static void
set_caller_window_wl (GcrPrompt *prompt, GdkWindow *window)
{
	if (!GDK_IS_WAYLAND_WINDOW (window))
		return;

	if (!gdk_wayland_window_export_handle (window, on_window_wl_export_handle, prompt, NULL))
		g_warning ("Couldn't export Wayland window handle");
}
#endif

static void
prompt_perform (GtkWidget *parent)
{
	GKeyFile *file;
	GError *error = NULL;
	gchar **groups, **keys;
	GValue value = { 0, };
	GParamSpec *spec;
	GcrPrompt *prompt;
	const gchar *key;
	const gchar *password;
	GcrPromptReply reply;
	gboolean cont = TRUE;
	GMainLoop *loop;
	gchar *type;
	gchar *choice;
	guint i, j;

	file = g_key_file_new ();
	if (!g_key_file_load_from_file (file, file_name, G_KEY_FILE_NONE, &error))
		g_error ("couldn't load prompt info: %s", error->message);

	if (!prompt_type || g_str_equal (prompt_type, "dialog"))
		prompt = g_object_new (GCR_TYPE_PROMPT_DIALOG, NULL);
	else if (g_str_equal (prompt_type, "system"))
		prompt = gcr_system_prompt_open_for_prompter ("org.gnome.keyring.SystemPrompter", 5, NULL, &error);
	else if (g_str_equal (prompt_type, "private"))
		prompt = gcr_system_prompt_open_for_prompter ("org.gnome.keyring.PrivatePrompter", 5, NULL, &error);
	else
		g_error ("invalid type: %s", prompt_type);

	if (error != NULL)
		g_error ("couldn't create prompt: %s", error->message);

	if (parent) {
		GdkWindow *window;

		window = gtk_widget_get_window (parent);
#ifdef GDK_WINDOWING_X11
		set_caller_window_x11 (prompt, window);
#endif
#ifdef GDK_WINDOWING_WAYLAND
		set_caller_window_wl (prompt, window);
#endif
	}

	loop = g_main_loop_new (NULL, FALSE);
	groups = g_key_file_get_groups (file, NULL);
	for (i = 0; cont && groups[i] != NULL; i++) {

		if (i != 0) {
			g_timeout_add_seconds (prompt_delay, on_delay_timeout, loop);
			g_main_loop_run (loop);
		}

		keys = g_key_file_get_keys (file, groups[i], NULL, NULL);
		for (j = 0; keys[j] != NULL; j++) {
			key = keys[j];
			if (g_str_equal (key, "type"))
				continue;
			spec = g_object_class_find_property (G_OBJECT_GET_CLASS (prompt), key);
			if (spec == NULL)
				g_error ("couldn't find property %s on prompt %s",
				      key, G_OBJECT_TYPE_NAME (prompt));
			g_value_init (&value, spec->value_type);
			switch (spec->value_type) {
			case G_TYPE_STRING:
				g_value_take_string (&value, g_key_file_get_string (file, groups[i], key, NULL));
				break;
			case G_TYPE_INT:
				g_value_set_int (&value, g_key_file_get_integer (file, groups[i], key, NULL));
				break;
			case G_TYPE_BOOLEAN:
				g_value_set_boolean (&value, g_key_file_get_boolean (file, groups[i], key, NULL));
				break;
			default:
				g_error ("unsupported type %s for property %s",
				      g_type_name (spec->value_type), key);
				break;
			}

			g_object_set_property (G_OBJECT (prompt), key, &value);
			g_value_unset (&value);
		}

		g_strfreev (keys);

		type = g_key_file_get_value (file, groups[i], "type", NULL);
		if (g_strcmp0 (type, "password") == 0) {
			password = gcr_prompt_password_run (prompt, NULL, &error);
			if (error != NULL)
				g_error ("couldn't prompt for password: %s", error->message);
			g_print ("prompt password: %s\n", password);
			g_print ("password strength: %d\n", gcr_prompt_get_password_strength (prompt));
			cont = (password != NULL);
		} else if (g_strcmp0 (type, "confirm") == 0) {
			reply = gcr_prompt_confirm_run (prompt, NULL, &error);
			if (error != NULL)
				g_error ("couldn't prompt for confirm: %s", error->message);
			g_print ("prompt confirm: %d\n", reply);
			cont = (reply != GCR_PROMPT_REPLY_CANCEL);
		} else {
			g_error ("unsupported prompt type: %s", type);
		}
		g_free (type);

		choice = gcr_prompt_get_choice_label (prompt);
		if (choice)
			g_print ("choice chosen: %s", gcr_prompt_get_choice_chosen (prompt) ? "true" : "false");
		g_free (choice);
		g_print ("\n");
	}

	g_main_loop_unref (loop);
	g_object_unref (prompt);
	g_strfreev (groups);
	g_key_file_free (file);

}

static void
on_prompt_clicked (GtkToolButton *button,
                   gpointer user_data)
{
	prompt_perform (user_data);
}

static gboolean
on_window_delete (GtkWidget *widget,
                  GdkEvent *event,
                  gpointer user_data)
{
	gtk_main_quit ();
	return FALSE;
}

static GOptionEntry option_entries[] = {
	{ "type", 'c', 0, G_OPTION_ARG_STRING, &prompt_type,
	  "'system', 'private' or 'dialog'", "type" },
	{ "delay", 'd', 0, G_OPTION_ARG_INT, &prompt_delay,
	  "delay in seconds between prompts", "delay" },
	{ "window", 'w', 0, G_OPTION_ARG_NONE, &prompt_window,
	  "prompt with a parent window", NULL },
	{ NULL }
};

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GtkWidget *window;
	GtkToolbar *toolbar;
	GtkToolItem *item;
	GError *error = NULL;

	g_set_prgname ("frob-prompt");

	context = g_option_context_new ("");
	g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (!g_option_context_parse (context, &argc, &argv, &error))
		g_error ("%s", error->message);

	g_option_context_free (context);

	if (argc < 2)
		g_error ("specify file");
	file_name = argv[1];

	if (prompt_window) {
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

	} else {
		prompt_perform (NULL);
	}

	g_free (prompt_type);
	return 0;
}
