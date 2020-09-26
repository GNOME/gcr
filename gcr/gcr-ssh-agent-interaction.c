/*
 * gnome-keyring
 *
 * Copyright (C) 2018 Red Hat, Inc.
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Daiki Ueno
 */

#include "config.h"

#include "gcr-ssh-agent-interaction.h"

#include "gcr-ssh-agent-private.h"
#include "gcr-prompt.h"
#include "gcr-system-prompt.h"

#include "egg/egg-secure-memory.h"
#include <glib/gi18n-lib.h>
#include <libsecret/secret.h>

static const SecretSchema schema = {
	"org.freedesktop.Secret.Generic", SECRET_SCHEMA_NONE,
	{
		{ "unique", SECRET_SCHEMA_ATTRIBUTE_STRING },
		{ NULL, 0 }
	}
};

enum {
	PROP_0,
	PROP_PROMPTER_NAME,
	PROP_LABEL,
	PROP_FIELDS
};

struct _GcrSshAgentInteraction {
	GTlsInteraction interaction;

	gchar *prompter_name;
	gchar *label;
	GHashTable *fields;
};

G_DEFINE_TYPE (GcrSshAgentInteraction, gcr_ssh_agent_interaction, G_TYPE_TLS_INTERACTION);

EGG_SECURE_DECLARE (gcr_ssh_agent_interaction);

static void
gcr_ssh_agent_interaction_init (GcrSshAgentInteraction *self)
{
}

static void
on_store_password_ready (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GError *error = NULL;

	if (secret_password_store_finish (res, &error)) {
		g_task_return_int (task, G_TLS_INTERACTION_HANDLED);
	} else {
		g_task_return_error (task, error);
	}
	g_object_unref (task);
}

static void
on_prompt_password (GObject *source_object,
		    GAsyncResult *result,
		    gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSshAgentInteraction *self = g_task_get_source_object (task);
	GTlsPassword *password = G_TLS_PASSWORD (g_task_get_task_data (task));
	GcrPrompt *prompt = GCR_PROMPT (source_object);
	GError *error = NULL;
	const gchar *value;
	gchar *secure_value;

	value = gcr_prompt_password_finish (prompt, result, &error);
	if (!value) {
		g_object_unref (prompt);
		if (error)
			g_task_return_error (task, error);
		else
			g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "cancelled");
		g_object_unref (task);
		return;
	}

	secure_value = egg_secure_strndup (value, strlen (value));
	g_tls_password_set_value_full (password,
				       (guchar *) secure_value,
				       strlen (secure_value),
				       (GDestroyNotify) egg_secure_free);

	if (gcr_prompt_get_choice_chosen (prompt)) {
		gchar *label;

		label = g_strdup_printf (_("Unlock password for: %s"), self->label);
		secret_password_storev (&schema,
					self->fields,
					NULL,
					label,
					secure_value,
					g_task_get_cancellable (task),
					on_store_password_ready,
					task);
		g_free (label);
	} else {
		g_task_return_int (task, G_TLS_INTERACTION_HANDLED);
		g_object_unref (task);
	}
	g_object_unref (prompt);
}

static void
on_prompt_open (GObject *source_object,
                GAsyncResult *result,
                gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GTlsPassword *password = g_task_get_task_data (task);
	GError *error = NULL;
	GcrPrompt *prompt;
	const gchar *choice;
	gchar *text;

	prompt = gcr_system_prompt_open_finish (result, &error);
	if (!prompt) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	gcr_prompt_set_title (prompt, _("Unlock private key"));
	gcr_prompt_set_message (prompt, _("Enter password to unlock the private key"));

	/* TRANSLATORS: The private key is locked */
	text = g_strdup_printf (_("An application wants access to the private key “%s”, but it is locked"),
				g_tls_password_get_description (password));
	gcr_prompt_set_description (prompt, text);
	g_free (text);

	choice = _("Automatically unlock this key whenever I’m logged in");
	gcr_prompt_set_choice_label (prompt, choice);
	gcr_prompt_set_continue_label (prompt, _("Unlock"));

	if (g_tls_password_get_flags (password) & G_TLS_PASSWORD_RETRY)
		gcr_prompt_set_warning (prompt, _("The unlock password was incorrect"));

	gcr_prompt_password_async (prompt, g_task_get_cancellable (task), on_prompt_password, task);
}

