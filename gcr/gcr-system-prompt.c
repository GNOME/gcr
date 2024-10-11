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

#include "gcr-dbus-constants.h"
#include "gcr-internal.h"
#include "gcr-library.h"
#include "gcr-prompt.h"
#include "gcr-secret-exchange.h"
#include "gcr-system-prompt.h"

#include "gcr/gcr-dbus-generated.h"

#include "egg/egg-error.h"

#include <glib/gi18n.h>

/**
 * GcrSystemPrompt:
 *
 * A [iface@Prompt] implementation which calls to the system prompter to
 * display prompts in a system modal fashion.
 *
 * Since the system prompter usually only displays one prompt at a time, you
 * may have to wait for the prompt to be displayed. Use [func@SystemPrompt.open]
 * or a related function to open a prompt. Since this can take a long time, you
 * should always check that the prompt is still needed after it is opened. A
 * previous prompt may have already provided the information needed and you
 * may no longer need to prompt.
 *
 * Use [method@SystemPrompt.close] to close the prompt when you're done with it.
 */

/**
 * GCR_SYSTEM_PROMPT_ERROR:
 *
 * The domain for errors returned from GcrSystemPrompt methods.
 */

/**
 * GcrSystemPromptError:
 * @GCR_SYSTEM_PROMPT_IN_PROGRESS: another prompt is already in progress
 *
 * No error returned by the #GcrSystemPrompt is suitable for display or
 * to the user.
 *
 * If the system prompter can only show one prompt at a time, and there is
 * already a prompt being displayed, and the timeout waiting to open the
 * prompt expires, then %GCR_SYSTEM_PROMPT_IN_PROGRESS is returned.
 */

enum {
	PROP_0,
	PROP_BUS_NAME,
	PROP_SECRET_EXCHANGE,
	PROP_TIMEOUT_SECONDS,

	PROP_TITLE,
	PROP_MESSAGE,
	PROP_DESCRIPTION,
	PROP_WARNING,
	PROP_PASSWORD_NEW,
	PROP_PASSWORD_STRENGTH,
	PROP_CHOICE_LABEL,
	PROP_CHOICE_CHOSEN,
	PROP_CALLER_WINDOW,
	PROP_CONTINUE_LABEL,
	PROP_CANCEL_LABEL
};

struct _GcrSystemPromptPrivate {
	gchar *prompter_bus_name;
	GcrSecretExchange *exchange;
	gboolean received;
	GHashTable *properties;
	GHashTable *dirty_properties;
	gint timeout_seconds;

	GDBusConnection *connection;
	gboolean begun_prompting;
	gboolean closed;
	guint prompt_registered;
	gchar *prompt_path;
	gchar *prompt_owner;

	GTask *pending;
	gchar *last_response;
};

static void     gcr_system_prompt_prompt_iface         (GcrPromptInterface *iface);

static void     gcr_system_prompt_initable_iface       (GInitableIface *iface);

static void     gcr_system_prompt_async_initable_iface (GAsyncInitableIface *iface);

static void     perform_init_async                     (GcrSystemPrompt *self,
                                                        GTask *task);

G_DEFINE_TYPE_WITH_CODE (GcrSystemPrompt, gcr_system_prompt, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GcrSystemPrompt);
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_PROMPT, gcr_system_prompt_prompt_iface);
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gcr_system_prompt_initable_iface);
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, gcr_system_prompt_async_initable_iface);
);

static gint unique_prompt_id = 0;

typedef struct {
	GSource *timeout;
	GSource *waiting;
	GMainContext *context;
	guint watch_id;
} CallClosure;

static void
call_closure_free (gpointer data)
{
	CallClosure *closure = data;
	if (closure->timeout) {
		g_source_destroy (closure->timeout);
		g_source_unref (closure->timeout);
	}
	if (closure->waiting) {
		g_source_destroy (closure->waiting);
		g_source_unref (closure->waiting);
	}
	if (closure->watch_id)
		g_bus_unwatch_name (closure->watch_id);
	g_free (data);
}

static void
on_propagate_cancelled (GCancellable *cancellable,
                        gpointer user_data)
{
	/* Propagate the cancelled signal */
	GCancellable *cancel = G_CANCELLABLE (user_data);
	g_cancellable_cancel (cancel);
}


/*
 * We use our own cancellable object, since we cancel it it in
 * situations other than when the caller cancels.
 *
 * Returns: (transfer none): a new GCancellable owned by the original one.
 */
static GCancellable *
get_sub_cancellable (GCancellable *cancellable)
{
	GCancellable *closure_cancellable;

	if (!cancellable)
		return NULL;

	closure_cancellable = g_cancellable_new ();
	g_cancellable_connect (cancellable, G_CALLBACK (on_propagate_cancelled),
	                       closure_cancellable, g_object_unref);
	return closure_cancellable;
}

static CallClosure *
call_closure_new (gboolean with_context)
{
	CallClosure *call;

	call = g_new0 (CallClosure, 1);
	if (with_context) {
		call->context = g_main_context_get_thread_default ();
		if (call->context)
			g_main_context_ref (call->context);
	}

	return call;
}

static void
gcr_system_prompt_init (GcrSystemPrompt *self)
{
	self->pv = gcr_system_prompt_get_instance_private (self);

	self->pv->timeout_seconds = -1;
	self->pv->properties = g_hash_table_new_full (g_direct_hash, g_direct_equal,
	                                              NULL, (GDestroyNotify)g_variant_unref);
	self->pv->dirty_properties = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static const gchar *
prompt_get_string_property (GcrSystemPrompt *self,
                            const gchar *property_name,
                            gboolean collapse_empty_to_null)
{
	const gchar *value = NULL;
	GVariant *variant;
	gconstpointer key;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), NULL);

	key = g_intern_string (property_name);
	variant = g_hash_table_lookup (self->pv->properties, key);
	if (variant != NULL) {
		value = g_variant_get_string (variant, NULL);
		if (collapse_empty_to_null && value != NULL && value[0] == '\0')
			value = NULL;
	}

	return value;
}

