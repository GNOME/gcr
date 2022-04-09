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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stef@thewalter.net>
 */

#include "config.h"

#include "gcr-prompt.h"

#include <glib/gi18n-lib.h>

/**
 * GcrPrompt:
 *
 * A prompt displayed to the user. It is an interface with various
 * implementations.
 *
 * Various properties are set on the prompt, and then the prompt is displayed
 * the various prompt methods like [method@Prompt.password_run].
 *
 * A `GcrPrompt` may be used to display multiple related prompts. Most
 * implementions do not hide the window between display of multiple related
 * prompts, and the #GcrPrompt must be closed or destroyed in order to make
 * it go away. This allows the user to see that the prompts are related.
 *
 * Use `GcrPromptDialog` (part of gcr-ui) to create an in-process GTK+ dialog
 * prompt. Use [class@SystemPrompt] to create a system prompt in a prompter
 * process.
 *
 * The prompt implementation will always display the [property@Prompt:message]
 * property, but may choose not to display the [property@Prompt:description] or
 * [property@Prompt:title] properties.
 */

/**
 * GcrPromptInterface:
 * @parent_iface: parent interface
 * @prompt_password_async: begin a password prompt
 * @prompt_password_finish: complete a password prompt
 * @prompt_confirm_async: begin a confirm prompt
 * @prompt_confirm_finish: complete a confirm prompt
 * @prompt_close: close a prompt
 *
 * The interface for implementing [iface@Prompt].
 */

/**
 * GcrPromptReply:
 * @GCR_PROMPT_REPLY_CONTINUE: the user replied with 'ok'
 * @GCR_PROMPT_REPLY_CANCEL: the prompt was cancelled
 *
 * Various replies returned by [method@Prompt.confirm] and friends.
 */

