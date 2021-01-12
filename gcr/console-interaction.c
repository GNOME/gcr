/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora, Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#ifdef G_OS_WIN32
#include <glib/gprintf.h>
#include <conio.h>
#endif

#include "console-interaction.h"

#include <unistd.h>

/*
 * WARNING: This is not the example you're looking for [slow hand wave]. This
 * is not industrial strength, it's just for testing. It uses embarassing
 * functions like getpass() and does lazy things with threads.
 */

#define TYPE_CONSOLE_INTERACTION         (console_interaction_get_type ())
#define CONSOLE_INTERACTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_CONSOLE_INTERACTION, ConsoleInteraction))
#define CONSOLE_INTERACTION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TYPE_CONSOLE_INTERACTION, ConsoleInteractionClass))
#define IS_CONSOLE_INTERACTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_CONSOLE_INTERACTION))
#define IS_CONSOLE_INTERACTION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_CONSOLE_INTERACTION))
#define CONSOLE_INTERACTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_CONSOLE_INTERACTION, ConsoleInteractionClass))

typedef struct _ConsoleInteraction        ConsoleInteraction;
typedef struct _ConsoleInteractionClass   ConsoleInteractionClass;

struct _ConsoleInteraction
{
  GTlsInteraction parent_instance;
};

struct _ConsoleInteractionClass
{
  GTlsInteractionClass parent_class;
};

GType                  console_interaction_get_type    (void) G_GNUC_CONST;

G_DEFINE_TYPE (ConsoleInteraction, console_interaction, G_TYPE_TLS_INTERACTION);

#ifdef G_OS_WIN32
/* win32 doesn't have getpass() */
static gchar *
getpass (const gchar *prompt)
{
  static gchar buf[BUFSIZ];
  gint i;

  g_printf ("%s", prompt);
  fflush (stdout);

  for (i = 0; i < BUFSIZ - 1; ++i)
    {
      buf[i] = _getch ();
      if (buf[i] == '\r')
        break;
    }
  buf[i] = '\0';

  g_printf ("\n");

  return &buf[0];
}
#endif

static GTlsInteractionResult
console_interaction_ask_password (GTlsInteraction    *interaction,
                                        GTlsPassword       *password,
                                        GCancellable       *cancellable,
                                        GError            **error)
{
  const gchar *value;
  gchar *prompt;

  prompt = g_strdup_printf ("Password \"%s\"': ", g_tls_password_get_description (password));
  value = getpass (prompt);
  g_free (prompt);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return G_TLS_INTERACTION_FAILED;

  g_tls_password_set_value (password, (guchar *)value, -1);
  return G_TLS_INTERACTION_HANDLED;
}

static void
ask_password_with_getpass (GTask        *task,
                           gpointer      src_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  GTlsPassword *password;
  GError *error = NULL;

  password = g_task_get_task_data (task);
  console_interaction_ask_password (G_TLS_INTERACTION (src_object), password,
                                    cancellable, &error);
  if (error == NULL)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static void
console_interaction_ask_password_async (GTlsInteraction    *interaction,
                                        GTlsPassword       *password,
                                        GCancellable       *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer            user_data)
{
  GTask *task;

  task = g_task_new (interaction, cancellable, callback, user_data);
  g_task_set_source_tag (task, console_interaction_ask_password);
  g_task_set_task_data (task, g_object_ref (password), g_object_unref);

  g_task_run_in_thread (task, ask_password_with_getpass);
  g_clear_object (&task);
}

static GTlsInteractionResult
console_interaction_ask_password_finish (GTlsInteraction *interaction,
                                         GAsyncResult    *result,
                                         GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, interaction),
                        G_TLS_INTERACTION_FAILED);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return G_TLS_INTERACTION_FAILED;

  return G_TLS_INTERACTION_HANDLED;
}

static void
console_interaction_init (ConsoleInteraction *interaction)
{

}

static void
console_interaction_class_init (ConsoleInteractionClass *klass)
{
  GTlsInteractionClass *interaction_class = G_TLS_INTERACTION_CLASS (klass);
  interaction_class->ask_password = console_interaction_ask_password;
  interaction_class->ask_password_async = console_interaction_ask_password_async;
  interaction_class->ask_password_finish = console_interaction_ask_password_finish;
}

GTlsInteraction *
console_interaction_new (void)
{
  return g_object_new (TYPE_CONSOLE_INTERACTION, NULL);
}