static void
prompt_set_string_property (GcrSystemPrompt *self,
                            const gchar *property_name,
                            const gchar *value)
{
	GVariant *variant;
	gpointer key;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPT (self));

	key = (gpointer)g_intern_string (property_name);
	variant = g_variant_ref_sink (g_variant_new_string (value ? value : ""));
	g_hash_table_insert (self->pv->properties, key, variant);
	g_hash_table_insert (self->pv->dirty_properties, key, key);
	g_object_notify (G_OBJECT (self), property_name);
}

static gint
prompt_get_int_property (GcrSystemPrompt *self,
                         const gchar *property_name)
{
	GVariant *variant;
	gconstpointer key;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), 0);

	key = g_intern_string (property_name);
	variant = g_hash_table_lookup (self->pv->properties, key);
	if (variant != NULL)
		return g_variant_get_int32 (variant);

	return 0;
}

static gboolean
prompt_get_boolean_property (GcrSystemPrompt *self,
                             const gchar *property_name)
{
	GVariant *variant;
	gconstpointer key;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), FALSE);

	key = g_intern_string (property_name);
	variant = g_hash_table_lookup (self->pv->properties, key);
	if (variant != NULL)
		return g_variant_get_boolean (variant);

	return FALSE;
}

static void
prompt_set_boolean_property (GcrSystemPrompt *self,
                             const gchar *property_name,
                             gboolean value)
{
	GVariant *variant;
	gpointer key;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPT (self));

	key = (gpointer)g_intern_string (property_name);
	variant = g_variant_ref_sink (g_variant_new_boolean (value));
	g_hash_table_insert (self->pv->properties, key, variant);
	g_hash_table_insert (self->pv->dirty_properties, key, key);
	g_object_notify (G_OBJECT (self), property_name);
}

