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

#include "gcr-dbus-constants.h"
#include "gcr-dbus-generated.h"
#include "gcr-internal.h"
#include "gcr-library.h"
#include "gcr-secret-exchange.h"
#include "gcr-system-prompt.h"

/**
 * SECTION:gcr-system-prompt
 * @title: GcrSystemPrompt
 * @short_description: XXX
 *
 * XXXX
 */

/**
 * GcrSystemPrompt:
 *
 * XXX
 */

/**
 * GcrSystemPromptClass:
 *
 * The class for #GcrSystemPrompt.
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
	PROP_CALLER_WINDOW
};

struct _GcrSystemPromptPrivate {
	gchar *prompter_bus_name;
	GDBusConnection *connection;
	GcrSecretExchange *exchange;
	GHashTable *properties_to_write;
	GHashTable *property_cache;
	GDBusProxy *prompt_proxy;
	gchar *prompt_path;
	gboolean exchanged;
	gboolean begun_prompting;
	gint timeout_seconds;
};

static void     gcr_system_prompt_initable_iface       (GInitableIface *iface);

static void     gcr_system_prompt_async_initable_iface (GAsyncInitableIface *iface);

static void     perform_init_async                     (GcrSystemPrompt *self,
                                                        GSimpleAsyncResult *res);

G_DEFINE_TYPE_WITH_CODE (GcrSystemPrompt, gcr_system_prompt, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gcr_system_prompt_initable_iface);
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, gcr_system_prompt_async_initable_iface);
);

static void
gcr_system_prompt_init (GcrSystemPrompt *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, GCR_TYPE_SYSTEM_PROMPT,
	                                        GcrSystemPromptPrivate);

	self->pv->timeout_seconds = -1;
	self->pv->properties_to_write = g_hash_table_new (g_str_hash, g_str_equal);
	self->pv->property_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                  NULL, (GDestroyNotify)g_variant_unref);
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
		gcr_system_prompt_set_secret_exchange (self, g_value_get_object (value));
		break;
	case PROP_TIMEOUT_SECONDS:
		self->pv->timeout_seconds = g_value_get_int (value);
		break;
	case PROP_TITLE:
		gcr_system_prompt_set_title (self, g_value_get_string (value));
		break;
	case PROP_MESSAGE:
		gcr_system_prompt_set_message (self, g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		gcr_system_prompt_set_description (self, g_value_get_string (value));
		break;
	case PROP_WARNING:
		gcr_system_prompt_set_warning (self, g_value_get_string (value));
		break;
	case PROP_PASSWORD_NEW:
		gcr_system_prompt_set_password_new (self, g_value_get_boolean (value));
		break;
	case PROP_CHOICE_LABEL:
		gcr_system_prompt_set_choice_label (self, g_value_get_string (value));
		break;
	case PROP_CHOICE_CHOSEN:
		gcr_system_prompt_set_choice_chosen (self, g_value_get_boolean (value));
		break;
	case PROP_CALLER_WINDOW:
		gcr_system_prompt_set_caller_window (self, g_value_get_string (value));
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
		g_value_set_string (value, gcr_system_prompt_get_title (self));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, gcr_system_prompt_get_description (self));
		break;
	case PROP_WARNING:
		g_value_set_string (value, gcr_system_prompt_get_warning (self));
		break;
	case PROP_PASSWORD_NEW:
		g_value_set_boolean (value, gcr_system_prompt_get_password_new (self));
		break;
	case PROP_PASSWORD_STRENGTH:
		g_value_set_int (value, gcr_system_prompt_get_password_strength (self));
		break;
	case PROP_CHOICE_LABEL:
		g_value_set_string (value, gcr_system_prompt_get_choice_label (self));
		break;
	case PROP_CHOICE_CHOSEN:
		g_value_set_boolean (value, gcr_system_prompt_get_choice_chosen (self));
		break;
	case PROP_CALLER_WINDOW:
		g_value_set_string (value, gcr_system_prompt_get_caller_window (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_system_prompt_dispose (GObject *obj)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (obj);

	g_clear_object (&self->pv->exchange);

	if (self->pv->prompt_path) {
		g_dbus_connection_call (self->pv->connection,
		                        self->pv->prompter_bus_name,
		                        GCR_DBUS_PROMPTER_OBJECT_PATH,
		                        GCR_DBUS_PROMPTER_INTERFACE,
		                        GCR_DBUS_PROMPTER_METHOD_FINISH,
		                        g_variant_new ("()"),
		                        NULL, G_DBUS_CALL_FLAGS_NO_AUTO_START,
		                        -1, NULL, NULL, NULL);
		g_free (self->pv->prompt_path);
		self->pv->prompt_path = NULL;
	}

	g_hash_table_remove_all (self->pv->properties_to_write);
	g_hash_table_remove_all (self->pv->property_cache);

	if (self->pv->prompt_proxy) {
		g_object_unref (self->pv->prompt_proxy);
		self->pv->prompt_proxy = NULL;
	}

	g_clear_object (&self->pv->connection);

	G_OBJECT_CLASS (gcr_system_prompt_parent_class)->dispose (obj);
}

static void
gcr_system_prompt_finalize (GObject *obj)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (obj);

	g_hash_table_destroy (self->pv->properties_to_write);
	g_hash_table_destroy (self->pv->property_cache);

	G_OBJECT_CLASS (gcr_system_prompt_parent_class)->finalize (obj);
}

static void
gcr_system_prompt_class_init (GcrSystemPromptClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_system_prompt_get_property;
	gobject_class->set_property = gcr_system_prompt_set_property;
	gobject_class->dispose = gcr_system_prompt_dispose;
	gobject_class->finalize = gcr_system_prompt_finalize;

	g_type_class_add_private (gobject_class, sizeof (GcrSystemPromptPrivate));

	g_object_class_install_property (gobject_class, PROP_BUS_NAME,
	            g_param_spec_string ("bus-name", "Bus name", "Prompter bus name",
	                                 NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_TIMEOUT_SECONDS,
	               g_param_spec_int ("timeout-seconds", "Timeout seconds", "Timeout (in seconds) for opening prompt",
	                                 -1, G_MAXINT, -1, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_SECRET_EXCHANGE,
	            g_param_spec_object ("secret-exchange", "Secret exchange", "Secret exchange for passing passwords",
	                                 GCR_TYPE_SECRET_EXCHANGE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_TITLE,
	            g_param_spec_string ("title", "Title", "Prompt title",
	                                 NULL, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_MESSAGE,
	            g_param_spec_string ("message", "Message", "Prompt message",
	                                 NULL, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
	            g_param_spec_string ("description", "Description", "Prompt description",
	                                 NULL, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_WARNING,
	            g_param_spec_string ("warning", "Warning", "Prompt warning",
	                                 NULL, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_PASSWORD_NEW,
	           g_param_spec_boolean ("password-new", "Password new", "Whether prompting for a new password",
	                                 FALSE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_PASSWORD_STRENGTH,
	               g_param_spec_int ("password-strength", "Password strength", "String of new password",
	                                 0, G_MAXINT, 0, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_CHOICE_LABEL,
	            g_param_spec_string ("choice-label", "Choice label", "Label for prompt choice",
	                                 NULL, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CHOICE_CHOSEN,
	           g_param_spec_boolean ("choice-chosen", "Choice chosen", "Whether prompt choice is chosen",
	                                  FALSE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CALLER_WINDOW,
	            g_param_spec_string ("caller-window", "Caller window", "Window ID of application window requesting prompt",
	                                 NULL, G_PARAM_READWRITE));
}

/**
 * gcr_system_prompt_get_secret_exchange:
 * @self: a prompter
 *
 * Get the current #GcrSecretExchange used to transfer secrets in this prompt.
 *
 * Returns: (transfer none): the secret exchange
 */