enum {
	PROMPT_CLOSE,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

typedef struct {
	GAsyncResult *result;
	GMainLoop *loop;
	GMainContext *context;
} RunClosure;

static void   gcr_prompt_default_init    (GcrPromptInterface *iface);

G_DEFINE_INTERFACE (GcrPrompt, gcr_prompt, G_TYPE_OBJECT);

static void
gcr_prompt_default_init (GcrPromptInterface *iface)
{
	static gsize initialized = 0;

	if (g_once_init_enter (&initialized)) {

		/**
		 * GcrPrompt:title:
		 *
		 * The title of the prompt.
		 *
		 * A prompt implementation may choose not to display the prompt title. The
		 * #GcrPrompt:message should contain relevant information.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("title", "Title", "Prompt title",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:message:
		 *
		 * The prompt message for the user.
		 *
		 * A prompt implementation should always display this message.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("message", "Message", "Prompt message",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:description:
		 *
		 * The detailed description of the prompt.
		 *
		 * A prompt implementation may choose not to display this detailed description.
		 * The prompt message should contain relevant information.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("description", "Description", "Prompt description",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:warning:
		 *
		 * A prompt warning displayed on the prompt, or %NULL for no warning.
		 *
		 * This is a warning like "The password is incorrect." usually displayed to the
		 * user about a previous 'unsuccessful' prompt.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("warning", "Warning", "Prompt warning",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:password-new:
		 *
		 * Whether the prompt will prompt for a new password.
		 *
		 * This will cause the prompt implementation to ask the user to confirm the
		 * password and/or display other relevant user interface for creating a new
		 * password.
		 */
		g_object_interface_install_property (iface,
		               g_param_spec_boolean ("password-new", "Password new", "Whether prompting for a new password",
		                                     FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:password-strength:
		 *
		 * Indication of the password strength.
		 *
		 * Prompts will return a zero value if the password is empty, and a value
		 * greater than zero if the password has any characters.
		 *
		 * This is only valid after a successful prompt for a password.
		 */
		g_object_interface_install_property (iface,
		                   g_param_spec_int ("password-strength", "Password strength", "String of new password",
		                                     0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:choice-label:
		 *
		 * The label for the additional choice.
		 *
		 * If this is a non-%NULL value then an additional boolean choice will be
		 * displayed by the prompt allowing the user to select or deselect it.
		 *
		 * If %NULL, then no additional choice is displayed.
		 *
		 * The initial value of the choice can be set with #GcrPrompt:choice-chosen.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("choice-label", "Choice label", "Label for prompt choice",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:choice-chosen:
		 *
		 * Whether the additional choice is chosen or not.
		 *
		 * The additional choice would have been setup using #GcrPrompt:choice-label.
		 */
		g_object_interface_install_property (iface,
		               g_param_spec_boolean ("choice-chosen", "Choice chosen", "Whether prompt choice is chosen",
		                                     FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:caller-window:
		 *
		 * The string handle of the caller's window.
		 *
		 * The caller window indicates to the prompt which window is prompting the
		 * user. The prompt may choose to ignore this information or use it in whatever
		 * way it sees fit.
		 *
		 * In X11, this will be a stringified version of the XWindow handle; in
		 * Wayland this is the result of an export using the XDG foreign
		 * protocol.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("caller-window", "Caller window", "Window ID of application window requesting prompt",
		                                     NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:continue-label:
		 *
		 * The label for the continue button in the prompt.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("continue-label", "Continue label", "Continue button label",
		                                     _("Continue"), G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt:cancel-label:
		 *
		 * The label for the cancel button in the prompt.
		 */
		g_object_interface_install_property (iface,
		                g_param_spec_string ("cancel-label", "Cancel label", "Cancel button label",
		                                     _("Cancel"), G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrPrompt::prompt-close:
		 *
		 * Action signal fired when the prompt is to be closed. After the default
		 * handler has run, the prompt is closed. The various prompting methods
		 * will return results as if the user dismissed the prompt.
		 *
		 * You can use the [method@Prompt.close] method to emit this signal.
		 */
		signals[PROMPT_CLOSE] = g_signal_new ("prompt-close", GCR_TYPE_PROMPT, G_SIGNAL_RUN_FIRST,
		                                      G_STRUCT_OFFSET (GcrPromptInterface, prompt_close),
		                                      NULL, NULL, NULL,
		                                      G_TYPE_NONE, 0);

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
	closure->loop = g_main_loop_new (context, FALSE);
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

/**
 * gcr_prompt_reset:
 * @prompt: the prompt
 *
 * Reset the contents and properties of the prompt.
 */
void
gcr_prompt_reset (GcrPrompt *prompt)
{
	GParamSpec **params;
	GcrPromptInterface *iface;
	guint i, n_params;

	g_return_if_fail (GCR_IS_PROMPT (prompt));

	iface = GCR_PROMPT_GET_IFACE (prompt);
	params = g_object_interface_list_properties (iface, &n_params);

	g_object_freeze_notify (G_OBJECT (prompt));

	for (i = 0; i < n_params; i++) {
		if (!(params[i]->flags & G_PARAM_WRITABLE))
			continue;

		if (params[i]->value_type == G_TYPE_STRING)
			g_object_set (prompt, params[i]->name,
			              ((GParamSpecString *)params[i])->default_value,
			              NULL);

		else if (params[i]->value_type == G_TYPE_INT)
			g_object_set (prompt, params[i]->name,
			              ((GParamSpecInt *)params[i])->default_value,
			              NULL);

		else if (params[i]->value_type == G_TYPE_BOOLEAN)
			g_object_set (prompt, params[i]->name,
			              ((GParamSpecBoolean *)params[i])->default_value,
			              NULL);

		else
			g_assert_not_reached ();
	}

	g_free (params);

	g_object_thaw_notify (G_OBJECT (prompt));
}

/**
 * gcr_prompt_get_title:
 * @prompt: the prompt
 *
 * Gets the title of the prompt.
 *
 * A prompt implementation may choose not to display the prompt title. The
 * prompt message should contain relevant information.
 *
 * Returns: (transfer full): a newly allocated string containing the prompt
 *          title.
 */
gchar *
gcr_prompt_get_title (GcrPrompt *prompt)
{
	gchar *title = NULL;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_object_get (prompt, "title", &title, NULL);
	return title;
}

/**
 * gcr_prompt_set_title:
 * @prompt: the prompt
 * @title: the prompt title
 *
 * Sets the title of the prompt.
 *
 * A prompt implementation may choose not to display the prompt title. The
 * prompt message should contain relevant information.
 */
void
gcr_prompt_set_title (GcrPrompt *prompt,
                      const gchar *title)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "title", title, NULL);
}

/**
 * gcr_prompt_get_message:
 * @prompt: the prompt
 *
 * Gets the prompt message for the user.
 *
 * A prompt implementation should always display this message.
 *
 * Returns: (transfer full): a newly allocated string containing the detailed
 *          description of the prompt
 */
gchar *
gcr_prompt_get_message (GcrPrompt *prompt)
{
	gchar *message = NULL;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_object_get (prompt, "message", &message, NULL);
	return message;
}

/**
 * gcr_prompt_set_message:
 * @prompt: the prompt
 * @message: the prompt message
 *
 * Sets the prompt message for the user.
 *
 * A prompt implementation should always display this message.
 */
void
gcr_prompt_set_message (GcrPrompt *prompt,
                        const gchar *message)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "message", message, NULL);
}

/**
 * gcr_prompt_get_description:
 * @prompt: the prompt
 *
 * Get the detailed description of the prompt.
 *
 * A prompt implementation may choose not to display this detailed description.
 * The prompt message should contain relevant information.
 *
 * Returns: (transfer full): a newly allocated string containing the detailed
 *          description of the prompt
 */
gchar *
gcr_prompt_get_description (GcrPrompt *prompt)
{
	gchar *description = NULL;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_object_get (prompt, "description", &description, NULL);
	return description;
}

/**
 * gcr_prompt_set_description:
 * @prompt: the prompt
 * @description: the detailed description
 *
 * Set the detailed description of the prompt.
 *
 * A prompt implementation may choose not to display this detailed description.
 * Use gcr_prompt_set_message() to set a general message containing relevant
 * information.
 */
void
gcr_prompt_set_description (GcrPrompt *prompt,
                            const gchar *description)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "description", description, NULL);
}

/**
 * gcr_prompt_get_warning:
 * @prompt: the prompt
 *
 * Get a prompt warning displayed on the prompt.
 *
 * This is a warning like "The password is incorrect." usually displayed to the
 * user about a previous 'unsuccessful' prompt.
 *
 * If this string is %NULL then no warning is displayed.
 *
 * Returns: (transfer full): a newly allocated string containing the prompt
 *          warning, or %NULL if no warning
 */
gchar *
gcr_prompt_get_warning (GcrPrompt *prompt)
{
	gchar *warning = NULL;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_object_get (prompt, "warning", &warning, NULL);
	return warning;
}

/**
 * gcr_prompt_set_warning:
 * @prompt: the prompt
 * @warning: (nullable): the warning or %NULL
 *
 * Set a prompt warning displayed on the prompt.
 *
 * This is a warning like "The password is incorrect." usually displayed to the
 * user about a previous 'unsuccessful' prompt.
 *
 * If this string is %NULL then no warning is displayed.
 */
void
gcr_prompt_set_warning (GcrPrompt *prompt,
                        const gchar *warning)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "warning", warning, NULL);
}

/**
 * gcr_prompt_get_choice_label:
 * @prompt: the prompt
 *
 * Get the label for the additional choice.
 *
 * This will be %NULL if no additional choice is being displayed.
 *
 * Returns: (transfer full): a newly allocated string containing the additional
 *          choice or %NULL
 */
gchar *
gcr_prompt_get_choice_label (GcrPrompt *prompt)
{
	gchar *choice_label = NULL;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_object_get (prompt, "choice-label", &choice_label, NULL);
	return choice_label;
}

/**
 * gcr_prompt_set_choice_label:
 * @prompt: the prompt
 * @choice_label: (nullable): the additional choice or %NULL
 *
 * Set the label for the additional choice.
 *
 * If this is a non-%NULL value then an additional boolean choice will be
 * displayed by the prompt allowing the user to select or deselect it.
 *
 * The initial value of the choice can be set with the
 * gcr_prompt_set_choice_label() method.
 *
 * If this is %NULL, then no additional choice is being displayed.
 */
void
gcr_prompt_set_choice_label (GcrPrompt *prompt,
                             const gchar *choice_label)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "choice-label", choice_label, NULL);
}

/**
 * gcr_prompt_get_choice_chosen:
 * @prompt: the prompt
 *
 * Get whether the additional choice was chosen or not.
 *
 * The additional choice would have been setup using
 * gcr_prompt_set_choice_label().
 *
 * Returns: whether chosen
 */
gboolean
gcr_prompt_get_choice_chosen (GcrPrompt *prompt)
{
	gboolean choice_chosen;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), FALSE);
	g_object_get (prompt, "choice-chosen", &choice_chosen, NULL);
	return choice_chosen;
}

/**
 * gcr_prompt_set_choice_chosen:
 * @prompt: the prompt
 * @chosen: whether chosen
 *
 * Set whether the additional choice is chosen or not.
 *
 * The additional choice should be set up using gcr_prompt_set_choice_label().
 */
void
gcr_prompt_set_choice_chosen (GcrPrompt *prompt,
                              gboolean chosen)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "choice-chosen", chosen, NULL);
}