static void
gcr_system_prompt_set_property (GObject *obj,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (obj);

	switch (prop_id) {
	case PROP_BUS_NAME:
		g_assert (self->pv->prompter_bus_name == NULL);
		self->pv->prompter_bus_name = g_value_dup_string (value);
		break;
	case PROP_SECRET_EXCHANGE:
		if (self->pv->exchange) {
			g_warning ("The secret exchange is already in use, and cannot be changed");
			return;
		}
		self->pv->exchange = g_value_dup_object (value);
		g_object_notify (G_OBJECT (self), "secret-exchange");
		break;
	case PROP_TIMEOUT_SECONDS:
		self->pv->timeout_seconds = g_value_get_int (value);
		break;
	case PROP_TITLE:
		prompt_set_string_property (self, "title", g_value_get_string (value));
		break;
	case PROP_MESSAGE:
		prompt_set_string_property (self, "message", g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		prompt_set_string_property (self, "description", g_value_get_string (value));
		break;
	case PROP_WARNING:
		prompt_set_string_property (self, "warning", g_value_get_string (value));
		break;
	case PROP_PASSWORD_NEW:
		prompt_set_boolean_property (self, "password-new", g_value_get_boolean (value));
		break;
	case PROP_CHOICE_LABEL:
		prompt_set_string_property (self, "choice-label", g_value_get_string (value));
		break;
	case PROP_CHOICE_CHOSEN:
		prompt_set_boolean_property (self, "choice-chosen", g_value_get_boolean (value));
		break;
	case PROP_CALLER_WINDOW:
		prompt_set_string_property (self, "caller-window", g_value_get_string (value));
		break;
	case PROP_CONTINUE_LABEL:
		prompt_set_string_property (self, "continue-label", g_value_get_string (value));
		break;
	case PROP_CANCEL_LABEL:
		prompt_set_string_property (self, "cancel-label", g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_system_prompt_get_property (GObject *obj,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (obj);

	switch (prop_id) {
	case PROP_BUS_NAME:
		g_value_set_string (value, self->pv->prompter_bus_name);
		break;
	case PROP_SECRET_EXCHANGE:
		g_value_set_object (value, gcr_system_prompt_get_secret_exchange (self));
		break;
	case PROP_TITLE:
		g_value_set_string (value, prompt_get_string_property (self, "title", FALSE));
		break;
	case PROP_MESSAGE:
		g_value_set_string (value, prompt_get_string_property (self, "message", FALSE));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, prompt_get_string_property (self, "description", FALSE));
		break;
	case PROP_WARNING:
		g_value_set_string (value, prompt_get_string_property (self, "warning", TRUE));
		break;
	case PROP_PASSWORD_NEW:
		g_value_set_boolean (value, prompt_get_boolean_property (self, "password-new"));
		break;
	case PROP_PASSWORD_STRENGTH:
		g_value_set_int (value, prompt_get_int_property (self, "password-strength"));
		break;
	case PROP_CHOICE_LABEL:
		g_value_set_string (value, prompt_get_string_property (self, "choice-label", TRUE));
		break;
	case PROP_CHOICE_CHOSEN:
		g_value_set_boolean (value, prompt_get_boolean_property (self, "choice-chosen"));
		break;
	case PROP_CALLER_WINDOW:
		g_value_set_string (value, prompt_get_string_property (self, "caller-window", TRUE));
		break;
	case PROP_CONTINUE_LABEL:
		g_value_set_string (value, prompt_get_string_property (self, "continue-label", TRUE));
		break;
	case PROP_CANCEL_LABEL:
		g_value_set_string (value, prompt_get_string_property (self, "cancel-label", TRUE));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_system_prompt_constructed (GObject *obj)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (obj);
	gint seed;

	G_OBJECT_CLASS (gcr_system_prompt_parent_class)->constructed (obj);

	seed = g_atomic_int_add (&unique_prompt_id, 1);

	self->pv->prompt_path = g_strdup_printf ("%s/p%d", GCR_DBUS_PROMPT_OBJECT_PREFIX, seed);

	if (self->pv->prompter_bus_name == NULL)
		self->pv->prompter_bus_name = g_strdup (GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME);
}

static void
on_prompter_stop_prompting (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	GError *error = NULL;
	GVariant *retval;

	retval = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
	if (error != NULL) {
		g_debug ("failed to stop prompting: %s", egg_error_message (error));
		g_clear_error (&error);
	}

	if (retval)
		g_variant_unref (retval);

	if (user_data) {
		GTask *task = G_TASK (user_data);
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
	}
}

static void
perform_close (GcrSystemPrompt *self,
               GTask *async)
{
	gboolean closed;

	closed = self->pv->closed;
	self->pv->closed = TRUE;

	if (!closed)
		g_debug ("closing prompt");

	if (self->pv->pending) {
		GTask *pending_task;
		GCancellable *cancellable;

		pending_task = g_steal_pointer (&self->pv->pending);
		cancellable = g_task_get_cancellable (pending_task);
		g_cancellable_cancel (cancellable);
		if (!g_task_had_error (pending_task)) {
			g_task_return_error_if_cancelled (pending_task);
		}

		g_object_unref (pending_task);
	}

	if (self->pv->prompt_registered) {
		g_dbus_connection_unregister_object (self->pv->connection,
		                                     self->pv->prompt_registered);
		self->pv->prompt_registered = 0;
	}

	if (self->pv->begun_prompting) {
		if (self->pv->connection && self->pv->prompt_path && self->pv->prompt_owner) {
			GCancellable *cancellable = async ? g_task_get_cancellable (async) : NULL;
			g_debug ("Calling the prompter %s method", GCR_DBUS_PROMPTER_METHOD_STOP);
			g_dbus_connection_call (self->pv->connection,
			                        self->pv->prompter_bus_name,
			                        GCR_DBUS_PROMPTER_OBJECT_PATH,
			                        GCR_DBUS_PROMPTER_INTERFACE,
			                        GCR_DBUS_PROMPTER_METHOD_STOP,
			                        g_variant_new ("(o)", self->pv->prompt_path),
			                        G_VARIANT_TYPE ("()"),
			                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
			                        -1, cancellable,
			                        on_prompter_stop_prompting,
			                        g_steal_pointer (&async));
		}
		self->pv->begun_prompting = FALSE;
	}

	g_free (self->pv->prompt_path);
	self->pv->prompt_path = NULL;

	g_clear_object (&self->pv->connection);

	if (async) {
		g_task_return_boolean (async, TRUE);
		g_object_unref (async);
	}

	/* Emit the signal if necessary, after closed */
	if (!closed)
		gcr_prompt_close (GCR_PROMPT (self));
}

static void
gcr_system_prompt_dispose (GObject *obj)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (obj);

	g_clear_object (&self->pv->exchange);
	perform_close (self, NULL);

	g_hash_table_remove_all (self->pv->properties);
	g_hash_table_remove_all (self->pv->dirty_properties);

	G_OBJECT_CLASS (gcr_system_prompt_parent_class)->dispose (obj);
}

static void
gcr_system_prompt_finalize (GObject *obj)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (obj);

	g_free (self->pv->prompter_bus_name);
	g_free (self->pv->prompt_owner);
	g_free (self->pv->last_response);
	g_hash_table_destroy (self->pv->properties);
	g_hash_table_destroy (self->pv->dirty_properties);

	G_OBJECT_CLASS (gcr_system_prompt_parent_class)->finalize (obj);
}

static void
gcr_system_prompt_class_init (GcrSystemPromptClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = gcr_system_prompt_constructed;
	gobject_class->get_property = gcr_system_prompt_get_property;
	gobject_class->set_property = gcr_system_prompt_set_property;
	gobject_class->dispose = gcr_system_prompt_dispose;
	gobject_class->finalize = gcr_system_prompt_finalize;

	/**
	 * GcrSystemPrompt:bus-name:
	 *
	 * The DBus bus name of the prompter to use for prompting, or %NULL
	 * for the default prompter.
	 */
	g_object_class_install_property (gobject_class, PROP_BUS_NAME,
		g_param_spec_string ("bus-name", "Bus name", "Prompter bus name",
		                     NULL,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GcrSystemPrompt:timeout-seconds:
	 *
	 * The timeout in seconds to wait when opening the prompt.
	 */
	g_object_class_install_property (gobject_class, PROP_TIMEOUT_SECONDS,
		g_param_spec_int ("timeout-seconds", "Timeout seconds", "Timeout (in seconds) for opening prompt",
		                  -1, G_MAXINT, -1,
		                  G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GcrSystemPrompt:secret-exchange:
	 *
	 * The #GcrSecretExchange to use when transferring passwords. A default
	 * secret exchange will be used if this is not set.
	 */
	g_object_class_install_property (gobject_class, PROP_SECRET_EXCHANGE,
		g_param_spec_object ("secret-exchange", "Secret exchange", "Secret exchange for passing passwords",
		                     GCR_TYPE_SECRET_EXCHANGE,
		                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_override_property (gobject_class, PROP_TITLE, "title");
	g_object_class_override_property (gobject_class, PROP_MESSAGE, "message");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
	g_object_class_override_property (gobject_class, PROP_WARNING, "warning");
	g_object_class_override_property (gobject_class, PROP_PASSWORD_NEW, "password-new");
	g_object_class_override_property (gobject_class, PROP_PASSWORD_STRENGTH, "password-strength");
	g_object_class_override_property (gobject_class, PROP_CHOICE_LABEL, "choice-label");
	g_object_class_override_property (gobject_class, PROP_CHOICE_CHOSEN, "choice-chosen");
	g_object_class_override_property (gobject_class, PROP_CALLER_WINDOW, "caller-window");
	g_object_class_override_property (gobject_class, PROP_CONTINUE_LABEL, "continue-label");
	g_object_class_override_property (gobject_class, PROP_CANCEL_LABEL, "cancel-label");
}

/**
 * gcr_system_prompt_get_secret_exchange:
 * @self: a prompter
 *
 * Get the current [class@SecretExchange] used to transfer secrets in this prompt.
 *
 * Returns: (transfer none): the secret exchange
 */
GcrSecretExchange *
gcr_system_prompt_get_secret_exchange (GcrSystemPrompt *self)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), NULL);

	if (!self->pv->exchange) {
		g_debug ("creating new secret exchange");
		self->pv->exchange = gcr_secret_exchange_new (NULL);
	}

	return self->pv->exchange;
}

static void
update_properties_from_iter (GcrSystemPrompt *self,
                             GVariantIter *iter)
{
	GObject *obj = G_OBJECT (self);
	GVariant *variant;
	GVariant *value;
	GVariant *already;
	gpointer key;
	gchar *name;

	g_object_freeze_notify (obj);
	while (g_variant_iter_loop (iter, "{sv}", &name, &variant)) {
		key = (gpointer)g_intern_string (name);
		value = g_variant_get_variant (variant);
		already = g_hash_table_lookup (self->pv->properties, key);
		if (!already || !g_variant_equal (already, value)) {
			g_hash_table_replace (self->pv->properties, key, g_variant_ref (value));
			g_object_notify (obj, name);
		}
		g_variant_unref (value);
	}
	g_object_thaw_notify (obj);
}

static GcrPromptReply
handle_last_response (GcrSystemPrompt *self)
{
	GcrPromptReply response;

	g_return_val_if_fail (self->pv->last_response != NULL,
	                      GCR_PROMPT_REPLY_CANCEL);

	if (g_str_equal (self->pv->last_response, GCR_DBUS_PROMPT_REPLY_YES)) {
		response = GCR_PROMPT_REPLY_CONTINUE;

	} else if (g_str_equal (self->pv->last_response, GCR_DBUS_PROMPT_REPLY_NO) ||
	           g_str_equal (self->pv->last_response, GCR_DBUS_PROMPT_REPLY_NONE)) {
		response = GCR_PROMPT_REPLY_CANCEL;

	} else {
		g_warning ("unknown response from prompter: %s", self->pv->last_response);
		response = GCR_PROMPT_REPLY_CANCEL;
	}

	return response;
}

static void
prompt_method_ready (GcrSystemPrompt *self,
                     GDBusMethodInvocation *invocation,
                     GVariant *parameters)
{
	GcrSecretExchange *exchange;
	GTask *task;
	GVariantIter *iter;
	gchar *received;

	g_return_if_fail (G_TASK (self->pv->pending));

	g_free (self->pv->last_response);
	g_variant_get (parameters, "(sa{sv}s)",
	               &self->pv->last_response,
	               &iter, &received);

	g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));

	update_properties_from_iter (self, iter);
	g_variant_iter_free (iter);

	exchange = gcr_system_prompt_get_secret_exchange (self);
	if (gcr_secret_exchange_receive (exchange, received))
		self->pv->received = TRUE;
	else
		g_warning ("received invalid secret exchange string");
	g_free (received);

	task = g_steal_pointer (&self->pv->pending);
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static void
prompt_method_done (GcrSystemPrompt *self,
                    GDBusMethodInvocation *invocation,
                    GVariant *parameters)
{
	g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));

	/*
	 * At this point we're done prompting, and calling StopPrompting
	 * on the prompter is no longer necessary. It may have already been
	 * called, or the prompter may have stopped on its own accord.
	 */
	self->pv->begun_prompting = FALSE;
	perform_close (self, NULL);
}

static void
prompt_method_call (GDBusConnection *connection,
                    const gchar *sender,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (user_data);

	g_return_if_fail (method_name != NULL);

	if (g_str_equal (method_name, GCR_DBUS_CALLBACK_METHOD_READY)) {
		prompt_method_ready (self, invocation, parameters);

	} else if (g_str_equal (method_name, GCR_DBUS_CALLBACK_METHOD_DONE)) {
		prompt_method_done (self, invocation, parameters);

	} else {
		g_return_if_reached ();
	}
}

static GVariant *
prompt_get_property (GDBusConnection *connection,
                     const gchar *sender,
                     const gchar *object_path,
                     const gchar *interface_name,
                     const gchar *property_name,
                     GError **error,
                     gpointer user_data)
{
	g_return_val_if_reached (NULL);
}

static gboolean
prompt_set_property (GDBusConnection *connection,
                     const gchar *sender,
                     const gchar *object_path,
                     const gchar *interface_name,
                     const gchar *property_name,
                     GVariant *value,
                     GError **error,
                     gpointer user_data)
{
	g_return_val_if_reached (FALSE);
}


static GDBusInterfaceVTable prompt_dbus_vtable = {
	prompt_method_call,
	prompt_get_property,
	prompt_set_property,
};

static void
register_prompt_object (GcrSystemPrompt *self,
                        GError **error)
{
	GError *lerror = NULL;
	guint id;

	/*
	 * The unregister/reregister allows us to register this under a whatever
	 * GMainContext has been pushed as the thread default context.
	 */

	if (self->pv->prompt_registered)
		g_dbus_connection_unregister_object (self->pv->connection,
		                                     self->pv->prompt_registered);

	id = g_dbus_connection_register_object (self->pv->connection,
	                                        self->pv->prompt_path,
	                                        _gcr_dbus_prompter_callback_interface_info (),
	                                        &prompt_dbus_vtable,
	                                        self, NULL, &lerror);
	self->pv->prompt_registered = id;

	if (lerror != NULL) {
		g_warning ("error registering prompter %s", egg_error_message (lerror));
		g_propagate_error (error, lerror);
	}
}

static void
on_prompter_present (GDBusConnection *connection,
                     const gchar *name,
                     const gchar *name_owner,
                     gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_task_get_source_object (task));

	g_free (self->pv->prompt_owner);
	self->pv->prompt_owner = g_strdup (name_owner);
}

static void
on_prompter_vanished (GDBusConnection *connection,
                      const gchar *name,
                      gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_task_get_source_object (task));

	if (self->pv->prompt_owner) {
		GCancellable *cancellable;
		cancellable = g_task_get_cancellable (task);
		g_free (self->pv->prompt_owner);
		self->pv->prompt_owner = NULL;
		g_debug ("prompter name owner has vanished: %s", name);
		g_cancellable_cancel (cancellable);
	}
}

