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

#include "console-interaction.h"

#include "gcr/gcr.h"

#include "egg/egg-armor.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

const gchar *cn_name = NULL;

static GckObject *
load_key_for_uri (const gchar *uri)
{
	GError *error = NULL;
	GTlsInteraction *interaction;
	GckEnumerator *enumerator;
	GList *modules;
	GckObject *key;

	gcr_pkcs11_initialize (NULL, &error);
	g_assert_no_error (error);

	modules = gcr_pkcs11_get_modules ();
	enumerator = gck_modules_enumerate_uri (modules, uri, GCK_SESSION_LOGIN_USER |
	                                        GCK_SESSION_READ_ONLY, &error);
	g_clear_list (&modules, g_object_unref);

	interaction = console_interaction_new ();
	gck_enumerator_set_interaction (enumerator, interaction);
	g_object_unref (interaction);

	key = gck_enumerator_next (enumerator, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (enumerator);

	return key;
}

static void
test_request (const gchar *uri)
{
	GcrCertificateRequest *req;
	GError *error = NULL;
	GckObject *key;
	guchar *data;
	gsize n_data;

	key = load_key_for_uri (uri);
	if (key == NULL)
		g_error ("couldn't find key for uri: %s", uri);

	if (!gcr_certificate_request_capable (key, NULL, &error)) {
		if (error != NULL)
			g_error ("error checking key capabilities: %s", error->message);
		g_clear_error (&error);
		g_printerr ("frob-certificate-request: key doesn't have right capabilities");
		g_object_unref (key);
		return;
	}

	req = gcr_certificate_request_prepare (GCR_CERTIFICATE_REQUEST_PKCS10, key);
	g_object_unref (key);

	gcr_certificate_request_set_cn (req, cn_name);
	gcr_certificate_request_complete (req, NULL, &error);
	g_assert_no_error (error);

	data = gcr_certificate_request_encode (req, TRUE, &n_data);

	if (write (1, data, n_data) < 0)
		g_error ("failed to write: %s", g_strerror (errno));
	g_free (data);
}

int
main(int argc, char *argv[])
{
	g_set_prgname ("frob-certificate-request");

	if (argc <= 1)
		g_printerr ("frob-certificate-request: specify pkcs11: url of key");

	if (cn_name == NULL)
		cn_name = g_strdup ("name.example.com");

	test_request (argv[1]);
	return 0;
}