/**
 * gcr_prompt_get_password_new:
 * @prompt: the prompt
 *
 * Get whether the prompt will prompt for a new password.
 *
 * This will cause the prompt implementation to ask the user to confirm the
 * password and/or display other relevant user interface for creating a new
 * password.
 *
 * Returns: whether in new password mode or not
 */
gboolean
gcr_prompt_get_password_new (GcrPrompt *prompt)
{
	gboolean password_new;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), FALSE);
	g_object_get (prompt, "password-new", &password_new, NULL);
	return password_new;
}

/**
 * gcr_prompt_set_password_new:
 * @prompt: the prompt
 * @new_password: whether in new password mode or not
 *
 * Set whether the prompt will prompt for a new password.
 *
 * This will cause the prompt implementation to ask the user to confirm the
 * password and/or display other relevant user interface for creating a new
 * password.
 */
void
gcr_prompt_set_password_new (GcrPrompt *prompt,
                             gboolean new_password)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "password-new", new_password, NULL);
}

/**
 * gcr_prompt_get_password_strength:
 * @prompt: the prompt
 *
 * Get indication of the password strength.
 *
 * Prompts will return a zero value if the password is empty, and a value
 * greater than zero if the password has any characters.
 *
 * This is only valid after a successful prompt for a password.
 *
 * Returns: zero if the password is empty, greater than zero if not
 */