static void
on_bus_connected (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_task_get_source_object (task));
	CallClosure *closure = g_task_get_task_data (task);
	GError *error = NULL;

	g_assert (self->pv->connection == NULL);
	self->pv->connection = g_bus_get_finish (result, &error);

	if (error != NULL) {
		g_debug ("failed to connect to bus: %s", egg_error_message (error));

	} else {
		g_return_if_fail (self->pv->connection != NULL);
		g_debug ("connected to bus");

		g_main_context_push_thread_default (closure->context);

		closure->watch_id = g_bus_watch_name_on_connection (self->pv->connection,
		                                                    self->pv->prompter_bus_name,
		                                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
		                                                    on_prompter_present,
		                                                    on_prompter_vanished,
		                                                    task, NULL);

		register_prompt_object (self, &error);

		g_main_context_pop_thread_default (closure->context);
	}

	if (error == NULL) {
		perform_init_async (self, g_steal_pointer (&task));
	} else {
		g_task_return_error (task, g_steal_pointer (&error));
		g_object_unref (task);
	}
}

static void
on_prompter_begin_prompting (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_task_get_source_object (task));
	GError *error = NULL;
	GVariant *ret;

	ret = g_dbus_connection_call_finish (self->pv->connection, result, &error);

	if (error == NULL) {
		self->pv->begun_prompting = TRUE;
		g_variant_unref (ret);

		g_debug ("registered prompt %s: %s",
		         self->pv->prompter_bus_name, self->pv->prompt_path);

		g_return_if_fail (self->pv->prompt_path != NULL);
		perform_init_async (self, g_steal_pointer (&task));

	} else {
		g_debug ("failed to register prompt %s: %s",
		         self->pv->prompter_bus_name, egg_error_message (error));

		g_task_return_error (task, g_steal_pointer (&error));
		g_object_unref (task);
	}
}

