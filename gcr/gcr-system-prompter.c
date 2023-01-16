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
#include "gcr-system-prompter.h"
#include "gcr-system-prompt.h"

#include "gcr/gcr-dbus-generated.h"
#include "gcr/gcr-enum-types.h"
#include "gcr/gcr-marshal.h"

#include "egg/egg-error.h"

#include <string.h>

/**
 * GcrSystemPrompter:
 *
 * A prompter used by implementations of system prompts.
 *
 * This is a D-Bus service which is rarely implemented. Use [class@SystemPrompt]
 * to display system prompts.
 *
 * The system prompter service responds to D-Bus requests to create system
 * prompts and creates #GcrPrompt type objects to display those prompts.
 *
 * Pass the GType of the implementation of [iface@Prompt] to
 * [ctor@SystemPrompter.new].
 */

/**
 * GcrSystemPrompterClass:
 * @parent_class: parent class
 * @new_prompt: default handler for the #GcrSystemPrompter::new-prompt signal
 *
 * The class for #GcrSystemPrompter.
 */

/**
 * GcrSystemPrompterMode:
 * @GCR_SYSTEM_PROMPTER_SINGLE: only one prompt shown at a time
 * @GCR_SYSTEM_PROMPTER_MULTIPLE: more than one prompt shown at a time
 *
 * The mode for the system prompter. Most system prompters can only show
 * one prompt at a time and would use the %GCR_SYSTEM_PROMPTER_SINGLE mode.
 */

enum {
	PROP_0,
	PROP_MODE,
	PROP_PROMPT_TYPE,
	PROP_PROMPTING
};

enum {
	NEW_PROMPT,
	LAST_SIGNAL,
};

struct _GcrSystemPrompterPrivate {
	GcrSystemPrompterMode mode;
	GType prompt_type;

	guint prompter_registered;
	GDBusConnection *connection;

	GHashTable *callbacks;       /* Callback -> guint watch_id */
	GHashTable *active;          /* Callback -> active (ActivePrompt) */
	GQueue waiting;
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GcrSystemPrompter, gcr_system_prompter, G_TYPE_OBJECT);

typedef struct {
	const gchar *path;
	const gchar *name;
} Callback;

typedef struct {
	gint refs;
	Callback *callback;
	GcrSystemPrompter *prompter;
	GCancellable *cancellable;
	GcrPrompt *prompt;
	gboolean ready;
	guint notify_sig;
	GHashTable *changed;
	GcrSecretExchange *exchange;
	gboolean received;
	gboolean closed;
	guint close_sig;
} ActivePrompt;

static void    prompt_send_ready               (ActivePrompt *active,
                                                const gchar *response,
                                                const gchar *secret);

static void    prompt_next_ready               (GcrSystemPrompter *self);

static void    prompt_stop_prompting           (GcrSystemPrompter *self,
                                                Callback *callback,
                                                gboolean send_done_message,
                                                gboolean wait_for_reply);

static ActivePrompt *
active_prompt_ref (ActivePrompt *active)
{
	g_atomic_int_inc (&active->refs);
	return active;
}

static void
on_prompt_notify (GObject *object,
                  GParamSpec *param,
                  gpointer user_data)
{
	ActivePrompt *active = user_data;
	gpointer key = (gpointer)g_intern_string (param->name);
	g_hash_table_replace (active->changed, key, key);
}

static void
on_prompt_close (GcrPrompt *prompt,
                 gpointer user_data)
{
	ActivePrompt *active = user_data;
	prompt_stop_prompting (active->prompter, active->callback, TRUE, FALSE);
}

static Callback *
callback_dup (Callback *original)
{
	Callback *callback = g_new0 (Callback, 1);
	g_assert (original != NULL);
	g_assert (original->path != NULL);
	g_assert (original->name != NULL);
	callback->path = g_strdup (original->path);
	callback->name = g_strdup (original->name);
	return callback;
}

static void
callback_free (gpointer data)
{
	Callback *callback = data;
	g_free ((gchar *)callback->path);
	g_free ((gchar *)callback->name);
	g_free (callback);
}

static guint
callback_hash (gconstpointer data)
{
	const Callback *callback = data;
	return g_str_hash (callback->name) ^ g_str_hash (callback->path);
}

