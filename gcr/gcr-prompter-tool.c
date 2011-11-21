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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr.h"

#include "gcr-dbus-constants.h"
#define DEBUG_FLAG GCR_DEBUG_PROMPT
#include "gcr-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <pango/pango.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#define QUIT_TIMEOUT 10

static GcrSystemPrompter *the_prompter = NULL;

#if 0
static gboolean
on_timeout_quit (gpointer unused)
{
	gtk_main_quit ();
	return FALSE; /* Don't run again */
}

static void
start_timeout (GcrPrompterDialog *self)
{
	if (g_getenv ("GCR_PERSIST") != NULL)
		return;

	if (!self->quit_timeout)
		self->quit_timeout = g_timeout_add_seconds (QUIT_TIMEOUT, on_timeout_quit, NULL);
}

static void
stop_timeout (GcrPrompterDialog *self)
{
	if (self->quit_timeout)
		g_source_remove (self->quit_timeout);
	self->quit_timeout = 0;
}
#endif

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{
	gcr_system_prompter_register (the_prompter, connection);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
	gcr_system_prompter_unregister (the_prompter, FALSE);
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	guint owner_id;

	g_type_init ();
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

	the_prompter = gcr_system_prompter_new (GCR_SYSTEM_PROMPTER_SINGLE,
	                                        GCR_TYPE_PROMPT_DIALOG);
	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
	                           GCR_DBUS_PROMPTER_BUS_NAME,
	                           G_BUS_NAME_OWNER_FLAGS_NONE,
	                           on_bus_acquired,
	                           on_name_acquired,
	                           on_name_lost,
	                           NULL,
	                           NULL);

	gtk_main ();

	g_bus_unown_name (owner_id);
	g_object_unref (the_prompter);

	return 0;
}
