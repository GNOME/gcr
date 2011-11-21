/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Stefan Walter
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stef@thewalter.net>
 */

#include "config.h"

#include "gcr-prompt.h"

typedef struct {
	GAsyncResult *result;
	GMainLoop *loop;
	GMainContext *context;
} RunClosure;

typedef GcrPromptIface GcrPromptInterface;

static void   gcr_prompt_default_init    (GcrPromptIface *iface);

G_DEFINE_INTERFACE (GcrPrompt, gcr_prompt, G_TYPE_OBJECT);

static void
gcr_prompt_default_init (GcrPromptIface *iface)
{
	static gsize initialized = 0;

	if (g_once_init_enter (&initialized)) {

		g_object_interface_install_property (iface,
		                g_param_spec_string ("title", "Title", "Prompt title",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                g_param_spec_string ("message", "Message", "Prompt message",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                g_param_spec_string ("description", "Description", "Prompt description",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                g_param_spec_string ("warning", "Warning", "Prompt warning",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		               g_param_spec_boolean ("password-new", "Password new", "Whether prompting for a new password",
		                                     FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                   g_param_spec_int ("password-strength", "Password strength", "String of new password",
		                                     0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                g_param_spec_string ("choice-label", "Choice label", "Label for prompt choice",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		               g_param_spec_boolean ("choice-chosen", "Choice chosen", "Whether prompt choice is chosen",
		                                     FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                g_param_spec_string ("caller-window", "Caller window", "Window ID of application window requesting prompt",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_once_init_leave (&initialized, 1);
	}
}

static void
run_closure_end (gpointer data)
{
	RunClosure *closure = data;
	g_clear_object (&closure->result);
	g_main_loop_unref (closure->loop);
	if (closure->context != NULL) {
		g_main_context_pop_thread_default (closure->context);
		g_main_context_unref (closure->context);
	}
	g_free (closure);
}

static RunClosure *
run_closure_begin (GMainContext *context)
{
	RunClosure *closure = g_new0 (RunClosure, 1);
	closure->loop = g_main_loop_new (context ? context : g_main_context_get_thread_default (), FALSE);
	closure->result = NULL;

	/* We assume ownership of context reference */
	closure->context = context;
	if (closure->context != NULL)
		g_main_context_push_thread_default (closure->context);

	return closure;
}

static void
on_run_complete (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	RunClosure *closure = user_data;
	g_return_if_fail (closure->result == NULL);
	closure->result = g_object_ref (result);
	g_main_loop_quit (closure->loop);
}

gchar *
gcr_prompt_get_title (GcrPrompt *prompt)
{
	gchar *title = NULL;
	g_object_get (prompt, "title", &title, NULL);
	return title;
}

void
gcr_prompt_set_title (GcrPrompt *prompt,
                      const gchar *title)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "title", title, NULL);
}

gchar *
gcr_prompt_get_message (GcrPrompt *prompt)
{
	gchar *message = NULL;
	g_object_get (prompt, "message", &message, NULL);
	return message;
}

void
gcr_prompt_set_message (GcrPrompt *prompt,
                        const gchar *message)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "message", message, NULL);
}

gchar *
gcr_prompt_get_description (GcrPrompt *prompt)
{
	gchar *description = NULL;
	g_object_get (prompt, "description", &description, NULL);
	return description;
}

void
gcr_prompt_set_description (GcrPrompt *prompt,
                            const gchar *description)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "description", description, NULL);
}

gchar *
gcr_prompt_get_warning (GcrPrompt *prompt)
{
	gchar *warning = NULL;
	g_object_get (prompt, "warning", &warning, NULL);
	return warning;
}

void
gcr_prompt_set_warning (GcrPrompt *prompt,
                        const gchar *warning)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "warning", warning, NULL);
}

gchar *
gcr_prompt_get_choice_label (GcrPrompt *prompt)
{
	gchar *choice_label = NULL;
	g_object_get (prompt, "choice-label", &choice_label, NULL);
	return choice_label;
}

void
gcr_prompt_set_choice_label (GcrPrompt *prompt,
                             const gchar *choice_label)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "choice-label", choice_label, NULL);
}

gboolean
gcr_prompt_get_choice_chosen (GcrPrompt *prompt)
{
	gboolean choice_chosen;
	g_object_get (prompt, "choice-chosen", &choice_chosen, NULL);
	return choice_chosen;
}

void
gcr_prompt_set_choice_chosen (GcrPrompt *prompt,
                              gboolean chosen)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "choice-chosen", chosen, NULL);
}

gboolean
gcr_prompt_get_password_new (GcrPrompt *prompt)
{
	gboolean password_new;
	g_object_get (prompt, "password-new", &password_new, NULL);
	return password_new;
}

void
gcr_prompt_set_password_new (GcrPrompt *prompt,
                             gboolean new_password)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "password-new", new_password, NULL);
}