gint
gcr_prompt_get_password_strength (GcrPrompt *prompt)
{
	gboolean password_strength;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), 0);
	g_object_get (prompt, "password-strength", &password_strength, NULL);
	return password_strength;
}

/**
 * gcr_prompt_get_caller_window:
 * @prompt: the prompt
 *
 * Get the string handle of the caller's window.
 *
 * The caller window indicates to the prompt which window is prompting the
 * user. The prompt may choose to ignore this information or use it in whatever
 * way it sees fit.
 *
 * Returns: (transfer full): a newly allocated string containing the string
 *          handle of the window.
 */
gchar *
gcr_prompt_get_caller_window (GcrPrompt *prompt)
{
	gchar *caller_window = NULL;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_object_get (prompt, "caller-window", &caller_window, NULL);
	return caller_window;
}

/**
 * gcr_prompt_set_caller_window:
 * @prompt: the prompt
 * @window_id: the window id
 *
 * Set the string handle of the caller's window.
 *
 * The caller window indicates to the prompt which window is prompting the
 * user. The prompt may choose to ignore this information or use it in whatever
 * way it sees fit.
 */
void
gcr_prompt_set_caller_window (GcrPrompt *prompt,
                              const gchar *window_id)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "caller-window", window_id, NULL);
}

/**
 * gcr_prompt_get_continue_label:
 * @prompt: the prompt
 *
 * Get the label for the continue button.
 *
 * This is the button that results in a %GCR_PROMPT_REPLY_CONTINUE reply
 * from the prompt.
 *
 * Returns: (transfer full): a newly allocated string containing the label
 */
gchar *
gcr_prompt_get_continue_label (GcrPrompt *prompt)
{
	gchar *continue_label = NULL;
	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_object_get (prompt, "continue-label", &continue_label, NULL);
	return continue_label;
}