GcrSecretExchange *
gcr_system_prompt_get_secret_exchange (GcrSystemPrompt *self)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), NULL);

	if (!self->pv->exchange)
		self->pv->exchange = gcr_secret_exchange_new (NULL);

	return self->pv->exchange;
}

void
gcr_system_prompt_set_secret_exchange (GcrSystemPrompt *self,
                                       GcrSecretExchange *exchange)
{
	g_return_if_fail (GCR_IS_SYSTEM_PROMPT (self));
	g_return_if_fail (GCR_IS_SECRET_EXCHANGE (exchange));

	if (self->pv->exchange) {
		g_warning ("The secret exchange is already in use, and cannot be changed");
		return;
	}

	self->pv->exchange = g_object_ref (exchange);
	g_object_notify (G_OBJECT (self), "secret-exchange");
}

static GVariant *
lookup_property_in_caches (GcrSystemPrompt *self,
                           const gchar *property_name)
{
	GVariant *variant;

	variant = g_hash_table_lookup (self->pv->property_cache, property_name);
	if (variant == NULL && self->pv->prompt_proxy) {
		variant = g_dbus_proxy_get_cached_property (self->pv->prompt_proxy, property_name);
		if (variant != NULL)
			g_hash_table_insert (self->pv->property_cache,
			                     (gpointer)g_intern_string (property_name),
			                     variant);
	}

	return variant;
}