static gboolean
on_call_timeout (gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_task_get_source_object (task));
	CallClosure *closure = g_task_get_task_data (task);

	g_source_destroy (closure->timeout);
	g_clear_pointer (&closure->timeout, g_source_unref);

	/* Tell the prompter we're no longer interested */
	perform_close (self, NULL);

	g_task_return_new_error_literal (task, GCR_SYSTEM_PROMPT_ERROR,
	                                 GCR_SYSTEM_PROMPT_IN_PROGRESS,
	                                 _("Another prompt is already in progress"));

	return G_SOURCE_REMOVE; /* Don't call this function again */
}

static gboolean
on_call_cancelled (GCancellable *cancellable,
                   gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_task_get_source_object (task));
	CallClosure *call = g_task_get_task_data (task);

	g_source_destroy (call->waiting);
	g_clear_pointer (&call->waiting, g_source_unref);

	g_task_return_new_error_literal (task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
	                                 _("The operation was cancelled"));

	/* Tell the prompter we're no longer interested */
	perform_close (self, NULL);

	return G_SOURCE_REMOVE; /* Don't call this function again */
}

void
perform_init_async (GcrSystemPrompt *self,
                    GTask *task)
{
	CallClosure *closure = g_task_get_task_data (task);
	GCancellable *cancellable = g_task_get_cancellable (task);

	g_main_context_push_thread_default (closure->context);

	/* 1. Connect to the session bus */
	if (!self->pv->connection) {
		g_debug ("connecting to bus");
		g_bus_get (G_BUS_TYPE_SESSION, cancellable,
		           on_bus_connected, g_steal_pointer (&task));

	/* 2. Export our object, BeginPrompting on prompter */
	} else if (!self->pv->begun_prompting) {
		g_assert (self->pv->prompt_path != NULL);

		g_debug ("calling %s method on prompter", GCR_DBUS_PROMPTER_METHOD_BEGIN);
		g_dbus_connection_call (self->pv->connection,
		                        self->pv->prompter_bus_name,
		                        GCR_DBUS_PROMPTER_OBJECT_PATH,
		                        GCR_DBUS_PROMPTER_INTERFACE,
		                        GCR_DBUS_PROMPTER_METHOD_BEGIN,
		                        g_variant_new ("(o)", self->pv->prompt_path),
		                        G_VARIANT_TYPE ("()"),
		                        G_DBUS_CALL_FLAGS_NONE,
		                        -1, cancellable,
		                        on_prompter_begin_prompting,
		                        g_steal_pointer (&task));

	/* 3. Wait for iterate */
	} else if (!self->pv->pending) {
		self->pv->pending = g_object_ref (task);
		if (self->pv->timeout_seconds > 0) {
			g_assert (closure->timeout == NULL);
			closure->timeout = g_timeout_source_new_seconds (self->pv->timeout_seconds);
			g_source_set_callback (closure->timeout, on_call_timeout, task, NULL);
			g_source_attach (closure->timeout, closure->context);
		}

		g_assert (closure->waiting == NULL);
		closure->waiting = g_cancellable_source_new (cancellable);
		g_source_set_callback (closure->waiting, (GSourceFunc)on_call_cancelled, task, NULL);
		g_source_attach (closure->waiting, closure->context);
		g_object_unref (task);

	/* 4. All done */
	} else {
		g_assert_not_reached ();
	}

	g_main_context_pop_thread_default (closure->context);
}