/**
 * gcr_prompt_set_continue_label:
 * @prompt: the prompt
 * @continue_label: the label
 *
 * Set the label for the continue button.
 *
 * This is the button that results in a %GCR_PROMPT_REPLY_CONTINUE reply
 * from the prompt.
 */
void
gcr_prompt_set_continue_label (GcrPrompt *prompt,
                               const gchar *continue_label)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "continue-label", continue_label, NULL);
}

/**
 * gcr_prompt_get_cancel_label:
 * @prompt: the prompt
 *
 * Get the label for the cancel button.
 *
 * This is the button that results in a %GCR_PROMPT_REPLY_CANCEL reply
 * from the prompt.
 *
 * Returns: (transfer full): a newly allocated string containing the label
 */
gchar *
gcr_prompt_get_cancel_label (GcrPrompt *prompt)
{
	gchar *cancel_label = NULL;
	g_object_get (prompt, "cancel-label", &cancel_label, NULL);
	return cancel_label;
}

/**
 * gcr_prompt_set_cancel_label:
 * @prompt: the prompt
 * @cancel_label: the label
 *
 * Set the label for the continue button.
 *
 * This is the button that results in a %GCR_PROMPT_REPLY_CANCEL reply
 * from the prompt.
 */
void
gcr_prompt_set_cancel_label (GcrPrompt *prompt,
                             const gchar *cancel_label)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_object_set (prompt, "cancel-label", cancel_label, NULL);
}

/**
 * gcr_prompt_password_async: (virtual prompt_password_async)
 * @prompt: a prompt
 * @cancellable: optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Prompts for password. Set the various properties on the prompt before calling
 * this method to explain which password should be entered.
 *
 * This method will return immediately and complete asynchronously.
 */
void
gcr_prompt_password_async (GcrPrompt *prompt,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	GcrPromptInterface *iface;

	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCR_PROMPT_GET_IFACE (prompt);
	g_return_if_fail (iface->prompt_password_async);

	(iface->prompt_password_async) (prompt, cancellable, callback, user_data);
}

/**
 * gcr_prompt_password_finish: (virtual prompt_password_finish)
 * @prompt: a prompt
 * @result: asynchronous result passed to callback
 * @error: location to place error on failure
 *
 * Complete an operation to prompt for a password.
 *
 * A password will be returned if the user enters a password successfully.
 * The returned password is valid until the next time a method is called
 * to display another prompt.
 *
 * %NULL will be returned if the user cancels or if an error occurs. Check the
 * @error argument to tell the difference.
 *
 * Returns: the password owned by the prompt, or %NULL
 */
const gchar *
gcr_prompt_password_finish (GcrPrompt *prompt,
                            GAsyncResult *result,
                            GError **error)
{
	GcrPromptInterface *iface;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	iface = GCR_PROMPT_GET_IFACE (prompt);
	g_return_val_if_fail (iface->prompt_password_async, NULL);

	return (iface->prompt_password_finish) (prompt, result, error);
}

/**
 * gcr_prompt_password:
 * @prompt: a prompt
 * @cancellable: optional cancellation object
 * @error: location to place error on failure
 *
 * Prompts for password. Set the various properties on the prompt before calling
 * this method to explain which password should be entered.
 *
 * This method will block until the a response is returned from the prompter.
 *
 * A password will be returned if the user enters a password successfully.
 * The returned password is valid until the next time a method is called
 * to display another prompt.
 *
 * %NULL will be returned if the user cancels or if an error occurs. Check the
 * @error argument to tell the difference.
 *
 * Returns: the password owned by the prompt, or %NULL
 */
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

/**
 * gcr_prompt_password_run:
 * @prompt: a prompt
 * @cancellable: optional cancellation object
 * @error: location to place error on failure
 *
 * Prompts for password. Set the various properties on the prompt before calling
 * this method to explain which password should be entered.
 *
 * This method will block until the a response is returned from the prompter
 * and will run a main loop similar to a gtk_dialog_run(). The application
 * will remain responsive but care must be taken to handle reentrancy issues.
 *
 * A password will be returned if the user enters a password successfully.
 * The returned password is valid until the next time a method is called
 * to display another prompt.
 *
 * %NULL will be returned if the user cancels or if an error occurs. Check the
 * @error argument to tell the difference.
 *
 * Returns: the password owned by the prompt, or %NULL
 */
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