static const gchar *
prompt_get_string_property (GcrSystemPrompt *self,
                            const gchar *property_name)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), NULL);

	variant = lookup_property_in_caches (self, property_name);
	if (variant != NULL)
		return g_variant_get_string (variant, NULL);

	return NULL;
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
	g_hash_table_insert (self->pv->property_cache, key, variant);
	g_hash_table_insert (self->pv->properties_to_write, key, key);
}

static gboolean
prompt_get_boolean_property (GcrSystemPrompt *self,
                             const gchar *property_name)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), FALSE);

	variant = lookup_property_in_caches (self, property_name);
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
	g_hash_table_insert (self->pv->property_cache, key, variant);
	g_hash_table_insert (self->pv->properties_to_write, key, key);
}

const gchar *
gcr_system_prompt_get_title (GcrSystemPrompt *self)
{
	return prompt_get_string_property (self, GCR_DBUS_PROMPT_PROPERTY_TITLE);
}

void
gcr_system_prompt_set_title (GcrSystemPrompt *self,
                             const gchar *title)
{
	prompt_set_string_property (self, GCR_DBUS_PROMPT_PROPERTY_TITLE, title);
	g_object_notify (G_OBJECT (self), "title");
}

const gchar *
gcr_system_prompt_get_message (GcrSystemPrompt *self)
{
	return prompt_get_string_property (self, GCR_DBUS_PROMPT_PROPERTY_MESSAGE);
}

void
gcr_system_prompt_set_message (GcrSystemPrompt *self,
                               const gchar *message)
{
	prompt_set_string_property (self, GCR_DBUS_PROMPT_PROPERTY_MESSAGE, message);
	g_object_notify (G_OBJECT (self), "message");
}


const gchar *
gcr_system_prompt_get_description (GcrSystemPrompt *self)
{
	return prompt_get_string_property (self, GCR_DBUS_PROMPT_PROPERTY_DESCRIPTION);
}

void
gcr_system_prompt_set_description (GcrSystemPrompt *self,
                                   const gchar *description)
{
	prompt_set_string_property (self, GCR_DBUS_PROMPT_PROPERTY_DESCRIPTION, description);
	g_object_notify (G_OBJECT (self), "description");
}

const gchar *
gcr_system_prompt_get_warning (GcrSystemPrompt *self)
{
	return prompt_get_string_property (self, GCR_DBUS_PROMPT_PROPERTY_WARNING);
}

void
gcr_system_prompt_set_warning (GcrSystemPrompt *self,
                               const gchar *warning)
{
	prompt_set_string_property (self, GCR_DBUS_PROMPT_PROPERTY_WARNING, warning);
	g_object_notify (G_OBJECT (self), "warning");
}