static gboolean
callback_equal (gconstpointer one,
                gconstpointer two)
{
	const Callback *cone = one;
	const Callback *ctwo = two;
	return g_str_equal (cone->name, ctwo->name) &&
	       g_str_equal (cone->path, ctwo->path);
}

static void
active_prompt_unref (gpointer data)
{
	ActivePrompt *active = data;

	if (g_atomic_int_dec_and_test (&active->refs)) {
		callback_free (active->callback);
		g_object_unref (active->prompter);
		g_object_unref (active->cancellable);
		if (g_signal_handler_is_connected (active->prompt, active->notify_sig))
			g_signal_handler_disconnect (active->prompt, active->notify_sig);
		if (g_signal_handler_is_connected (active->prompt, active->close_sig))
			g_signal_handler_disconnect (active->prompt, active->close_sig);
		g_object_unref (active->prompt);
		g_hash_table_destroy (active->changed);
		if (active->exchange)
			g_object_unref (active->exchange);
		g_free (active);
	}
}

static GcrSecretExchange *
active_prompt_get_secret_exchange (ActivePrompt *active)
{
	if (active->exchange == NULL)
		active->exchange = gcr_secret_exchange_new (NULL);
	return active->exchange;
}

static ActivePrompt *
active_prompt_create (GcrSystemPrompter *self,
                      Callback *lookup)
{
	ActivePrompt *active;

	active = g_new0 (ActivePrompt, 1);
	active->refs = 1;
	active->callback = callback_dup (lookup);
	active->prompter = g_object_ref (self);
	active->cancellable = g_cancellable_new ();
	g_signal_emit (self, signals[NEW_PROMPT], 0, &active->prompt);
	g_return_val_if_fail (active->prompt != NULL, NULL);

	active->notify_sig = g_signal_connect (active->prompt, "notify", G_CALLBACK (on_prompt_notify), active);
	active->close_sig = g_signal_connect (active->prompt, "prompt-close", G_CALLBACK (on_prompt_close), active);
	active->changed = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* Insert us into the active hash table */
	g_hash_table_replace (self->pv->active, active->callback, active);
	return active;
}

static void
unwatch_name (gpointer data)
{
	g_bus_unwatch_name (GPOINTER_TO_UINT (data));
}

static void
gcr_system_prompter_init (GcrSystemPrompter *self)
{
	self->pv = gcr_system_prompter_get_instance_private (self);
	self->pv->callbacks = g_hash_table_new_full (callback_hash, callback_equal, callback_free, unwatch_name);
	self->pv->active = g_hash_table_new_full (callback_hash, callback_equal, NULL, active_prompt_unref);
}

