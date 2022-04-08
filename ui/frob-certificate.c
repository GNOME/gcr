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

#include "egg/egg-hex.h"

#include <gtk/gtk.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

static void
on_parser_parsed (GcrParser *parser, gpointer user_data)
{
	GcrCertificateWidget *details;
	GtkDialog *dialog = GTK_DIALOG (user_data);
	GcrCertificate *cert;
	gpointer dn;
	gsize n_dn;
	gchar *string;

	details = g_object_new (GCR_TYPE_CERTIFICATE_WIDGET,
	                        "attributes", gcr_parser_get_parsed_attributes (parser),
	                        NULL);

	cert = gcr_certificate_widget_get_certificate (details);

	dn = gcr_certificate_get_subject_raw (cert, &n_dn);
	string = egg_hex_encode_full (dn, n_dn, TRUE, "\\", 1);
	g_print ("subject: %s\n", string);
	g_free (string);
	g_free (dn);

	dn = gcr_certificate_get_issuer_raw (cert, &n_dn);
	string = egg_hex_encode_full (dn, n_dn, TRUE, "\\", 1);
	g_print ("issuer: %s\n", string);
	g_free (string);
	g_free (dn);

	gtk_widget_show (GTK_WIDGET (details));
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (dialog)), GTK_WIDGET (details));

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 20);
}

static void
test_certificate (const gchar *path)
{
	GcrParser *parser;
	GError *err = NULL;
	guchar *data;
	gsize n_data;
	GBytes *bytes;
	GtkWidget *dialog;

	if (!g_file_get_contents (path, (gchar**)&data, &n_data, NULL))
		g_error ("couldn't read file: %s", path);

	dialog = gtk_dialog_new ();
	g_object_ref_sink (dialog);

	parser = gcr_parser_new ();
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), dialog);
	bytes = g_bytes_new_take (data, n_data);
	if (!gcr_parser_parse_bytes (parser, bytes, &err))
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
	g_set_prgname ("frob-certificate");

	if (argc > 1)
		test_certificate (argv[1]);
	else
		test_certificate (SRCDIR "/ui/fixtures/der-certificate.crt");

	return 0;
}