gboolean
gcr_system_prompt_get_password_new (GcrSystemPrompt *self)
{
	return prompt_get_boolean_property (self, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_NEW);
}

void
gcr_system_prompt_set_password_new (GcrSystemPrompt *self,
                                    gboolean new_password)
{
	prompt_set_boolean_property (self, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_NEW, new_password);
	g_object_notify (G_OBJECT (self), "password-new");
}

gint
gcr_system_prompt_get_password_strength (GcrSystemPrompt *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), 0);

	variant = lookup_property_in_caches (self, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_STRENGTH);
	if (variant != NULL)
		return g_variant_get_int32 (variant);

	return 0;
}


const gchar *
gcr_system_prompt_get_choice_label (GcrSystemPrompt *self)
{
	return prompt_get_string_property (self, GCR_DBUS_PROMPT_PROPERTY_CHOICE_LABEL);
}

void
gcr_system_prompt_set_choice_label (GcrSystemPrompt *self,
                                    const gchar *label)
{
	prompt_set_string_property (self, GCR_DBUS_PROMPT_PROPERTY_CHOICE_LABEL, label);
	g_object_notify (G_OBJECT (self), "choice-label");
}

gboolean
gcr_system_prompt_get_choice_chosen (GcrSystemPrompt *self)
{
	return prompt_get_boolean_property (self, GCR_DBUS_PROMPT_PROPERTY_CHOICE_CHOSEN);
}

void
gcr_system_prompt_set_choice_chosen (GcrSystemPrompt *self,
                                     gboolean chosen)
{
	prompt_set_boolean_property (self, GCR_DBUS_PROMPT_PROPERTY_CHOICE_CHOSEN, chosen);
	g_object_notify (G_OBJECT (self), "choice-chosen");
}

const gchar *
gcr_system_prompt_get_caller_window (GcrSystemPrompt *self)
{
	return prompt_get_string_property (self, GCR_DBUS_PROMPT_PROPERTY_CALLER_WINDOW);
}

void
gcr_system_prompt_set_caller_window (GcrSystemPrompt *self,
                                     const gchar *window_id)
{
	prompt_set_string_property (self, GCR_DBUS_PROMPT_PROPERTY_CALLER_WINDOW, window_id);
	g_object_notify (G_OBJECT (self), "caller-window");
}

static gboolean
gcr_system_prompt_real_init (GInitable *initable,
                             GCancellable *cancellable,
                             GError **error)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (initable);

	/* 1. Connect to the session bus */
	if (!self->pv->connection) {
		self->pv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
		                                       cancellable, error);
		if (self->pv->connection == NULL)
			return FALSE;
	}

	/* 2. Tell the prompter we want to begin prompting */
	if (!self->pv->prompt_path) {
		GVariant *ret;

		ret = g_dbus_connection_call_sync (self->pv->connection,
		                                   self->pv->prompter_bus_name,
		                                   GCR_DBUS_PROMPTER_OBJECT_PATH,
		                                   GCR_DBUS_PROMPTER_INTERFACE,
		                                   GCR_DBUS_PROMPTER_METHOD_BEGIN,
		                                   g_variant_new ("()"),
		                                   G_VARIANT_TYPE ("(o)"),
		                                   G_DBUS_CALL_FLAGS_NONE,
		                                   -1, cancellable, error);
		if (ret == NULL)
			return FALSE;

		self->pv->begun_prompting = TRUE;
		g_assert (self->pv->prompt_path == NULL);
		g_variant_get (ret, "(o)", &self->pv->prompt_path);
		g_variant_unref (ret);
	}

	/* 3. Create a dbus proxy */
	if (!self->pv->prompt_proxy) {
		self->pv->prompt_proxy = g_dbus_proxy_new_sync (self->pv->connection,
		                                                G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		                                                _gcr_prompter_prompt_interface_info (),
		                                                self->pv->prompter_bus_name,
		                                                self->pv->prompt_path,
		                                                GCR_DBUS_PROMPT_INTERFACE,
		                                                cancellable, error);
		if (self->pv->prompt_proxy == NULL)
			return FALSE;

		g_dbus_proxy_set_default_timeout (self->pv->prompt_proxy, G_MAXINT);
	}

	return TRUE;
}