gint
gcr_prompt_get_password_strength (GcrPrompt *prompt)
{
	gboolean password_strength;
	g_object_get (prompt, "password-strength", &password_strength, NULL);
	return password_strength;
}

gchar *
gcr_prompt_get_caller_window (GcrPrompt *prompt)
{
	gchar *caller_window = NULL;
	g_object_get (prompt, "caller-window", &caller_window, NULL);
	return caller_window;
}

void
gcr_prompt_set_caller_window (GcrPrompt *prompt,
                              const gchar *window_id)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "caller-window", window_id, NULL);
}

void
gcr_prompt_password_async (GcrPrompt *prompt,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	GcrPromptIface *iface;

	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCR_PROMPT_GET_INTERFACE (prompt);
	g_return_if_fail (iface->prompt_password_async);

	(iface->prompt_password_async) (prompt, cancellable, callback, user_data);
}

const gchar *
gcr_prompt_password_finish (GcrPrompt *prompt,
                            GAsyncResult *result,
                            GError **error)
{
	GcrPromptIface *iface;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	iface = GCR_PROMPT_GET_INTERFACE (prompt);
	g_return_val_if_fail (iface->prompt_password_async, NULL);

	return (iface->prompt_password_finish) (prompt, result, error);
}

const gchar *
gcr_prompt_password (GcrPrompt *prompt,
                     GCancellable *cancellable,
                     GError **error)
{
	RunClosure *closure;
	const gchar *reply;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	closure = run_closure_begin (g_main_context_new ());

	gcr_prompt_password_async (prompt, cancellable, on_run_complete, closure);

	g_main_loop_run (closure->loop);

	reply = gcr_prompt_password_finish (prompt, closure->result, error);
	run_closure_end (closure);

	return reply;
}

const gchar *
gcr_prompt_password_run (GcrPrompt *prompt,
                         GCancellable *cancellable,
                         GError **error)
{
	RunClosure *closure;
	const gchar *reply;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	closure = run_closure_begin (NULL);

	gcr_prompt_password_async (prompt, cancellable, on_run_complete, closure);

	g_main_loop_run (closure->loop);

	reply = gcr_prompt_password_finish (prompt, closure->result, error);
	run_closure_end (closure);

	return reply;
}

void
gcr_prompt_confirm_async (GcrPrompt *prompt,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GcrPromptIface *iface;

	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCR_PROMPT_GET_INTERFACE (prompt);
	g_return_if_fail (iface->prompt_confirm_async);

	(iface->prompt_confirm_async) (prompt, cancellable, callback, user_data);
}

GcrPromptReply
gcr_prompt_confirm_finish (GcrPrompt *prompt,
                           GAsyncResult *result,
                           GError **error)
{
	GcrPromptIface *iface;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (error == NULL || *error == NULL, GCR_PROMPT_REPLY_CANCEL);

	iface = GCR_PROMPT_GET_INTERFACE (prompt);
	g_return_val_if_fail (iface->prompt_confirm_async, GCR_PROMPT_REPLY_CANCEL);

	return (iface->prompt_confirm_finish) (prompt, result, error);
}

GcrPromptReply
gcr_prompt_confirm (GcrPrompt *prompt,
                    GCancellable *cancellable,
                    GError **error)
{
	RunClosure *closure;
	GcrPromptReply reply;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (error == NULL || *error == NULL, GCR_PROMPT_REPLY_CANCEL);

	closure = run_closure_begin (g_main_context_new ());

	gcr_prompt_confirm_async (prompt, cancellable, on_run_complete, closure);

	g_main_loop_run (closure->loop);

	reply = gcr_prompt_confirm_finish (prompt, closure->result, error);
	run_closure_end (closure);

	return reply;
}

/**
 * gcr_system_prompt_confirm:
 * @self: a prompt
 * @cancellable: optional cancellation object
 * @error: location to place error on failure
 *
 * Prompts for confirmation asking a cancel/continue style question.
 * Set the various properties on the prompt to represent the question
 * correctly.
 *
 * This method will block until the a response is returned from the prompter.
 * and %TRUE or %FALSE will be returned based on the response. The return value
 * will also be %FALSE if an error occurs. Check the @error argument to tell
 * the difference.
 *
 * Returns: whether the prompt was confirmed or not
 */
GcrPromptReply
gcr_prompt_confirm_run (GcrPrompt *prompt,
                        GCancellable *cancellable,
                        GError **error)
{
	RunClosure *closure;
	GcrPromptReply reply;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (error == NULL || *error == NULL, GCR_PROMPT_REPLY_CANCEL);

	closure = run_closure_begin (NULL);

	gcr_prompt_confirm_async (prompt, cancellable, on_run_complete, closure);

	g_main_loop_run (closure->loop);

	reply = gcr_prompt_confirm_finish (prompt, closure->result, error);
	run_closure_end (closure);

	return reply;
}