/**
 * gcr_prompt_confirm_async: (virtual prompt_confirm_async)
 * @prompt: a prompt
 * @cancellable: optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Prompts for confirmation asking a cancel/continue style question.
 * Set the various properties on the prompt before calling this method to
 * represent the question correctly.
 *
 * This method will return immediately and complete asynchronously.
 */
void
gcr_prompt_confirm_async (GcrPrompt *prompt,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GcrPromptInterface *iface;

	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCR_PROMPT_GET_IFACE (prompt);
	g_return_if_fail (iface->prompt_confirm_async);

	(iface->prompt_confirm_async) (prompt, cancellable, callback, user_data);
}

/**
 * gcr_prompt_confirm_finish: (virtual prompt_confirm_finish)
 * @prompt: a prompt
 * @result: asynchronous result passed to callback
 * @error: location to place error on failure
 *
 * Complete an operation to prompt for confirmation.
 *
 * %GCR_PROMPT_REPLY_CONTINUE will be returned if the user confirms the prompt. The
 * return value will also be %GCR_PROMPT_REPLY_CANCEL if the user cancels or if
 * an error occurs. Check the @error argument to tell the difference.
 *
 * Returns: the reply from the prompt
 */
GcrPromptReply
gcr_prompt_confirm_finish (GcrPrompt *prompt,
                           GAsyncResult *result,
                           GError **error)
{
	GcrPromptInterface *iface;

	g_return_val_if_fail (GCR_IS_PROMPT (prompt), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), GCR_PROMPT_REPLY_CANCEL);
	g_return_val_if_fail (error == NULL || *error == NULL, GCR_PROMPT_REPLY_CANCEL);

	iface = GCR_PROMPT_GET_IFACE (prompt);
	g_return_val_if_fail (iface->prompt_confirm_async, GCR_PROMPT_REPLY_CANCEL);

	return (iface->prompt_confirm_finish) (prompt, result, error);
}

/**
 * gcr_prompt_confirm:
 * @prompt: a prompt
 * @cancellable: optional cancellation object
 * @error: location to place error on failure
 *
 * Prompts for confirmation asking a cancel/continue style question.
 * Set the various properties on the prompt before calling this function to
 * represent the question correctly.
 *
 * This method will block until the a response is returned from the prompter.
 *
 * %GCR_PROMPT_REPLY_CONTINUE will be returned if the user confirms the prompt. The
 * return value will also be %GCR_PROMPT_REPLY_CANCEL if the user cancels or if
 * an error occurs. Check the @error argument to tell the difference.
 *
 * Returns: the reply from the prompt
 */
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
 * gcr_prompt_confirm_run:
 * @prompt: a prompt
 * @cancellable: optional cancellation object
 * @error: location to place error on failure
 *
 * Prompts for confirmation asking a cancel/continue style question.
 * Set the various properties on the prompt before calling this function to
 * represent the question correctly.
 *
 * This method will block until the a response is returned from the prompter
 * and will run a main loop similar to a `gtk_dialog_run()`. The application
 * will remain responsive but care must be taken to handle reentrancy issues.
 *
 * %GCR_PROMPT_REPLY_CONTINUE will be returned if the user confirms the prompt. The
 * return value will also be %GCR_PROMPT_REPLY_CANCEL if the user cancels or if
 * an error occurs. Check the @error argument to tell the difference.
 *
 * Returns: the reply from the prompt
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

/**
 * gcr_prompt_close:
 * @prompt: a prompt
 *
 * Closes the prompt so that in can no longer be used to prompt. The various
 * prompt methods will return results as if the user dismissed the prompt.
 *
 * The prompt may also be closed by the implementor of the prompt object.
 *
 * This emits the [signal@Prompt::prompt-close] signal on the prompt object.
 */
void
gcr_prompt_close (GcrPrompt *prompt)
{
	g_return_if_fail (GCR_IS_PROMPT (prompt));
	g_signal_emit (prompt, signals[PROMPT_CLOSE], 0);
}