static void
gcr_system_prompt_initable_iface (GInitableIface *iface)
{
	iface->init = gcr_system_prompt_real_init;
}

typedef struct {
	GCancellable *cancellable;
	guint subscribe_sig;
} InitClosure;

static void
init_closure_free (gpointer data)
{
	InitClosure *closure = data;
	g_clear_object (&closure->cancellable);
	g_free (closure);
}

static void
on_bus_connected (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_async_result_get_source_object (user_data));
	GError *error = NULL;

	g_assert (self->pv->connection == NULL);
	self->pv->connection = g_bus_get_finish (result, &error);

	if (error == NULL) {
		g_return_if_fail (self->pv->connection != NULL);
		perform_init_async (self, res);

	} else {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	}

	g_object_unref (self);
	g_object_unref (res);
}

static void
on_prompter_begin_prompting (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_async_result_get_source_object (user_data));
	GError *error = NULL;
	GVariant *ret;

	ret = g_dbus_connection_call_finish (self->pv->connection, result, &error);

	if (error == NULL) {
		self->pv->begun_prompting = TRUE;
		g_assert (self->pv->prompt_path == NULL);
		g_variant_get (ret, "(o)", &self->pv->prompt_path);
		g_variant_unref (ret);

		g_return_if_fail (self->pv->prompt_path != NULL);
		perform_init_async (self, res);

	} else {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	}

	g_object_unref (self);
	g_object_unref (res);
}

static void
on_prompt_proxy_new (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_async_result_get_source_object (user_data));
	GError *error = NULL;

	g_assert (self->pv->prompt_proxy == NULL);
	self->pv->prompt_proxy = g_dbus_proxy_new_finish (result, &error);

	if (error == NULL) {
		g_return_if_fail (self->pv->prompt_proxy != NULL);
		perform_init_async (self, res);

	} else {
		g_simple_async_result_take_error (res, error);
	}

	g_object_unref (self);
	g_object_unref (res);
}

void
perform_init_async (GcrSystemPrompt *self,
                    GSimpleAsyncResult *res)
{
	InitClosure *closure = g_simple_async_result_get_op_res_gpointer (res);

	/* 1. Connect to the session bus */
	if (!self->pv->connection) {
		g_bus_get (G_BUS_TYPE_SESSION, closure->cancellable,
		           on_bus_connected, g_object_ref (res));

	/* 2. Tell the prompter we want to begin prompting */
	} else if (!self->pv->prompt_path) {
		g_dbus_connection_call (self->pv->connection,
		                        self->pv->prompter_bus_name,
		                        GCR_DBUS_PROMPTER_OBJECT_PATH,
		                        GCR_DBUS_PROMPTER_INTERFACE,
		                        GCR_DBUS_PROMPTER_METHOD_BEGIN,
		                        g_variant_new ("()"),
		                        G_VARIANT_TYPE ("(o)"),
		                        G_DBUS_CALL_FLAGS_NONE,
		                        -1, closure->cancellable,
		                        on_prompter_begin_prompting,
		                        g_object_ref (res));

	/* 3. Create a dbus proxy */
	} else if (!self->pv->prompt_proxy) {
		g_dbus_proxy_new (self->pv->connection,
		                  G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		                  _gcr_prompter_prompt_interface_info (),
		                  self->pv->prompter_bus_name,
		                  self->pv->prompt_path,
		                  GCR_DBUS_PROMPT_INTERFACE,
		                  closure->cancellable,
		                  on_prompt_proxy_new,
		                  g_object_ref (res));

	}

	g_simple_async_result_complete (res);
}

