/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
   Copyright (C) 2014 Stefan Walter

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@gnome.org>
*/

#include "config.h"

#include "gcr/gcr.h"

#include "egg/egg-testing.h"
#include "egg/mock-interaction.h"

#include <glib/gstdio.h>

extern const char *gcr_ssh_askpass_executable;

typedef struct {
	GTlsInteraction *interaction;
	GcrSshAskpass *askpass;
} Test;

static void
setup (Test *test,
       gconstpointer password)
{
	test->interaction = mock_interaction_new (password);
	test->askpass = gcr_ssh_askpass_new (test->interaction);
}

static void
teardown (Test *test,
          gconstpointer unused)
{
	g_object_unref (test->interaction);

	g_object_add_weak_pointer (G_OBJECT (test->askpass), (gpointer *)&test->askpass);
	g_object_unref (test->askpass);
	g_assert (test->askpass == NULL);
}

static void
on_ssh_child (GPid pid,
              gint status,
              gpointer user_data)
{
	gint *out = user_data;
	*out = status;
}

static void
test_ssh_keygen (Test *test,
                 gconstpointer password)
{

	GError *error = NULL;
	gint status;
	GPid pid;

	gchar *filename = g_build_filename (SRCDIR, "gcr", "fixtures", "pem-rsa-enc.key", NULL);
	gchar *argv[] = { "ssh-keygen", "-y", "-f", filename, NULL };

	g_assert_cmpstr (password, ==, "booo");

	if (g_chmod (filename, 0600) < 0)
		g_assert_not_reached ();

	g_spawn_async (SRCDIR "/gcr/fixtures", argv, NULL,
	               G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CLOEXEC_PIPES,
	               gcr_ssh_askpass_child_setup, test->askpass, &pid, &error);

	g_free (filename);

	if (g_error_matches (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT)) {
		g_test_skip ("ssh-keygen not found");
		return;
	}

	g_assert_no_error (error);

	status = -1;
	g_child_watch_add (pid, on_ssh_child, &status);

	while (status == -1)
		g_main_context_iteration (NULL, TRUE);

	g_spawn_check_exit_status (status, &error);
	g_assert_cmpint (status, ==, 0);
	g_assert_no_error (error);
}

static void
test_cancelled (Test *test,
                gconstpointer password)
{

	GError *error = NULL;
	gint status;
	GPid pid;

	gchar *filename = g_build_filename (SRCDIR, "gcr", "fixtures", "pem-rsa-enc.key", NULL);
	gchar *argv[] = { "ssh-keygen", "-y", "-f", filename, NULL };

	g_assert_cmpstr (password, ==, NULL);

	if (g_chmod (filename, 0600) < 0)
		g_assert_not_reached ();

	g_spawn_async (SRCDIR "/gcr/fixtures", argv, NULL,
	               G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CLOEXEC_PIPES,
	               gcr_ssh_askpass_child_setup, test->askpass, &pid, &error);

	g_free (filename);

	if (g_error_matches (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT)) {
		g_test_skip ("ssh-keygen not found");
		return;
	}

	g_assert_no_error (error);

	status = -1;
	g_child_watch_add (pid, on_ssh_child, &status);

	while (status == -1)
		g_main_context_iteration (NULL, TRUE);

	g_assert_cmpint (status, !=, 0);
	g_assert_no_error (error);
}

static void
test_properties (Test *test,
                 gconstpointer unused)
{
	GTlsInteraction *interaction;

	g_object_get (test->askpass, "interaction", &interaction, NULL);
	g_assert (interaction == test->interaction);
	g_object_unref (interaction);

	g_assert (gcr_ssh_askpass_get_interaction (test->askpass) == test->interaction);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-ssh-askpass");

	gcr_ssh_askpass_executable = _GCR_TEST_SSH_ASKPASS_PATH;

	g_test_add ("/gcr/ssh-askpass/ssh-keygen", Test, "booo", setup, test_ssh_keygen, teardown);
	g_test_add ("/gcr/ssh-askpass/cancelled", Test, NULL, setup, test_cancelled, teardown);
	g_test_add ("/gcr/ssh-askpass/properties", Test, NULL, setup, test_properties, teardown);

	return g_test_run ();
}