static void
gcr_system_prompt_real_init_async (GAsyncInitable *initable,
                                   int io_priority,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (initable);
	GTask *task;
	CallClosure *closure;

	closure = call_closure_new (TRUE);
	task = g_task_new (self, get_sub_cancellable (cancellable), callback, user_data);
	g_task_set_source_tag (task, gcr_system_prompt_real_init_async);
	g_task_set_task_data (task, closure, call_closure_free);

	perform_init_async (self, g_steal_pointer (&task));

}

static gboolean
gcr_system_prompt_real_init_finish (GAsyncInitable *initable,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (initable), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, initable), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, gcr_system_prompt_real_init_async), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gcr_system_prompt_async_initable_iface (GAsyncInitableIface *iface)
{
	iface->init_async = gcr_system_prompt_real_init_async;
	iface->init_finish = gcr_system_prompt_real_init_finish;
}

typedef struct {
	GAsyncResult *result;
	GMainContext *context;
	GMainLoop *loop;
} SyncClosure;

static SyncClosure *
sync_closure_new (void)
{
	SyncClosure *closure;

	closure = g_new0 (SyncClosure, 1);

	closure->context = g_main_context_new ();
	closure->loop = g_main_loop_new (closure->context, FALSE);

	return closure;
}

static void
sync_closure_free (gpointer data)
{
	SyncClosure *closure = data;

	g_clear_object (&closure->result);
	g_main_loop_unref (closure->loop);
	g_main_context_unref (closure->context);
	g_free (closure);
}

static void
on_sync_result (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
	SyncClosure *closure = user_data;
	closure->result = g_object_ref (result);
	g_main_loop_quit (closure->loop);
}

static gboolean
gcr_system_prompt_real_init (GInitable *initable,
                             GCancellable *cancellable,
                             GError **error)
{
	SyncClosure *closure;
	gboolean result;

	closure = sync_closure_new ();
	g_main_context_push_thread_default (closure->context);

	gcr_system_prompt_real_init_async (G_ASYNC_INITABLE (initable),
	                                   G_PRIORITY_DEFAULT, cancellable,
	                                   on_sync_result, closure);

	g_main_loop_run (closure->loop);

	result = gcr_system_prompt_real_init_finish (G_ASYNC_INITABLE (initable),
	                                             closure->result, error);

	g_main_context_pop_thread_default (closure->context);
	sync_closure_free (closure);

	return result;
}

static void
gcr_system_prompt_initable_iface (GInitableIface *iface)
{
	iface->init = gcr_system_prompt_real_init;
}

static GVariantBuilder *
build_dirty_properties (GcrSystemPrompt *self)
{
	GVariantBuilder *builder;
	GHashTableIter iter;
	GVariant *variant;
	gpointer key;

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

	g_hash_table_iter_init (&iter, self->pv->dirty_properties);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		variant = g_hash_table_lookup (self->pv->properties, key);
		g_variant_builder_add (builder, "{sv}", (const gchar *)key, variant);
	}

	g_hash_table_remove_all (self->pv->dirty_properties);
	return builder;
}

static void
on_perform_prompt_complete (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_task_get_source_object (task));
	CallClosure *call = g_task_get_task_data (task);
	GError *error = NULL;
	GVariant *retval;

	retval = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
	if (error != NULL) {
		g_clear_object (&self->pv->pending);
		g_task_return_error (task, g_steal_pointer (&error));
	} else {

		g_assert (call->waiting == NULL);
		call->waiting = g_cancellable_source_new (g_task_get_cancellable (task));
		g_source_set_callback (call->waiting, (GSourceFunc)on_call_cancelled, task, NULL);
		g_source_attach (call->waiting, call->context);
	}

	if (retval)
		g_variant_unref (retval);

	g_object_unref (task);
}

static void
perform_prompt_async (GcrSystemPrompt *self,
                      const gchar *type,
                      gpointer source_tag,
                      GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
	GTask *task;
	GcrSecretExchange *exchange;
	GVariantBuilder *builder;
	CallClosure *closure;
	gchar *sent;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPT (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	if (self->pv->pending != NULL) {
		g_warning ("another operation is already pending on this prompt");
		return;
	}

	closure = call_closure_new (FALSE);
	task = g_task_new (self, get_sub_cancellable (cancellable), callback, user_data);
	g_task_set_source_tag (task, source_tag);
	g_task_set_task_data (task, closure, call_closure_free);

	if (self->pv->closed) {
		g_free (self->pv->last_response);
		self->pv->last_response = g_strdup (GCR_DBUS_PROMPT_REPLY_NONE);
		g_task_return_int (task, handle_last_response (self));
		g_object_unref (task);
		return;
	}

	g_debug ("prompting for password");

	exchange = gcr_system_prompt_get_secret_exchange (self);
	if (self->pv->received)
		sent = gcr_secret_exchange_send (exchange, NULL, 0);
	else
		sent = gcr_secret_exchange_begin (exchange);

	closure->watch_id = g_bus_watch_name_on_connection (self->pv->connection,
	                                                    self->pv->prompter_bus_name,
	                                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
	                                                    on_prompter_present,
	                                                    on_prompter_vanished,
	                                                    task, NULL);

	builder = build_dirty_properties (self);

	/* Reregister the prompt object in the current GMainContext */
	register_prompt_object (self, NULL);

	self->pv->pending = g_object_ref (task);
	g_dbus_connection_call (self->pv->connection,
	                        self->pv->prompter_bus_name,
	                        GCR_DBUS_PROMPTER_OBJECT_PATH,
	                        GCR_DBUS_PROMPTER_INTERFACE,
	                        GCR_DBUS_PROMPTER_METHOD_PERFORM,
	                        g_variant_new ("(osa{sv}s)", self->pv->prompt_path,
	                                       type, builder, sent),
	                        G_VARIANT_TYPE ("()"),
	                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
	                        -1, cancellable,
	                        on_perform_prompt_complete,
	                        g_steal_pointer (&task));
	g_variant_builder_unref(builder);
	g_free (sent);
}

static void
gcr_system_prompt_password_async (GcrPrompt *prompt,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (prompt);
	perform_prompt_async (self, GCR_DBUS_PROMPT_TYPE_PASSWORD,
	                      gcr_system_prompt_password_async,
	                      cancellable, callback, user_data);
}

static const gchar *
gcr_system_prompt_password_finish (GcrPrompt *prompt,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (prompt), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, prompt), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, gcr_system_prompt_password_async), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (g_task_propagate_int (G_TASK (result), error) == GCR_PROMPT_REPLY_CONTINUE) {
		GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (prompt);
		return gcr_secret_exchange_get_secret (self->pv->exchange, NULL);
	}

	return NULL;
}