static void
gcr_system_prompt_real_init_async (GAsyncInitable *initable,
                                   int io_priority,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (initable);
	GSimpleAsyncResult *res;
	InitClosure *closure;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 gcr_system_prompt_real_init_async);

	closure = g_new0 (InitClosure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, init_closure_free);

	perform_init_async (self, res);

	g_object_unref (res);

}

static gboolean
gcr_system_prompt_real_init_finish (GAsyncInitable *initable,
                                    GAsyncResult *result,
                                    GError **error)
{
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (initable);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      gcr_system_prompt_real_init_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
gcr_system_prompt_async_initable_iface (GAsyncInitableIface *iface)
{
	iface->init_async = gcr_system_prompt_real_init_async;
	iface->init_finish = gcr_system_prompt_real_init_finish;
}

static GVariant *
parameter_properties (GcrSystemPrompt *self)
{
	GHashTableIter iter;
	GVariantBuilder builder;
	const gchar *property_name;
	GVariant *variant;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssv)"));

	g_hash_table_iter_init (&iter, self->pv->properties_to_write);
	while (g_hash_table_iter_next (&iter, (gpointer *)&property_name, NULL)) {
		variant = g_hash_table_lookup (self->pv->property_cache, property_name);
		if (variant == NULL)
			g_warning ("couldn't find prompt property to write: %s", property_name);
		else
			g_variant_builder_add (&builder, "(ssv)", GCR_DBUS_PROMPT_INTERFACE,
			                       property_name, variant);
	}

	g_hash_table_remove_all (self->pv->properties_to_write);
	return g_variant_builder_end (&builder);
}

static void
return_properties (GcrSystemPrompt *self,
                   GVariant *properties)
{
	GVariantIter *iter;
	GVariant *value;
	gchar *interface;
	gchar *property_name;
	gpointer key;

	g_variant_get (properties, "a(ssv)", &iter);
	while (g_variant_iter_loop (iter, "(ssv)", &interface, &property_name, &value)) {
		key = (gpointer)g_intern_string (property_name);
		if (!g_hash_table_lookup (self->pv->properties_to_write, key)) {
			g_hash_table_insert (self->pv->property_cache,
			                     key, g_variant_ref (value));
		}
	}
	g_variant_iter_free (iter);
}

static GVariant *
parameters_for_password (GcrSystemPrompt *self)
{
	GcrSecretExchange *exchange;
	GVariant *properties;
	GVariant *params;
	gchar *input;

	exchange = gcr_system_prompt_get_secret_exchange (self);

	if (self->pv->exchanged)
		input = gcr_secret_exchange_send (exchange, NULL, 0);
	else
		input = gcr_secret_exchange_begin (exchange);
	self->pv->exchanged = TRUE;

	properties = parameter_properties (self);
	params = g_variant_new ("(@a(ssv)s)", properties, input);
	g_free (input);

	return params;
}

static const gchar *
return_for_password (GcrSystemPrompt *self,
                     GVariant *retval,
                     GError **error)
{
	GcrSecretExchange *exchange;
	GVariant *properties;
	const gchar *ret = NULL;
	gchar *output;

	exchange = gcr_system_prompt_get_secret_exchange (self);
	g_variant_get (retval, "(@a(ssv)s)", &properties, &output);

	return_properties (self, properties);

	if (output && output[0]) {
		if (!gcr_secret_exchange_receive (exchange, output)) {
			g_set_error (error, GCR_SYSTEM_PROMPT_ERROR,
			             GCR_SYSTEM_PROMPT_FAILED, "Invalid secret exchanged");
		} else {
			ret = gcr_secret_exchange_get_secret (exchange, NULL);
		}
	}

	g_variant_unref (properties);
	g_free (output);

	return ret;
}

