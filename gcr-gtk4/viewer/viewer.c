/*
 * Copyright 2011,2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * Copyright Stef Walter <stefw@collabora.co.uk>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gcr-gtk4/gcr-gtk4.h>

#include "config.h"

static gchar **remaining_args = NULL;

static gboolean
print_version_and_exit (const gchar  *option_name,
                        const gchar  *value,
                        gpointer      data,
                        GError      **error)
{
	g_print("%s -- %s\n", _("GCR Certificate and Key Viewer"), VERSION);
	exit (0);
	return TRUE;
}

static const GOptionEntry options[] = {
	{ "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	  print_version_and_exit, N_("Show the application's version"), NULL},
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY,
	  &remaining_args, NULL, N_("[file...]") },
	{ NULL }
};

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *scrolled;
	GCancellable *cancellable = NULL;

	window = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (window), "Window");
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
	scrolled = gtk_scrolled_window_new ();
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	g_object_set (G_OBJECT (scrolled),
		      "child", box,
		      "hscrollbar-policy", GTK_POLICY_NEVER,
		      NULL);

	if (remaining_args) {
		for (int i = 0; remaining_args[i] != NULL; ++i) {
			GFile *file;
			GError *error = NULL;
			GcrCertificate *certificate;
			GtkWidget *widget;

			file = g_file_new_for_commandline_arg (remaining_args[i]);
			certificate = gcr_simple_certificate_new_from_file (file, cancellable, &error);
			g_object_unref (file);
			widget = gcr_certificate_widget_new (GCR_CERTIFICATE (certificate));
			g_object_unref (certificate);
			gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
			gtk_box_append (GTK_BOX (box), widget);
		}

		g_clear_pointer (&remaining_args, g_strfreev);
		remaining_args = NULL;
	}

	gtk_window_set_child (GTK_WINDOW (window), scrolled);
	gtk_widget_show (window);
}

int
main (int    argc,
      char **argv)
{
	GtkApplication *app;
	int status;

	app = gtk_application_new ("org.gnome.GcrViewerGtk4", G_APPLICATION_FLAGS_NONE);
	g_application_add_main_option_entries (G_APPLICATION (app), options);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return status;
}