static void
gcr_system_prompter_set_property (GObject *obj,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (obj);

	switch (prop_id) {
	case PROP_MODE:
		self->pv->mode = g_value_get_enum (value);
		break;
	case PROP_PROMPT_TYPE:
		self->pv->prompt_type = g_value_get_gtype (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_system_prompter_get_property (GObject *obj,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (obj);

	switch (prop_id) {
	case PROP_MODE:
		g_value_set_enum (value, gcr_system_prompter_get_mode (self));
		break;
	case PROP_PROMPT_TYPE:
		g_value_set_gtype (value, gcr_system_prompter_get_prompt_type (self));
		break;
	case PROP_PROMPTING:
		g_value_set_boolean (value, gcr_system_prompter_get_prompting (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_system_prompter_dispose (GObject *obj)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (obj);

	g_debug ("disposing prompter");

	if (self->pv->prompter_registered)
		gcr_system_prompter_unregister (self, FALSE);

	g_hash_table_remove_all (self->pv->callbacks);
	g_hash_table_remove_all (self->pv->active);
	g_object_notify (obj, "prompting");

	G_OBJECT_CLASS (gcr_system_prompter_parent_class)->dispose (obj);
}

static void
gcr_system_prompter_finalize (GObject *obj)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (obj);

	g_debug ("finalizing prompter");

	g_assert (self->pv->connection == NULL);
	g_assert (self->pv->prompter_registered == 0);

	g_hash_table_destroy (self->pv->callbacks);
	g_hash_table_destroy (self->pv->active);

	G_OBJECT_CLASS (gcr_system_prompter_parent_class)->finalize (obj);
}

static GcrPrompt *
gcr_system_prompter_new_prompt (GcrSystemPrompter *self)
{
	g_return_val_if_fail (self->pv->prompt_type != 0, NULL);

	g_debug ("creating new %s prompt", g_type_name (self->pv->prompt_type));

	return g_object_new (self->pv->prompt_type, NULL);
}

static gboolean
gcr_system_prompter_new_prompt_acculmulator (GSignalInvocationHint *ihint,
                                             GValue *return_accu,
                                             const GValue *handler_return,
                                             gpointer user_data)
{
	if (g_value_get_object (handler_return) != NULL) {
		g_value_copy (handler_return, return_accu);
		return FALSE;
	}

	return TRUE;
}

static void
gcr_system_prompter_class_init (GcrSystemPrompterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_system_prompter_get_property;
	gobject_class->set_property = gcr_system_prompter_set_property;
	gobject_class->dispose = gcr_system_prompter_dispose;
	gobject_class->finalize = gcr_system_prompter_finalize;

	klass->new_prompt = gcr_system_prompter_new_prompt;

	/**
	 * GcrSystemPrompter:mode:
	 *
	 * The mode for this prompter.
	 *
	 * Most system prompters only display one prompt at a time and therefore
	 * return %GCR_SYSTEM_PROMPTER_SINGLE.
	 */
	g_object_class_install_property (gobject_class, PROP_MODE,
	              g_param_spec_enum ("mode", "Mode", "Prompting mode",
	                                 GCR_TYPE_SYSTEM_PROMPTER_MODE, GCR_SYSTEM_PROMPTER_SINGLE,
	                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GcrSystemPrompter:prompt-type:
	 *
	 * The #GType for prompts created by this prompter. This must be a
	 * #GcrPrompt implementation.
	 */
	g_object_class_install_property (gobject_class, PROP_PROMPT_TYPE,
	             g_param_spec_gtype ("prompt-type", "Prompt GType", "GObject type of prompts",
	                                 GCR_TYPE_PROMPT,
	                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GcrSystemPrompter:prompting:
	 *
	 * Whether the prompter is prompting or not.
	 */
	g_object_class_install_property (gobject_class, PROP_PROMPTING,
	           g_param_spec_boolean ("prompting", "Prompting", "Whether prompting or not",
	                                 FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GcrSystemPrompter::new-prompt:
	 *
	 * Signal emitted to create a new prompt when needed.
	 *
	 * The default implementation of this signal creates a prompt of the type
	 * gcr_system_prompter_get_prompt_type().
	 *
	 * Returns: (transfer full): the new prompt
	 */
	signals[NEW_PROMPT] = g_signal_new ("new-prompt", GCR_TYPE_SYSTEM_PROMPTER, G_SIGNAL_RUN_LAST,
	                                    G_STRUCT_OFFSET (GcrSystemPrompterClass, new_prompt),
	                                    gcr_system_prompter_new_prompt_acculmulator, NULL,
	                                    _gcr_marshal_OBJECT__VOID,
	                                    GCR_TYPE_PROMPT, 0, G_TYPE_NONE);
}

static GVariantBuilder *
prompt_build_properties (GcrPrompt *prompt,
                         GHashTable *changed)
{
	GObject *obj = G_OBJECT (prompt);
	GVariantBuilder *builder;
	const gchar *property_name;
	GParamSpec *pspec;
	GHashTableIter iter;
	const GVariantType *type;
	GVariant *variant;
	GValue value;

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_hash_table_iter_init (&iter, changed);
	while (g_hash_table_iter_next (&iter, (gpointer *)&property_name, NULL)) {

		/* Make sure this property is on the prompt interface */
		pspec = g_object_interface_find_property (GCR_PROMPT_GET_IFACE (obj),
		                                          property_name);
		if (pspec == NULL)
			continue;

		memset (&value, 0, sizeof (GValue));
		g_value_init (&value, pspec->value_type);
		g_object_get_property (obj, property_name, &value);

		switch (pspec->value_type) {
		case G_TYPE_STRING:
			type = G_VARIANT_TYPE ("s");
			break;
		case G_TYPE_INT:
			type = G_VARIANT_TYPE ("i");
			break;
		case G_TYPE_BOOLEAN:
			type = G_VARIANT_TYPE ("b");
			break;
		default:
			g_critical ("encountered unsupported property type on GcrPrompt: %s",
			            g_type_name (pspec->value_type));
			continue;
		}

		variant = g_dbus_gvalue_to_gvariant (&value, type);
		g_variant_builder_add (builder, "{sv}", property_name,
		                       g_variant_new_variant (variant));
		g_value_unset (&value);
		g_variant_unref (variant);
	}
	g_hash_table_remove_all (changed);
	return builder;
}

static void
prompt_stop_prompting (GcrSystemPrompter *self,
                       Callback *callback,
                       gboolean send_done_message,
                       gboolean wait_for_reply)
{
	ActivePrompt *active;
	GVariant *retval;
	gpointer watch;
	Callback *orig_callback;

	g_debug ("stopping prompting for operation %s@%s",
	         callback->path, callback->name);

	/* Get a pointer to our actual callback */
	if (!g_hash_table_lookup_extended (self->pv->callbacks, callback,
	                                   (gpointer *)&orig_callback, &watch)) {
		g_debug ("couldn't find the callback for prompting operation %s@%s",
		         callback->path, callback->name);
		return;
	}
	callback = orig_callback;

	/*
	 * We remove these from the callbacks hash table so that we don't
	 * do this stuff more than once. However we still need the callback
	 * to be valid.
	 */
	if (!g_hash_table_steal (self->pv->callbacks, callback))
		g_assert_not_reached ();

	/* Removed from the waiting queue */
	g_queue_remove (&self->pv->waiting, callback);

	/* Close any active prompt */
	active = g_hash_table_lookup (self->pv->active, callback);
	if (active != NULL) {
		active_prompt_ref (active);
		g_hash_table_remove (self->pv->active, callback);

		if (!active->ready) {
			g_debug ("cancelling active prompting operation for %s@%s",
			         callback->path, callback->name);
			g_cancellable_cancel (active->cancellable);
		}

		g_debug ("closing the prompt");
		gcr_prompt_close (active->prompt);
		g_object_run_dispose (G_OBJECT (active->prompt));
		active_prompt_unref (active);
	}

	/* Notify the caller */
	if (send_done_message && wait_for_reply) {
		g_debug ("calling the %s method on %s@%s, and waiting for reply",
		         GCR_DBUS_CALLBACK_METHOD_DONE, callback->path, callback->name);

		retval = g_dbus_connection_call_sync (self->pv->connection,
		                                      callback->name,
		                                      callback->path,
		                                      GCR_DBUS_CALLBACK_INTERFACE,
		                                      GCR_DBUS_CALLBACK_METHOD_DONE,
		                                      g_variant_new ("()"),
		                                      G_VARIANT_TYPE ("()"),
		                                      G_DBUS_CALL_FLAGS_NO_AUTO_START,
		                                      -1, NULL, NULL);
		if (retval)
			g_variant_unref (retval);

		g_debug ("returned from %s on %s@%s",
		         GCR_DBUS_CALLBACK_METHOD_DONE, callback->path, callback->name);

	} else if (send_done_message) {
		g_debug ("calling the %s method on %s@%s, and ignoring reply",
		         GCR_DBUS_CALLBACK_METHOD_DONE, callback->path, callback->name);

		g_dbus_connection_call (self->pv->connection,
		                        callback->name,
		                        callback->path,
		                        GCR_DBUS_CALLBACK_INTERFACE,
		                        GCR_DBUS_CALLBACK_METHOD_DONE,
		                        g_variant_new ("()"),
		                        G_VARIANT_TYPE ("()"),
		                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
		                        -1, NULL, NULL, NULL);
	}

	/*
	 * And all traces gone, including watch. We stole these values from
	 * the callbacks hashtable above. Now free them
	 */

	callback_free (callback);
	unwatch_name (watch);

	g_object_notify (G_OBJECT (self), "prompting");
}

static void
on_prompt_ready_complete (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
	ActivePrompt *active = user_data;
	GcrSystemPrompter *self = g_object_ref (active->prompter);
	GError *error = NULL;
	GVariant *retval;

	g_assert (active->ready == FALSE);

	g_debug ("returned from the %s method on %s@%s",
	         GCR_DBUS_CALLBACK_METHOD_READY, active->callback->path, active->callback->name);

	active->ready = TRUE;
	retval = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);

	/* Was cancelled, prompter probably unregistered */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
	    g_cancellable_is_cancelled (active->cancellable)) {
		g_error_free (error);

	/* The ready call failed,  */
	} else if (error != NULL) {
		if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD))
			g_debug ("prompt %s@%s disappeared or does not exist",
			         active->callback->path, active->callback->name);
		else
			g_message ("received an error from the prompt callback: %s", error->message);
		g_error_free (error);

		prompt_stop_prompting (self, active->callback, FALSE, FALSE);

		/* Another new prompt may be ready to go active? */
		prompt_next_ready (self);
	}

	if (retval != NULL)
		g_variant_unref (retval);

	active_prompt_unref (active);
	g_object_unref (self);
}

static void
prompt_send_ready (ActivePrompt *active,
                   const gchar *response,
                   const gchar *secret)
{
	GcrSystemPrompter *self;
	GVariantBuilder *builder;
	GcrSecretExchange *exchange;
	gchar *sent;

	g_assert (active->ready == FALSE);

	exchange = active_prompt_get_secret_exchange (active);
	if (!active->received) {
		g_return_if_fail (secret == NULL);
		sent = gcr_secret_exchange_begin (exchange);
	} else {
		sent = gcr_secret_exchange_send (exchange, secret, -1);
	}

	self = active->prompter;
	builder = prompt_build_properties (active->prompt, active->changed);

	g_debug ("calling the %s method on %s@%s",
	         GCR_DBUS_CALLBACK_METHOD_READY, active->callback->path, active->callback->name);

	g_dbus_connection_call (self->pv->connection,
	                        active->callback->name,
	                        active->callback->path,
	                        GCR_DBUS_CALLBACK_INTERFACE,
	                        GCR_DBUS_CALLBACK_METHOD_READY,
	                        g_variant_new ("(sa{sv}s)", response, builder, sent),
	                        G_VARIANT_TYPE ("()"),
	                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
	                        -1, active->cancellable,
	                        on_prompt_ready_complete,
	                        active_prompt_ref (active));

	g_variant_builder_unref (builder);
	g_free (sent);
}

static void
prompt_next_ready (GcrSystemPrompter *self)
{
	ActivePrompt *active;
	Callback *callback;

	if (self->pv->mode == GCR_SYSTEM_PROMPTER_SINGLE &&
	    g_hash_table_size (self->pv->active) > 0)
		return;

	callback = g_queue_pop_head (&self->pv->waiting);
	if (callback == NULL)
		return;

	g_debug ("preparing a prompt for callback %s@%s",
	         callback->path, callback->name);

	active = g_hash_table_lookup (self->pv->active, callback);
	g_assert (active == NULL);

	active = active_prompt_create (self, callback);
	g_return_if_fail (active != NULL);

	prompt_send_ready (active, GCR_DBUS_PROMPT_REPLY_NONE, NULL);
}

static void
prompt_update_properties (GcrPrompt *prompt,
                          GVariantIter *iter)
{
	GObject *obj = G_OBJECT (prompt);
	gchar *property_name;
	GVariant *variant;
	GValue value;

	g_object_freeze_notify (obj);
	while (g_variant_iter_loop (iter, "{&sv}", &property_name, &variant)) {
		memset (&value, 0, sizeof (GValue));
		g_dbus_gvariant_to_gvalue (variant, &value);
		g_object_set_property (obj, property_name, &value);
		g_value_unset (&value);
	}
	g_object_thaw_notify (obj);
}

static GVariant *
prompter_get_property (GDBusConnection *connection,
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
prompter_set_property (GDBusConnection *connection,
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

static void
on_caller_vanished (GDBusConnection *connection,
                    const gchar *name,
                    gpointer user_data)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (user_data);
	GQueue queue = G_QUEUE_INIT;
	Callback *callback;
	GHashTableIter iter;

	g_hash_table_iter_init (&iter, self->pv->callbacks);
	while (g_hash_table_iter_next (&iter, (gpointer *)&callback, NULL)) {
		if (g_strcmp0 (name, callback->name) == 0)
			g_queue_push_tail (&queue, callback);
	}

	while ((callback = g_queue_pop_head (&queue)) != NULL) {
		g_debug ("caller vanished for callback %s@%s",
		         callback->path, callback->name);
		prompt_stop_prompting (self, callback, FALSE, FALSE);
	}
}

static void
prompter_method_begin_prompting (GcrSystemPrompter *self,
                                 GDBusMethodInvocation *invocation,
                                 GVariant *parameters)
{
	Callback lookup;
	Callback *callback;
	const gchar *caller;
	guint watch_id;

	lookup.name = caller = g_dbus_method_invocation_get_sender (invocation);
	g_variant_get (parameters, "(&o)", &lookup.path);

	g_debug ("received %s call from callback %s@%s",
	         GCR_DBUS_PROMPTER_METHOD_BEGIN,
	         lookup.path, lookup.name);

	if (g_hash_table_lookup (self->pv->callbacks, &lookup)) {
		g_debug ("already begun prompting for callback %s@%s",
		         lookup.path, lookup.name);
		g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
		                                               "Already begun prompting for this prompt callback");
		return;
	}

	callback = callback_dup (&lookup);
	watch_id = g_bus_watch_name_on_connection (self->pv->connection, caller,
	                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
	                                           NULL, on_caller_vanished,
	                                           self, NULL);
	g_hash_table_insert (self->pv->callbacks, callback, GUINT_TO_POINTER (watch_id));

	g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));

	g_queue_push_tail (&self->pv->waiting, callback);
	g_object_notify (G_OBJECT (self), "prompting");

	prompt_next_ready (self);
}

static void
on_prompt_password (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	ActivePrompt *active = user_data;
	const gchar *reply;
	GError *error = NULL;
	const gchar *response;

	g_assert (active->ready == FALSE);
	g_assert (active->callback != NULL);

	g_debug ("completed password prompt for callback %s@%s",
	         active->callback->name, active->callback->path);

	reply = gcr_prompt_password_finish (GCR_PROMPT (source), result, &error);
	if (error != NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("prompting failed: %s", error->message);
		g_clear_error (&error);
	}

	if (reply == NULL)
		response = "no";
	else
		response = "yes";

	prompt_send_ready (active, response, reply);
	active_prompt_unref (active);
}

static void
on_prompt_confirm (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
	ActivePrompt *active = user_data;
	GcrPromptReply reply;
	GError *error = NULL;
	const gchar *response;

	g_assert (active->ready == FALSE);
	g_assert (active->callback != NULL);

	g_debug ("completed confirm prompt for callback %s@%s",
	         active->callback->name, active->callback->path);

	reply = gcr_prompt_confirm_finish (GCR_PROMPT (source), result, &error);
	if (error != NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("prompting failed: %s", error->message);
		g_clear_error (&error);
	}

	switch (reply) {
	case GCR_PROMPT_REPLY_CONTINUE:
		response = GCR_DBUS_PROMPT_REPLY_YES;
		break;
	case GCR_PROMPT_REPLY_CANCEL:
		response = GCR_DBUS_PROMPT_REPLY_NO;
		break;
	default:
		response = GCR_DBUS_PROMPT_REPLY_NONE;
		g_warn_if_reached ();
		break;
	}

	prompt_send_ready (active, response, NULL);
	active_prompt_unref (active);
}

static void
prompter_method_perform_prompt (GcrSystemPrompter *self,
                                GDBusMethodInvocation *invocation,
                                GVariant *parameters)
{
	GcrSecretExchange *exchange;
	GError *error = NULL;
	ActivePrompt *active;
	Callback lookup;
	const gchar *type;
	GVariantIter *iter;
	const gchar *received;

	lookup.name = g_dbus_method_invocation_get_sender (invocation);
	g_variant_get (parameters, "(&o&sa{sv}&s)",
	               &lookup.path, &type, &iter, &received);

	g_debug ("received %s call from callback %s@%s",
	         GCR_DBUS_PROMPTER_METHOD_PERFORM,
	         lookup.path, lookup.name);

	active = g_hash_table_lookup (self->pv->active, &lookup);
	if (active == NULL) {
		g_debug ("not begun prompting for this callback %s@%s",
		         lookup.path, lookup.name);
		error = g_error_new (G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
		                     "Not begun prompting for this prompt callback");

	} else if (!active->ready) {
		g_debug ("already performing prompt for this callback %s@%s",
		         lookup.path, lookup.name);
		error = g_error_new (G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
		                     "Already performing a prompt for this prompt callback");
	}

	if (error != NULL) {
		g_dbus_method_invocation_take_error (invocation, error);
		g_variant_iter_free (iter);
		return;
	}

	g_assert (active != NULL);
	prompt_update_properties (active->prompt, iter);
	g_variant_iter_free (iter);

	exchange = active_prompt_get_secret_exchange (active);
	if (!gcr_secret_exchange_receive (exchange, received)) {
		g_debug ("received invalid secret exchange from callback %s@%s",
		         lookup.path, lookup.name);
		g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
		                                       "Invalid secret exchange received");
		return;
	}

	active->received = TRUE;

	if (g_strcmp0 (type, GCR_DBUS_PROMPT_TYPE_CONFIRM) == 0) {
		active->ready = FALSE;
		g_debug ("starting confirm prompt for callback %s@%s",
		         lookup.path, lookup.name);
		gcr_prompt_confirm_async (active->prompt, active->cancellable,
		                          on_prompt_confirm, active_prompt_ref (active));

	} else if (g_strcmp0 (type, GCR_DBUS_PROMPT_TYPE_PASSWORD) == 0) {
		active->ready = FALSE;
		g_debug ("starting password prompt for callback %s@%s",
		         lookup.path, lookup.name);
		gcr_prompt_password_async (active->prompt, active->cancellable,
		                           on_prompt_password, active_prompt_ref (active));

	} else {
		g_debug ("invalid type of prompt from callback %s@%s",
		         lookup.path, lookup.name);
		g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
		                                               "Invalid type argument");
		return;
	}

	g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
}

static void
prompter_method_stop_prompting (GcrSystemPrompter *self,
                                GDBusMethodInvocation *invocation,
                                GVariant *parameters)
{
	Callback lookup;

	lookup.name = g_dbus_method_invocation_get_sender (invocation);
	g_variant_get (parameters, "(&o)", &lookup.path);

	g_debug ("received %s call from callback %s@%s",
	         GCR_DBUS_PROMPTER_METHOD_PERFORM,
	         lookup.path, lookup.name);

	prompt_stop_prompting (self, &lookup, TRUE, FALSE);

	g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
	prompt_next_ready (self);
}

static void
prompter_method_call (GDBusConnection *connection,
                      const gchar *sender,
                      const gchar *object_path,
                      const gchar *interface_name,
                      const gchar *method_name,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer user_data)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (user_data);

	g_return_if_fail (method_name != NULL);

	if (g_str_equal (method_name, GCR_DBUS_PROMPTER_METHOD_BEGIN)) {
		prompter_method_begin_prompting (self, invocation, parameters);

	} else if (g_str_equal (method_name, GCR_DBUS_PROMPTER_METHOD_PERFORM)) {
		prompter_method_perform_prompt (self, invocation, parameters);

	} else if (g_str_equal (method_name, GCR_DBUS_PROMPTER_METHOD_STOP)) {
		prompter_method_stop_prompting (self, invocation, parameters);

	} else {
		g_return_if_reached ();
	}
}

static GDBusInterfaceVTable prompter_dbus_vtable = {
	prompter_method_call,
	prompter_get_property,
	prompter_set_property,
};

/**
 * gcr_system_prompter_register:
 * @self: the system prompter
 * @connection: a DBus connection
 *
 * Register this system prompter on the DBus @connection.
 *
 * This makes the prompter available for clients to call. The prompter will
 * remain registered until gcr_system_prompter_unregister() is called, or the
 * prompter is unreferenced.
 */
void
gcr_system_prompter_register (GcrSystemPrompter *self,
                              GDBusConnection *connection)
{
	GError *error = NULL;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));
	g_return_if_fail (G_DBUS_CONNECTION (connection));
	g_return_if_fail (self->pv->prompter_registered == 0);
	g_return_if_fail (self->pv->connection == NULL);

	g_debug ("registering prompter");

	self->pv->connection = g_object_ref (connection);

	self->pv->prompter_registered = g_dbus_connection_register_object (connection,
	                                                                   GCR_DBUS_PROMPTER_OBJECT_PATH,
	                                                                   _gcr_dbus_prompter_interface_info (),
	                                                                   &prompter_dbus_vtable,
	                                                                   self, NULL, &error);
	if (error != NULL) {
		g_warning ("error registering prompter %s", egg_error_message (error));
		g_clear_error (&error);
	}
}