static void
on_prompt_requested_password (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_async_result_get_source_object (user_data));
	const gchar *password;
	GError *error = NULL;
	GVariant *retval;

	retval = g_dbus_proxy_call_finish (self->pv->prompt_proxy, result, &error);

	if (retval != NULL) {
		password = return_for_password (self, retval, &error);
		g_simple_async_result_set_op_res_gpointer (res, (gpointer)password, NULL);
		g_variant_unref (retval);
	}

	if (error != NULL)
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (self);
	g_object_unref (res);
}

void
gcr_system_prompt_password_async (GcrSystemPrompt *self,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPT (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 gcr_system_prompt_password_async);

	g_dbus_proxy_call (G_DBUS_PROXY (self->pv->prompt_proxy),
	                   GCR_DBUS_PROMPT_METHOD_PASSWORD,
	                   parameters_for_password (self),
	                   G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, cancellable,
	                   on_prompt_requested_password, g_object_ref (res));

	g_object_unref (res);
}

const gchar *
gcr_system_prompt_password_finish (GcrSystemPrompt *self,
                                   GAsyncResult *result,
                                   GError **error)
{
	GSimpleAsyncResult *res;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      gcr_system_prompt_password_async), FALSE);

	res = G_SIMPLE_ASYNC_RESULT (result);
	if (g_simple_async_result_propagate_error (res, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gpointer (res);
}

const gchar *
gcr_system_prompt_password (GcrSystemPrompt *self,
                            GCancellable *cancellable,
                            GError **error)
{
	GVariant *retval;
	const gchar *ret = NULL;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!self->pv->prompt_proxy) {
		g_warning ("GcrSystemPrompt was not successfully opened");
		return NULL;
	}

	retval = g_dbus_proxy_call_sync (self->pv->prompt_proxy,
	                                 GCR_DBUS_PROMPT_METHOD_PASSWORD,
	                                 parameters_for_password (self),
	                                 G_DBUS_CALL_FLAGS_NO_AUTO_START, -1,
	                                 cancellable, error);

	if (retval != NULL) {
		ret = return_for_password (self, retval, error);
		g_variant_unref (retval);
	}

	return ret;
}

static GVariant *
parameters_for_confirm (GcrSystemPrompt *self)
{
	GVariant *properties;

	properties = parameter_properties (self);
	return g_variant_new ("(@a(ssv))", properties);
}

static gboolean
return_for_confirm (GcrSystemPrompt *self,
                    GVariant *retval,
                    GError **error)
{
	GVariant *properties;
	gboolean confirm = FALSE;

	g_variant_get (retval, "(@a(ssv)b)", &properties, &confirm);

	return_properties (self, properties);

	g_variant_unref (properties);

	return confirm;
}

static void
on_prompt_requested_confirm (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrSystemPrompt *self = GCR_SYSTEM_PROMPT (g_async_result_get_source_object (user_data));
	GError *error = NULL;
	GVariant *retval;
	gboolean ret;

	retval = g_dbus_proxy_call_finish (self->pv->prompt_proxy, result, &error);

	if (retval != NULL) {
		ret = return_for_confirm (self, retval, &error);
		g_simple_async_result_set_op_res_gboolean (res, ret);
	}

	if (error != NULL)
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (self);
	g_object_unref (res);
}

/**
 * gcr_system_prompt_confirm_async:
 * @self: a prompt
 * @cancellable: optional cancellation object
 *
 * xxx
 */
void
gcr_system_prompt_confirm_async (GcrSystemPrompt *self,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPT (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 gcr_system_prompt_confirm_async);

	g_dbus_proxy_call (G_DBUS_PROXY (self->pv->prompt_proxy),
	                   GCR_DBUS_PROMPT_METHOD_CONFIRM,
	                   parameters_for_confirm (self),
	                   G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, cancellable,
	                   on_prompt_requested_confirm, g_object_ref (res));

	g_object_unref (res);
}

