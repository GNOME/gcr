/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
 * Copyright (C) 2007 Nokia Corporation
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "gck.h"
#include "gck-debug.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <p11-kit/pkcs11.h>

#include <sys/types.h>

#include <unistd.h>

#ifdef WITH_DEBUG

static gsize initialized_flags = 0;
static GckDebugFlags current_flags = 0;

static GDebugKey keys[] = {
	{ "session", GCK_DEBUG_SESSION },
	{ "enumerator", GCK_DEBUG_ENUMERATOR },
	{ 0, }
};

static void
debug_set_flags (GckDebugFlags new_flags)
{
	current_flags |= new_flags;
}

void
_gck_debug_set_flags (const gchar *flags_string)
{
	guint nkeys;

	for (nkeys = 0; keys[nkeys].value; nkeys++);

	if (flags_string)
		debug_set_flags (g_parse_debug_string (flags_string, keys, nkeys));
}

static void
on_gck_log_debug (const gchar *log_domain,
                  GLogLevelFlags log_level,
                  const gchar *message,
                  gpointer user_data)
{
	GString *gstring;
	const gchar *progname;

	gstring = g_string_new (NULL);

	progname = g_get_prgname ();
	g_string_append_printf (gstring, "(%s:%lu): %s-DEBUG: %s\n",
	                        progname ? progname : "process",
	                        (gulong)getpid (), log_domain,
	                        message ? message : "(NULL) message");

	/*
	 * Give up on debug messages if stdout got lost.
	 */
	if (write (1, gstring->str, gstring->len) != gstring->len)
		current_flags = 0;

	g_string_free (gstring, TRUE);
}

static void
initialize_debug_flags (void)
{
	const gchar *messages_env;
	const gchar *debug_env;

	if (g_once_init_enter (&initialized_flags)) {
		messages_env = g_getenv ("G_MESSAGES_DEBUG");
		debug_env = g_getenv ("GCK_DEBUG");
#ifdef GCK_DEBUG
		if (debug_env == NULL)
			debug_env = G_STRINGIFY (GCK_DEBUG);
#endif

		/*
		 * If the caller is selectively asking for certain debug
		 * messages with the GCK_DEBUG environment variable, then
		 * we install our own output handler and only print those
		 * messages. This happens irrespective of G_MESSAGES_DEBUG
		 */
		if (messages_env == NULL && debug_env != NULL)
			g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			                   on_gck_log_debug, NULL);

		/*
		 * If the caller is using G_MESSAGES_DEBUG then we enable
		 * all our debug messages, and let Glib filter which ones
		 * to display.
		 */
		if (messages_env != NULL && debug_env == NULL)
			debug_env = "all";

		_gck_debug_set_flags (debug_env);

		g_once_init_leave (&initialized_flags, 1);
	}
}

gboolean
_gck_debug_flag_is_set (GckDebugFlags flag)
{
	if G_UNLIKELY (!initialized_flags)
		initialize_debug_flags ();
	return (flag & current_flags) != 0;
}

void
_gck_debug_message (GckDebugFlags flag,
                    const gchar *format,
                    ...)
{
	va_list args;

	if G_UNLIKELY (!initialized_flags)
		initialize_debug_flags ();

	if (flag & current_flags) {
		va_start (args, format);
		g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
		va_end (args);
	}
}

#else /* !WITH_DEBUG */

gboolean
_gck_debug_flag_is_set (GckDebugFlags flag)
{
	return FALSE;
}

void
_gck_debug_message (GckDebugFlags flag,
                    const gchar *format,
                    ...)
{
}

void
_gck_debug_set_flags (const gchar *flags_string)
{
}

#endif /* !WITH_DEBUG */