static void
gcr_system_prompt_confirm_async (GcrPrompt *prompt,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (prompt);
	perform_prompt_async (self, GCR_DBUS_PROMPT_TYPE_CONFIRM,
	                      gcr_system_prompt_confirm_async,
	                      cancellable, callback, user_data);
}

static GcrPromptReply
gcr_system_prompt_confirm_finish (GcrPrompt *prompt,
                                  GAsyncResult *result,
                                  GError **error)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (prompt), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, prompt), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, gcr_system_prompt_confirm_async), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return g_task_propagate_int (G_TASK (result), error);
}

static void
gcr_system_prompt_real_close (GcrPrompt *prompt)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (prompt);

	/*
	 * Setting this before calling close_async allows us to prevent firing
	 * this signal again in a loop.
	 */

	if (!self->pv->closed) {
		self->pv->closed = TRUE;
		perform_close (self, NULL);
	}
}

static void
gcr_system_prompt_prompt_iface (GcrPromptInterface *iface)
{
	iface->prompt_password_async = gcr_system_prompt_password_async;
	iface->prompt_password_finish = gcr_system_prompt_password_finish;
	iface->prompt_confirm_async = gcr_system_prompt_confirm_async;
	iface->prompt_confirm_finish = gcr_system_prompt_confirm_finish;
	iface->prompt_close = gcr_system_prompt_real_close;
}

/**
 * gcr_system_prompt_open_async:
 * @timeout_seconds: the number of seconds to wait to access the prompt, or -1
 * @cancellable: optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass the callback
 *
 * Asynchronously open a system prompt with the default system prompter.
 *
 * Most system prompters only allow showing one prompt at a time, and if
 * another prompt is shown then this method will block for up to
 * @timeout_seconds seconds. If @timeout_seconds is equal to -1, then this
 * will block indefinitely until the prompt can be opened. If @timeout_seconds
 * expires, then this operation will fail with a %GCR_SYSTEM_PROMPT_IN_PROGRESS
 * error.
 */
void
gcr_system_prompt_open_async (gint timeout_seconds,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	g_return_if_fail (timeout_seconds >= -1);
	g_return_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable));

	gcr_system_prompt_open_for_prompter_async (NULL, timeout_seconds,
	                                           cancellable, callback,
	                                           user_data);
}

/**
 * gcr_system_prompt_open_for_prompter_async: (finish-func gcr_system_prompt_open_finish):
 * @prompter_name: (nullable): the prompter D-Bus name
 * @timeout_seconds: the number of seconds to wait to access the prompt, or -1
 * @cancellable: (nullable): optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass the callback
 *
 * Opens a system prompt asynchronously. If prompter_name is %NULL, then the
 * default system prompter is used.
 *
 * Most system prompters only allow showing one prompt at a time, and if
 * another prompt is shown then this method will block for up to
 * @timeout_seconds seconds. If @timeout_seconds is equal to -1, then this
 * will block indefinitely until the prompt can be opened. If @timeout_seconds
 * expires, then this operation will fail with a %GCR_SYSTEM_PROMPT_IN_PROGRESS
 * error.
 */
void
gcr_system_prompt_open_for_prompter_async (const gchar *prompter_name,
                                           gint timeout_seconds,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
	g_return_if_fail (timeout_seconds >= -1);
	g_return_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable));

	if (prompter_name == NULL)
		g_debug ("opening prompt");
	else
		g_debug ("opening prompt for prompter: %s", prompter_name);

	g_async_initable_new_async (GCR_TYPE_SYSTEM_PROMPT,
	                            G_PRIORITY_DEFAULT,
	                            cancellable,
	                            callback,
	                            user_data,
	                            "timeout-seconds", timeout_seconds,
	                            "bus-name", prompter_name,
	                            NULL);
}

/**
 * gcr_system_prompt_open_finish:
 * @result: the asynchronous result
 * @error: location to place an error on failure
 *
 * Complete an operation to asynchronously open a system prompt.
 *
 * Returns: (transfer full) (type Gcr.SystemPrompt): the prompt, or %NULL if
 *          prompt could not be opened
 */
GcrPrompt *
gcr_system_prompt_open_finish (GAsyncResult *result,
                               GError **error)
{
	GObject *object;
	GObject *source_object;

	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	source_object = g_async_result_get_source_object (result);
	g_assert (source_object != NULL);

	object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
	                                      result, error);
	g_object_unref (source_object);

	if (object != NULL)
		return GCR_PROMPT (object);
	else
		return NULL;
}