gboolean
gcr_system_prompt_confirm_finish (GcrSystemPrompt *self,
                                  GAsyncResult *result,
                                  GError **error)
{
	GSimpleAsyncResult *res;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      gcr_system_prompt_confirm_async), FALSE);

	res = G_SIMPLE_ASYNC_RESULT (result);
	if (g_simple_async_result_propagate_error (res, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (res);
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
gboolean
gcr_system_prompt_confirm (GcrSystemPrompt *self,
                           GCancellable *cancellable,
                           GError **error)
{
	GVariant *retval;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPT (self), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!self->pv->prompt_proxy) {
		g_warning ("GcrSystemPrompt was not successfully opened");
		return FALSE;
	}

	retval = g_dbus_proxy_call_sync (self->pv->prompt_proxy,
	                                 GCR_DBUS_PROMPT_METHOD_CONFIRM,
	                                 parameters_for_confirm (self),
	                                 G_DBUS_CALL_FLAGS_NO_AUTO_START, -1,
	                                 cancellable, error);

	if (retval != NULL) {
		ret = return_for_confirm (self, retval, error);
		g_variant_unref (retval);
	}

	return ret;
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
 * gcr_system_prompt_open_for_prompter_async:
 * @prompter_name: (allow-none): the prompter dbus name
 * @timeout_seconds: the number of seconds to wait to access the prompt, or -1
 * @cancellable: optional cancellation object
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
 * Returns: (transfer full): the prompt, or %NULL if prompt could not
 *          be opened
 */
GcrSystemPrompt *
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
		return GCR_SYSTEM_PROMPT (object);
	else
		return NULL;
}

/**
 * gcr_system_prompt_open:
 * @timeout_seconds: the number of seconds to wait to access the prompt, or -1
 * @cancellable: optional cancellation object
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
 * Returns: (transfer full): the prompt, or %NULL if prompt could not
 *          be opened
 */
GcrSystemPrompt *
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
 * @prompter_name: (allow-none): the prompter dbus name
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
 * Returns: (transfer full): the prompt, or %NULL if prompt could not
 *          be opened
 */
GcrSystemPrompt *
gcr_system_prompt_open_for_prompter (const gchar *prompter_name,
                                     gint timeout_seconds,
                                     GCancellable *cancellable,
                                     GError **error)
{
	g_return_val_if_fail (timeout_seconds >= -1, NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	return g_initable_new (GCR_TYPE_SYSTEM_PROMPT, cancellable, error,
	                       "timeout-seconds", timeout_seconds,
	                       "bus-name", prompter_name,
	                       NULL);
}

/**
 * gcr_system_prompt_close:
 * @self: the prompt
 *
 * Close this prompt. After closing the prompt, no further methods may be
 * called on this object. The prompt object is not unreferenced by this
 * function, and you must unreference it once done.
 */
void
gcr_system_prompt_close (GcrSystemPrompt *self)
{
	g_return_if_fail (GCR_IS_SYSTEM_PROMPT (self));

	g_object_run_dispose (G_OBJECT (self));
}

static const GDBusErrorEntry SYSTEM_PROMPT_ERRORS[] = {
	{ GCR_SYSTEM_PROMPT_IN_PROGRESS, GCR_DBUS_PROMPT_ERROR_IN_PROGRESS },
	{ GCR_SYSTEM_PROMPT_NOT_HAPPENING, GCR_DBUS_PROMPT_ERROR_NOT_HAPPENING },
	{ GCR_SYSTEM_PROMPT_FAILED, GCR_DBUS_PROMPT_ERROR_FAILED },
};

GQuark
gcr_system_prompt_error_get_domain (void)
{
	static volatile gsize quark_volatile = 0;
	g_dbus_error_register_error_domain ("gcr-system-prompt-error-domain",
	                                    &quark_volatile,
	                                    SYSTEM_PROMPT_ERRORS,
	                                    G_N_ELEMENTS (SYSTEM_PROMPT_ERRORS));
	G_STATIC_ASSERT (G_N_ELEMENTS (SYSTEM_PROMPT_ERRORS) == GCR_SYSTEM_PROMPT_FAILED);
	return (GQuark) quark_volatile;
}
