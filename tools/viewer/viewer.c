/*
 * Copyright 2011,2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * Copyright Stef Walter <stefw@collabora.co.uk>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "config.h"
#include "gcr-certificate-widget.h"

static gchar **remaining_args = NULL;

static gboolean
print_version_and_exit (const gchar  *option_name,
                        const gchar  *value,
                        gpointer      data,
                        GError      **error)
{
	g_print("%s -- %s\n", _("GCR Certificate Viewer"), VERSION);
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
on_parser_parsed (GcrParser *parser,
                  gpointer user_data)
{
	GcrCertificate **cert = user_data;
	GckAttributes *attributes;
	const GckAttribute *attr;

	attributes = gcr_parser_get_parsed_attributes (parser);
	attr = gck_attributes_find (attributes, CKA_VALUE);
	*cert = gcr_simple_certificate_new (attr->value, attr->length);
}

static GcrCertificate *
simple_certificate_new_from_file (GFile         *file,
                                  GCancellable  *cancellable,
                                  GError       **error)
{
	GcrCertificate *cert = NULL;
	GcrParser *parser;
	GBytes *bytes;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (!error || !*error, NULL);


	bytes = g_file_load_bytes (file, cancellable, NULL, error);
	if (!bytes) {
		return NULL;
	}

	parser = gcr_parser_new ();
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), &cert);
	if (!gcr_parser_parse_bytes (parser, bytes, error)) {
		g_bytes_unref (bytes);
		g_object_unref (parser);
		g_object_unref (cert);
		return NULL;
	}

	g_bytes_unref (bytes);
	g_object_unref (parser);
	return cert;
}

static GcrCertificate *
simple_certificate_new_from_commandline_arg (const gchar   *arg,
                                             GCancellable  *cancellable,
                                             GError       **error)
{
	GcrCertificate *cert;

	if (g_str_has_prefix (arg, "pkcs11:")) {
		cert = gcr_pkcs11_certificate_new_from_uri (arg, cancellable, error);
	} else {
		GFile *file;

		file = g_file_new_for_commandline_arg (arg);
		cert = simple_certificate_new_from_file (file, cancellable, error);
		g_object_unref (file);
	}

	return cert;
}


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
			GError *error = NULL;
			GcrCertificate *certificate;
			GtkWidget *widget;

			certificate = simple_certificate_new_from_commandline_arg (remaining_args[i], cancellable, &error);
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