/**
 * gcr_system_prompt_open:
 * @timeout_seconds: the number of seconds to wait to access the prompt, or -1
 * @cancellable: (nullable): optional cancellation object
 * @error: location to place error on failure
 *
 * Opens a system prompt with the default prompter.
 *
 * Most system prompters only allow showing one prompt at a time, and if
 * another prompt is shown then this method will block for up to
 * @timeout_seconds seconds. If @timeout_seconds is equal to -1, then this
 * will block indefinitely until the prompt can be opened. If @timeout_seconds
 * expires, then this function will fail with a %GCR_SYSTEM_PROMPT_IN_PROGRESS
 * error.
 *
 * Returns: (transfer full) (type Gcr.SystemPrompt): the prompt, or %NULL if
 *          prompt could not be opened
 */
GcrPrompt *
gcr_system_prompt_open (gint timeout_seconds,
                        GCancellable *cancellable,
                        GError **error)
{
	g_return_val_if_fail (timeout_seconds >= -1, NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	return gcr_system_prompt_open_for_prompter (NULL, timeout_seconds,
	                                            cancellable, error);
}

/**
 * gcr_system_prompt_open_for_prompter:
 * @prompter_name: (nullable): the prompter dbus name
 * @timeout_seconds: the number of seconds to wait to access the prompt, or -1
 * @cancellable: optional cancellation object
 * @error: location to place error on failure
 *
 * Opens a system prompt. If prompter_name is %NULL, then the default
 * system prompter is used.
 *
 * Most system prompters only allow showing one prompt at a time, and if
 * another prompt is shown then this method will block for up to
 * @timeout_seconds seconds. If @timeout_seconds is equal to -1, then this
 * will block indefinitely until the prompt can be opened. If @timeout_seconds
 * expires, then this function will fail with a %GCR_SYSTEM_PROMPT_IN_PROGRESS
 * error.
 *
 * Returns: (transfer full) (type Gcr.SystemPrompt): the prompt, or %NULL if
 *          prompt could not be opened
 */
GcrPrompt *
gcr_system_prompt_open_for_prompter (const gchar *prompter_name,
                                     gint timeout_seconds,
                                     GCancellable *cancellable,
                                     GError **error)
{
	g_return_val_if_fail (timeout_seconds >= -1, NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (prompter_name == NULL)
		g_debug ("opening prompt");
	else
		g_debug ("opening prompt for prompter: %s", prompter_name);

	return g_initable_new (GCR_TYPE_SYSTEM_PROMPT, cancellable, error,
	                       "timeout-seconds", timeout_seconds,
	                       "bus-name", prompter_name,
	                       NULL);
}

/**
 * gcr_system_prompt_close:
 * @self: the prompt
 * @cancellable: an optional cancellation object
 * @error: location to place an error on failure
 *
 * Close this prompt. After calling this function, no further prompts will
 * succeed on this object. The prompt object is not unreferenced by this
 * function, and you must unreference it once done.
 *
 * This call may block, use the gcr_system_prompt_close_async() to perform
 * this action indefinitely.
 *
 * Whether or not this function returns %TRUE, the system prompt object is
 * still closed and may not be further used.
 *
 * Returns: whether close was cleanly completed
 */
gboolean
gcr_system_prompt_close (GcrSystemPrompt *self,
                         GCancellable *cancellable,
                         GError **error)
{
	SyncClosure *closure;
	gboolean result;

	closure = sync_closure_new ();
	g_main_context_push_thread_default (closure->context);

	gcr_system_prompt_close_async (self, cancellable,
	                               on_sync_result, closure);

	g_main_loop_run (closure->loop);

	result = gcr_system_prompt_close_finish (self, closure->result, error);

	g_main_context_pop_thread_default (closure->context);
	sync_closure_free (closure);

	return result;
}

/**
 * gcr_system_prompt_close_async:
 * @self: the prompt
 * @cancellable: an optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Close this prompt asynchronously. After calling this function, no further
 * methods may be called on this object. The prompt object is not unreferenced
 * by this function, and you must unreference it once done.
 *
 * This call returns immediately and completes asynchronously.
 */
void
gcr_system_prompt_close_async (GcrSystemPrompt *self,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GTask *task;
	CallClosure *closure;

	g_return_if_fail (GCR_SYSTEM_PROMPT (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	closure = call_closure_new (TRUE);
	task = g_task_new (NULL, get_sub_cancellable (cancellable), callback, user_data);
	g_task_set_source_tag (task, gcr_system_prompt_close_async);
	g_task_set_task_data (task, closure, call_closure_free);

	perform_close (self, g_steal_pointer (&task));
}

/**
 * gcr_system_prompt_close_finish:
 * @self: the prompt
 * @result: asynchronous operation result
 * @error: location to place an error on failure
 *
 * Complete operation to close this prompt.
 *
 * Whether or not this function returns %TRUE, the system prompt object is
 * still closed and may not be further used.
 *
 * Returns: whether close was cleanly completed
 */
gboolean
gcr_system_prompt_close_finish (GcrSystemPrompt *self,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, gcr_system_prompt_close_async), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static const GDBusErrorEntry SYSTEM_PROMPT_ERRORS[] = {
	{ GCR_SYSTEM_PROMPT_IN_PROGRESS, GCR_DBUS_PROMPT_ERROR_IN_PROGRESS },
};

GQuark
gcr_system_prompt_error_get_domain (void)
{
	static size_t quark_volatile = 0;
	g_dbus_error_register_error_domain ("gcr-system-prompt-error-domain",
	                                    &quark_volatile,
	                                    SYSTEM_PROMPT_ERRORS,
	                                    G_N_ELEMENTS (SYSTEM_PROMPT_ERRORS));
	G_STATIC_ASSERT (G_N_ELEMENTS (SYSTEM_PROMPT_ERRORS) == GCR_SYSTEM_PROMPT_IN_PROGRESS);
	return (GQuark) quark_volatile;
}