static void
on_lookup_password_ready (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSshAgentInteraction *self = g_task_get_source_object (task);
	GTlsPassword *password = G_TLS_PASSWORD (g_task_get_task_data (task));
	GError *error = NULL;

	gchar *value = secret_password_lookup_finish (res, &error);
	if (value) {
		/* If password is found in the login keyring, return it */
		g_tls_password_set_value_full (password,
					       (guchar *)value, strlen (value),
					       (GDestroyNotify)egg_secure_free);
		g_task_return_int (task, G_TLS_INTERACTION_HANDLED);
		g_object_unref (task);
	} else {
		/* Otherwise, prompt a password */
		gcr_system_prompt_open_for_prompter_async (self->prompter_name, 60,
							   g_task_get_cancellable (task),
							   on_prompt_open,
							   task);
	}
}

static void
gcr_ssh_agent_interaction_ask_password_async (GTlsInteraction *interaction,
					      GTlsPassword *password,
					      GCancellable *cancellable,
					      GAsyncReadyCallback callback,
					      gpointer user_data)
{
	GcrSshAgentInteraction *self = GCR_SSH_AGENT_INTERACTION (interaction);
	GTask *task;

	task = g_task_new (interaction, cancellable, callback, user_data);
	g_task_set_task_data (task, g_object_ref (password), g_object_unref);

	secret_password_lookupv (&schema,
				 self->fields,
				 cancellable,
				 on_lookup_password_ready,
				 task);
}

static GTlsInteractionResult
gcr_ssh_agent_interaction_ask_password_finish (GTlsInteraction *interaction,
					       GAsyncResult *res,
					       GError **error)
{
	GTask *task = G_TASK (res);
	GTlsInteractionResult result;

	result = g_task_propagate_int (task, error);
	if (result == -1)
		return G_TLS_INTERACTION_FAILED;
	return result;
}

static void
gcr_ssh_agent_interaction_set_property (GObject *object,
					guint prop_id,
					const GValue *value,
					GParamSpec *pspec)
{
	GcrSshAgentInteraction *self = GCR_SSH_AGENT_INTERACTION (object);

	switch (prop_id) {
	case PROP_PROMPTER_NAME:
		self->prompter_name = g_value_dup_string (value);
		break;
	case PROP_LABEL:
		self->label = g_value_dup_string (value);
		break;
	case PROP_FIELDS:
		self->fields = g_value_dup_boxed (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gcr_ssh_agent_interaction_finalize (GObject *object)
{
	GcrSshAgentInteraction *self = GCR_SSH_AGENT_INTERACTION (object);

	g_free (self->prompter_name);
	g_free (self->label);
	g_hash_table_unref (self->fields);

	G_OBJECT_CLASS (gcr_ssh_agent_interaction_parent_class)->finalize (object);
}

static void
gcr_ssh_agent_interaction_class_init (GcrSshAgentInteractionClass *klass)
{
	GTlsInteractionClass *interaction_class = G_TLS_INTERACTION_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	interaction_class->ask_password_async = gcr_ssh_agent_interaction_ask_password_async;
	interaction_class->ask_password_finish = gcr_ssh_agent_interaction_ask_password_finish;

	gobject_class->set_property = gcr_ssh_agent_interaction_set_property;
	gobject_class->finalize = gcr_ssh_agent_interaction_finalize;

	g_object_class_install_property (gobject_class, PROP_PROMPTER_NAME,
					 g_param_spec_string ("prompter-name", "Prompter-name", "Prompter-name",
							      NULL,
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
	g_object_class_install_property (gobject_class, PROP_LABEL,
					 g_param_spec_string ("label", "Label", "Label",
							      "",
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
	g_object_class_install_property (gobject_class, PROP_FIELDS,
					 g_param_spec_boxed ("fields", "Fields", "Fields",
							     G_TYPE_HASH_TABLE,
							     G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

GTlsInteraction *
gcr_ssh_agent_interaction_new (const gchar *prompter_name,
			       const gchar *label,
			       GHashTable *fields)
{
	return g_object_new (GCR_TYPE_SSH_AGENT_INTERACTION,
			     "prompter-name", prompter_name,
			     "label", label,
			     "fields", fields,
			     NULL);
}
