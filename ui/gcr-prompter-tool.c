/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-viewer-tool.c: Command line utility

   Copyright (C) 2011 Collabora Ltd.

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

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr/gcr.h"
#include "ui/gcr-ui.h"
#include "gcr/gcr-dbus-constants.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define QUIT_TIMEOUT 10

static GcrSystemPrompter *the_prompter = NULL;
static gboolean registered_prompter = FALSE;
static gboolean acquired_system_prompter = FALSE;
static gboolean acquired_private_prompter = FALSE;
static guint timeout_source = 0;

static gboolean
on_timeout_quit (gpointer unused)
{
	g_debug ("%d second inactivity timeout, quitting", QUIT_TIMEOUT);
	gtk_main_quit ();

	return FALSE; /* Don't run again */
}

static void
start_timeout (void)
{
	if (g_getenv ("GCR_PERSIST") != NULL)
		return;

	if (!timeout_source)
		timeout_source = g_timeout_add_seconds (QUIT_TIMEOUT, on_timeout_quit, NULL);
}

static void
stop_timeout (void)
{
	if (timeout_source)
		g_source_remove (timeout_source);
	timeout_source = 0;
}

static void
on_prompter_prompting (GObject *obj,
                       GParamSpec *param,
                       gpointer user_data)
{
	if (gcr_system_prompter_get_prompting (the_prompter))
		stop_timeout ();
	else
		start_timeout ();
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
	g_debug ("bus acquired: %s", name);

	if (!registered_prompter)
		gcr_system_prompter_register (the_prompter, connection);

	registered_prompter = TRUE;
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{
	g_debug ("acquired name: %s", name);

	if (g_strcmp0 (name, GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME) == 0)
		acquired_system_prompter = TRUE;

	else if (g_strcmp0 (name, GCR_DBUS_PROMPTER_PRIVATE_BUS_NAME) == 0)
		acquired_private_prompter = TRUE;
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
	g_debug ("lost name: %s", name);

	/* Called like so when no connection can be made */
	if (connection == NULL) {
		g_warning ("couldn't connect to session bus");
		gtk_main_quit ();

	} else if (g_strcmp0 (name, GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME) == 0) {
		acquired_system_prompter = FALSE;

	} else if (g_strcmp0 (name, GCR_DBUS_PROMPTER_PRIVATE_BUS_NAME) == 0) {
		acquired_private_prompter = FALSE;

	}
}

static void
log_handler (const gchar *log_domain,
             GLogLevelFlags log_level,
             const gchar *message,
             gpointer user_data)
{
	int level;

	/* Note that crit and err are the other way around in syslog */

	switch (G_LOG_LEVEL_MASK & log_level) {
	case G_LOG_LEVEL_ERROR:
		level = LOG_CRIT;
		break;
	case G_LOG_LEVEL_CRITICAL:
		level = LOG_ERR;
		break;
	case G_LOG_LEVEL_WARNING:
		level = LOG_WARNING;
		break;
	case G_LOG_LEVEL_MESSAGE:
		level = LOG_NOTICE;
		break;
	case G_LOG_LEVEL_INFO:
		level = LOG_INFO;
		break;
	case G_LOG_LEVEL_DEBUG:
		level = LOG_DEBUG;
		break;
	default:
		level = LOG_ERR;
		break;
	}

	/* Log to syslog first */
	if (log_domain)
		syslog (level, "%s: %s", log_domain, message);
	else
		syslog (level, "%s", message);

	/* And then to default handler for aborting and stuff like that */
	g_log_default_handler (log_domain, log_level, message, user_data);
}

static void
printerr_handler (const gchar *string)
{
	/* Print to syslog and stderr */
	syslog (LOG_WARNING, "%s", string);
	fprintf (stderr, "%s", string);
}

static void
prepare_logging ()
{
	GLogLevelFlags flags = G_LOG_FLAG_FATAL | G_LOG_LEVEL_ERROR |
	                       G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING |
	                       G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO;

	openlog ("gcr-prompter", LOG_PID, LOG_AUTH);

	g_log_set_handler (NULL, flags, log_handler, NULL);
	g_log_set_handler ("Glib", flags, log_handler, NULL);
	g_log_set_handler ("Gtk", flags, log_handler, NULL);
	g_log_set_handler ("Gnome", flags, log_handler, NULL);
	g_log_set_handler ("Gcr", flags, log_handler, NULL);
	g_log_set_handler ("Gck", flags, log_handler, NULL);
	g_log_set_default_handler (log_handler, NULL);
	g_set_printerr_handler (printerr_handler);
}

int
main (int argc, char *argv[])
{
	guint system_owner_id;
	guint private_owner_id;

	gtk_init (&argc, &argv);

#ifdef HAVE_LOCALE_H
	/* internationalisation */
	setlocale (LC_ALL, "");
#endif

#ifdef HAVE_GETTEXT
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

	prepare_logging ();

	the_prompter = gcr_system_prompter_new (GCR_SYSTEM_PROMPTER_SINGLE,
	                                        GCR_TYPE_PROMPT_DIALOG);
	g_signal_connect (the_prompter, "notify::prompting",
	                  G_CALLBACK (on_prompter_prompting), NULL);

	system_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                  GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME,
	                                  G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                  on_bus_acquired,
	                                  on_name_acquired,
	                                  on_name_lost,
	                                  NULL,
	                                  NULL);

	private_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                   GCR_DBUS_PROMPTER_PRIVATE_BUS_NAME,
	                                   G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                   on_bus_acquired,
	                                   on_name_acquired,
	                                   on_name_lost,
	                                   NULL,
	                                   NULL);

	start_timeout ();
	gtk_main ();

	if (registered_prompter)
		gcr_system_prompter_unregister (the_prompter, TRUE);

	g_bus_unown_name (system_owner_id);
	g_bus_unown_name (private_owner_id);

	g_object_unref (the_prompter);

	return 0;
}