/**
 * gcr_system_prompter_unregister:
 * @self: the system prompter
 * @wait: whether to wait for closing prompts
 *
 * Unregister this system prompter on the DBus @connection.
 *
 * The prompter must have previously been registered with gcr_system_prompter_register().
 *
 * If @wait is set then this function will wait until all prompts have been closed
 * or cancelled. This is usually only used by tests.
 */
void
gcr_system_prompter_unregister (GcrSystemPrompter *self,
                                gboolean wait)
{
	GList *callbacks;
	GList *l;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));
	g_return_if_fail (self->pv->prompter_registered != 0);

	g_debug ("unregistering prompter");

	callbacks = g_hash_table_get_keys (self->pv->callbacks);
	for (l = callbacks; l != NULL; l = g_list_next (l))
		prompt_stop_prompting (self, l->data, TRUE, wait);
	g_list_free (callbacks);

	g_assert (g_hash_table_size (self->pv->active) == 0);
	g_assert (g_hash_table_size (self->pv->callbacks) == 0);
	g_assert (g_queue_is_empty (&self->pv->waiting));

	if (!g_dbus_connection_unregister_object (self->pv->connection, self->pv->prompter_registered))
		g_assert_not_reached ();
	self->pv->prompter_registered = 0;

	g_clear_object (&self->pv->connection);
}

/**
 * gcr_system_prompter_new:
 * @mode: the mode for the prompt
 * @prompt_type: the gobject type for prompts created by this prompter
 *
 * Create a new system prompter service. This prompter won't do anything unless
 * you connect to its signals and show appropriate prompts.
 *
 * If @prompt_type is zero, then the new-prompt signal must be handled and
 * return a valid prompt object implementing the #GcrPrompt interface.
 *
 * If @prompt_type is non-zero then the #GType must implement the #GcrPrompt
 * interface.
 *
 * Returns: (transfer full): a new prompter service
 */
GcrSystemPrompter *
gcr_system_prompter_new (GcrSystemPrompterMode mode,
                         GType prompt_type)
{
	if (prompt_type == 0) {
		return g_object_new (GCR_TYPE_SYSTEM_PROMPTER,
		                     "mode", mode,
		                     NULL);

	} else {
		return g_object_new (GCR_TYPE_SYSTEM_PROMPTER,
		                     "mode", mode,
		                     "prompt-type", prompt_type,
		                     NULL);
	}
}

/**
 * gcr_system_prompter_get_mode:
 * @self: the prompter
 *
 * Get the mode for this prompter.
 *
 * Most system prompters only display one prompt at a time and therefore
 * return %GCR_SYSTEM_PROMPTER_SINGLE.
 *
 * Returns: the prompter mode
 */
GcrSystemPrompterMode
gcr_system_prompter_get_mode (GcrSystemPrompter *self)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), GCR_SYSTEM_PROMPTER_SINGLE);
	return self->pv->mode;
}

/**
 * gcr_system_prompter_get_prompt_type:
 * @self: the prompter
 *
 * Get the #GType for prompts created by this prompter.
 *
 * The returned #GType will be a #GcrPrompt implementation.
 *
 * Returns: the prompt #GType
 */
GType
gcr_system_prompter_get_prompt_type (GcrSystemPrompter *self)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), 0);
	return self->pv->prompt_type;
}

/**
 * gcr_system_prompter_get_prompting:
 * @self: the prompter
 *
 * Get whether prompting or not.
 *
 * Returns: whether prompting or not
 */
gboolean
gcr_system_prompter_get_prompting (GcrSystemPrompter *self)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), FALSE);
	return g_hash_table_size (self->pv->callbacks);
}
