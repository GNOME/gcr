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
#define DEBUG_FLAG GCR_DEBUG_PROMPT
#include "gcr-debug.h"
#include "gcr-internal.h"
#include "gcr-library.h"
#include "gcr-secret-exchange.h"
#include "gcr-system-prompter.h"
#include "gcr-system-prompt.h"

#include "egg/egg-error.h"

#include <string.h>

/**
 * SECTION:gcr-system-prompter
 * @title: GcrSystemPrompter
 * @short_description: XXX
 *
 * XXXX
 */

/**
 * GcrSystemPrompter:
 *
 * XXX
 */

/**
 * GcrSystemPrompterClass:
 *
 * The class for #GcrSystemPrompter.
 */

enum {
	PROP_0,
	PROP_TITLE,
	PROP_MESSAGE,
	PROP_DESCRIPTION,
	PROP_WARNING,
	PROP_PASSWORD_NEW,
	PROP_PASSWORD_STRENGTH,
	PROP_CHOICE_LABEL,
	PROP_CHOICE_CHOSEN,
	PROP_CALLER_WINDOW,
};

enum {
	OPEN,
	PROMPT_PASSWORD,
	PROMPT_CONFIRM,
	RESPONDED,
	CLOSE,
	LAST_SIGNAL
};

struct _GcrSystemPrompterPrivate {
	guint prompter_registered;
	GDBusConnection *connection;

	/* Prompt state */
	gchar *prompt_path;
	guint prompt_registered;
	gchar *owner_name;
	guint owner_watching_id;
	GcrSecretExchange *exchange;
	GDBusMethodInvocation *invocation;
	gboolean opened;

	/* Properties */
	GHashTable *properties;
	GQueue *properties_changed;
};

static gint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GcrSystemPrompter, gcr_system_prompter, G_TYPE_OBJECT);

static GVariant *
build_changed_properties (GcrSystemPrompter *self)
{
	const gchar *property_name;
	GVariantBuilder builder;
	GVariant *value;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssv)"));

	while ((property_name = g_queue_pop_head (self->pv->properties_changed))) {
		value = g_hash_table_lookup (self->pv->properties, property_name);
		g_variant_builder_add (&builder, "(ssv)", GCR_DBUS_PROMPT_INTERFACE, property_name, value);
	}

	return g_variant_builder_end (&builder);
}

static void
prompt_emit_changed (GcrSystemPrompter *self,
                     const gchar *dbus_property)
{
	const gchar *object_property;

	g_assert (dbus_property);

	g_queue_push_tail (self->pv->properties_changed,
	                   (gpointer)g_intern_string (dbus_property));

	if (g_str_equal ("Title", dbus_property))
		object_property = "title";
	else if (g_str_equal ("Message", dbus_property))
		object_property = "message";
	else if (g_str_equal ("Warning", dbus_property))
		object_property = "warning";
	else if (g_str_equal ("Description", dbus_property))
		object_property = "description";
	else if (g_str_equal ("PasswordNew", dbus_property))
		object_property = "password-new";
	else if (g_str_equal ("PasswordStrength", dbus_property))
		object_property = "password-strength";
	else if (g_str_equal ("ChoiceLabel", dbus_property))
		object_property = "choice-label";
	else if (g_str_equal ("ChoiceChosen", dbus_property))
		object_property = "choice-chosen";
	else if (g_str_equal ("CallerWindow", dbus_property))
		object_property = "caller-window";
	else
		g_assert_not_reached ();

	g_object_notify (G_OBJECT (self), object_property);
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
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (user_data);
	GVariant *ret;

	g_return_val_if_fail (property_name != NULL, NULL);

	if (g_strcmp0 (self->pv->owner_name, sender) != 0) {
		g_error_new_literal (G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
		                     "This prompt is owned by another process.");
		return NULL;
	}

	ret = g_hash_table_lookup (self->pv->properties, property_name);
	g_return_val_if_fail (ret != NULL, NULL);

	return g_variant_ref (ret);
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
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (user_data);

	g_return_val_if_fail (property_name != NULL, FALSE);

	if (g_strcmp0 (self->pv->owner_name, sender) != 0) {
		g_error_new_literal (G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
		                     "This prompt is owned by another process.");
		return FALSE;
	}

	if (!g_hash_table_lookup (self->pv->properties, property_name))
		g_return_val_if_reached (FALSE);

	g_hash_table_insert (self->pv->properties,
	                     (gpointer)g_intern_string (property_name),
	                     g_variant_ref (value));

	prompt_emit_changed (self, property_name);
	return TRUE;
}

static void
prompt_update_properties (GcrSystemPrompter *self,
                          GVariant *properties)
{
	GObject *obj = G_OBJECT (self);
	GVariantIter *iter;
	GVariant *variant;
	gchar *property_name;
	gchar *interface;

	g_object_freeze_notify (obj);

	g_variant_get (properties, "a(ssv)", &iter);
	while (g_variant_iter_loop (iter, "(ssv)", &interface, &property_name, &variant)) {
		if (g_strcmp0 (interface, GCR_DBUS_PROMPT_INTERFACE) != 0)
			continue;
		if (g_hash_table_lookup (self->pv->properties, property_name)) {
			g_hash_table_insert (self->pv->properties,
					     (gpointer)g_intern_string (property_name),
					     g_variant_ref (variant));
			prompt_emit_changed (self, property_name);
		}
	}

	g_variant_iter_free (iter);

	g_object_thaw_notify (obj);
}

static void
prompt_clear_property (GcrSystemPrompter *self,
                       const gchar *property_name,
                       GVariant *value)
{
	g_hash_table_insert (self->pv->properties,
	                     (gpointer)g_intern_string (property_name),
	                     g_variant_ref (value));
}

static void
prompt_clear_properties (GcrSystemPrompter *self)
{
	GVariant *variant;

	variant = g_variant_ref_sink (g_variant_new_string (""));
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_TITLE, variant);
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_MESSAGE, variant);
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_WARNING, variant);
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_DESCRIPTION, variant);
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_CHOICE_LABEL, variant);
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_CALLER_WINDOW, variant);
	g_variant_unref (variant);

	variant = g_variant_ref_sink (g_variant_new_boolean (FALSE));
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_NEW, variant);
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_CHOICE_CHOSEN, variant);
	g_variant_unref (variant);

	variant = g_variant_ref_sink (g_variant_new_int32 (0));
	prompt_clear_property (self, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_STRENGTH, variant);
	g_variant_unref (variant);

	g_queue_clear (self->pv->properties_changed);
}

static void
prompt_method_request_password (GcrSystemPrompter *self,
                                GDBusMethodInvocation *invocation,
                                GVariant *properties,
                                const gchar *exchange)
{
	gboolean result;

	if (self->pv->invocation != NULL) {
		g_dbus_method_invocation_return_error_literal (invocation, GCR_SYSTEM_PROMPT_ERROR,
		                                               GCR_SYSTEM_PROMPT_IN_PROGRESS,
		                                               "The prompt is already requesting.");
		return;
	}

	if (!gcr_secret_exchange_receive (self->pv->exchange, exchange)) {
		g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR,
		                                               G_DBUS_ERROR_INVALID_ARGS,
		                                               "The exchange argument was invalid.");
		return;
	}

	prompt_update_properties (self, properties);

	if (!self->pv->opened) {
		self->pv->opened = TRUE;
		_gcr_debug ("prompter opening prompt");
		g_signal_emit (self, signals[OPEN], 0);
	}

	self->pv->invocation = invocation;
	g_signal_emit (self, signals[PROMPT_PASSWORD], 0, &result);

	if (!result) {
		g_return_if_fail (self->pv->invocation == invocation);
		self->pv->invocation = NULL;
		g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR,
		                                               G_DBUS_ERROR_NOT_SUPPORTED,
		                                               "Prompting for confirmation not supported.");
	}
}

static void
prompt_method_request_confirm (GcrSystemPrompter *self,
                               GDBusMethodInvocation *invocation,
                               GVariant *properties)
{
	gboolean result;

	if (self->pv->invocation != NULL) {
		g_dbus_method_invocation_return_error_literal (invocation, GCR_SYSTEM_PROMPT_ERROR,
		                                               GCR_SYSTEM_PROMPT_IN_PROGRESS,
		                                               "The prompt is already requesting.");
		return;
	}

	prompt_update_properties (self, properties);

	if (!self->pv->opened) {
		self->pv->opened = TRUE;
		_gcr_debug ("prompter opening prompt");
		g_signal_emit (self, signals[OPEN], 0);
	}

	self->pv->invocation = invocation;
	g_signal_emit (self, signals[PROMPT_CONFIRM], 0, &result);

	if (!result) {
		g_return_if_fail (self->pv->invocation == invocation);
		self->pv->invocation = NULL;
		g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR,
		                                               G_DBUS_ERROR_NOT_SUPPORTED,
		                                               "Prompting for confirmation not supported.");
	}
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
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (user_data);
	GVariant *properties = NULL;
	gchar *string = NULL;

	g_return_if_fail (method_name != NULL);

	if (g_strcmp0 (self->pv->owner_name, sender) != 0) {
		g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR,
		                                               G_DBUS_ERROR_ACCESS_DENIED,
		                                               "This prompt is owned by another process.");

	} else if (g_str_equal (method_name, "RequestPassword")) {
		g_variant_get (parameters, "(@a(ssv)s)", &properties, &string);
		prompt_method_request_password (self, invocation, properties, string);

	} else if (g_str_equal (method_name, "RequestConfirm")) {
		g_variant_get (parameters, "(@a(ssv))", &properties);
		prompt_method_request_confirm (self, invocation, properties);

	} else {
		g_return_if_reached ();
	}

	if (properties)
		g_variant_unref (properties);
	g_free (string);
}

static GDBusInterfaceVTable prompt_dbus_vtable = {
	prompt_method_call,
	prompt_get_property,
	prompt_set_property,
};

static const gchar *
begin_prompting (GcrSystemPrompter *self,
                 GDBusConnection *connection)
{
	GError *error = NULL;

	g_assert (self->pv->prompt_path == NULL);

	self->pv->prompt_path = g_strdup (GCR_DBUS_PROMPTER_OBJECT_PATH "/prompt0");
	_gcr_debug ("prompter registering prompt: %s", self->pv->prompt_path);
	self->pv->prompt_registered = g_dbus_connection_register_object (connection, self->pv->prompt_path,
	                                                                 _gcr_prompter_prompt_interface_info (),
	                                                                 &prompt_dbus_vtable, self, NULL, &error);

	if (error != NULL) {
		g_warning ("couldn't register prompt object: %s", error->message);
		g_clear_object (&error);
	}

	/* Setup the secret exchange */
	g_assert (self->pv->exchange == NULL);
	self->pv->exchange = gcr_secret_exchange_new (NULL);

	return self->pv->prompt_path;
}

static void
finish_prompting (GcrSystemPrompter *self)
{
	g_assert (self->pv->invocation == NULL);

	prompt_clear_properties (self);

	/* Tell the implementation to hide the display */
	if (self->pv->opened) {
		_gcr_debug ("prompter closing prompt");
		self->pv->opened = FALSE;
		g_signal_emit (self, signals[CLOSE], 0);
	}

	if (self->pv->owner_watching_id) {
		_gcr_debug ("prompter stopping watch of client: %s", self->pv->owner_name);
		g_bus_unwatch_name (self->pv->owner_watching_id);
		self->pv->owner_watching_id = 0;
	}

	g_free (self->pv->owner_name);
	self->pv->owner_name = NULL;

	if (self->pv->prompt_registered) {
		_gcr_debug ("prompter unregistering prompt: %s", self->pv->prompt_path);
		if (!g_dbus_connection_unregister_object (self->pv->connection,
		                                          self->pv->prompt_registered))
			g_return_if_reached ();
		self->pv->prompt_registered = 0;
	}

	g_free (self->pv->prompt_path);
	self->pv->prompt_path = NULL;

	g_clear_object (&self->pv->exchange);
}


static void
gcr_system_prompter_init (GcrSystemPrompter *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, GCR_TYPE_SYSTEM_PROMPTER,
	                                        GcrSystemPrompterPrivate);
	self->pv->properties = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
	                                              (GDestroyNotify)g_variant_unref);
	self->pv->properties_changed = g_queue_new ();
	prompt_clear_properties (self);
}

static void
gcr_system_prompter_set_property (GObject *obj,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (obj);

	switch (prop_id) {
	case PROP_CHOICE_CHOSEN:
		gcr_system_prompter_set_choice_chosen (self, g_value_get_boolean (value));
		break;
	case PROP_PASSWORD_STRENGTH:
		gcr_system_prompter_set_password_strength (self, g_value_get_int (value));
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
	case PROP_TITLE:
		g_value_set_string (value, gcr_system_prompter_get_title (self));
		break;
	case PROP_MESSAGE:
		g_value_set_string (value, gcr_system_prompter_get_message (self));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, gcr_system_prompter_get_description (self));
		break;
	case PROP_WARNING:
		g_value_set_string (value, gcr_system_prompter_get_warning (self));
		break;
	case PROP_PASSWORD_NEW:
		g_value_set_boolean (value, gcr_system_prompter_get_password_new (self));
		break;
	case PROP_PASSWORD_STRENGTH:
		g_value_set_int (value, gcr_system_prompter_get_password_strength (self));
		break;
	case PROP_CHOICE_LABEL:
		g_value_set_string (value, gcr_system_prompter_get_choice_label (self));
		break;
	case PROP_CHOICE_CHOSEN:
		g_value_set_boolean (value, gcr_system_prompter_get_choice_chosen (self));
		break;
	case PROP_CALLER_WINDOW:
		g_value_set_string (value, gcr_system_prompter_get_caller_window (self));
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

	_gcr_debug ("disposing prompter");

	if (self->pv->invocation)
		gcr_system_prompter_respond_cancelled (self);

	finish_prompting (self);

	if (self->pv->prompter_registered)
		gcr_system_prompter_unregister (self, self->pv->connection);

	G_OBJECT_CLASS (gcr_system_prompter_parent_class)->dispose (obj);
}

static void
gcr_system_prompter_finalize (GObject *obj)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (obj);

	_gcr_debug ("finalizing prompter");

	g_hash_table_destroy (self->pv->properties);
	g_queue_free (self->pv->properties_changed);

	G_OBJECT_CLASS (gcr_system_prompter_parent_class)->finalize (obj);
}

static void
gcr_system_prompter_real_show_prompt (GcrSystemPrompter *self)
{

}

static gboolean
gcr_system_prompter_real_prompt_confirm (GcrSystemPrompter *self)
{
	return FALSE;
}

static gboolean
gcr_system_prompter_real_prompt_password (GcrSystemPrompter *self)
{
	return FALSE;
}

static void
gcr_system_prompter_real_hide_prompt (GcrSystemPrompter *self)
{

}

static void
gcr_system_prompter_class_init (GcrSystemPrompterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_system_prompter_get_property;
	gobject_class->set_property = gcr_system_prompter_set_property;
	gobject_class->dispose = gcr_system_prompter_dispose;
	gobject_class->finalize = gcr_system_prompter_finalize;

	klass->open = gcr_system_prompter_real_show_prompt;
	klass->prompt_password= gcr_system_prompter_real_prompt_password;
	klass->prompt_confirm = gcr_system_prompter_real_prompt_confirm;
	klass->close = gcr_system_prompter_real_hide_prompt;

	g_type_class_add_private (gobject_class, sizeof (GcrSystemPrompterPrivate));

	g_object_class_install_property (gobject_class, PROP_TITLE,
	            g_param_spec_string ("title", "Title", "Prompt title",
	                                 "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_MESSAGE,
	            g_param_spec_string ("message", "Message", "Prompt message",
	                                 "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
	            g_param_spec_string ("description", "Description", "Prompt description",
	                                 "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_WARNING,
	            g_param_spec_string ("warning", "Warning", "Prompt warning",
	                                 "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_PASSWORD_NEW,
	           g_param_spec_boolean ("password-new", "Password new", "Whether is a new password",
	                                 FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_PASSWORD_STRENGTH,
	               g_param_spec_int ("password-strength", "Password strength", "Strength of password",
	                                 0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_CHOICE_LABEL,
	            g_param_spec_string ("choice-label", "Choice label", "Label for prompt choice",
	                                 "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_CHOICE_CHOSEN,
	           g_param_spec_boolean ("choice-chosen", "Choice chosen", "Whether choice is chosen",
	                                 FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_CALLER_WINDOW,
	            g_param_spec_string ("caller-window", "Caller window", "Window id of caller",
	                                 "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	signals[OPEN] = g_signal_new ("open", GCR_TYPE_SYSTEM_PROMPTER, G_SIGNAL_RUN_LAST,
	                              G_STRUCT_OFFSET (GcrSystemPrompterClass, open),
	                              NULL, NULL, NULL, G_TYPE_NONE, 0);

	signals[PROMPT_PASSWORD] = g_signal_new ("prompt-password", GCR_TYPE_SYSTEM_PROMPTER, G_SIGNAL_RUN_LAST,
	                                         G_STRUCT_OFFSET (GcrSystemPrompterClass, prompt_password),
	                                         g_signal_accumulator_true_handled, NULL, NULL,
	                                         G_TYPE_BOOLEAN, 0);

	signals[PROMPT_CONFIRM] = g_signal_new ("prompt-confirm", GCR_TYPE_SYSTEM_PROMPTER, G_SIGNAL_RUN_LAST,
	                                        G_STRUCT_OFFSET (GcrSystemPrompterClass, prompt_confirm),
	                                        g_signal_accumulator_true_handled, NULL, NULL,
	                                        G_TYPE_BOOLEAN, 0);

	signals[RESPONDED] = g_signal_new ("responded", GCR_TYPE_SYSTEM_PROMPTER, G_SIGNAL_RUN_LAST,
	                                   G_STRUCT_OFFSET (GcrSystemPrompterClass, responded),
	                                   NULL, NULL, NULL, G_TYPE_NONE, 0);

	signals[CLOSE] = g_signal_new ("close", GCR_TYPE_SYSTEM_PROMPTER, G_SIGNAL_RUN_LAST,
	                               G_STRUCT_OFFSET (GcrSystemPrompterClass, close),
	                               NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
on_owner_vanished (GDBusConnection *connection,
                   const gchar *name,
                   gpointer user_data)
{
	GcrSystemPrompter *self = GCR_SYSTEM_PROMPTER (user_data);

	if (self->pv->invocation != NULL)
		gcr_system_prompter_respond_cancelled (self);

	finish_prompting (self);

	/* Now let everyone else know, we're ready! */
	_gcr_prompter_emit_prompter_ready (GCR_PROMPTER (self));
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
prompter_method_begin_prompting (GcrSystemPrompter *self,
                                 GDBusMethodInvocation *invocation)
{
	GDBusConnection *connection;
	const gchar *path;

	if (self->pv->owner_name != NULL) {
		g_dbus_method_invocation_return_error_literal (invocation,
		                                               GCR_SYSTEM_PROMPT_ERROR,
		                                               GCR_SYSTEM_PROMPT_IN_PROGRESS,
		                                               "There is already a prompt in progress");
		return;
	}

	/* Setup the owner of the prompting */
	self->pv->owner_name = g_strdup (g_dbus_method_invocation_get_sender (invocation));
	g_return_if_fail (self->pv->owner_name != NULL);

	connection = g_dbus_method_invocation_get_connection (invocation);
	_gcr_debug ("prompter starting watch of dbus client: %s", self->pv->owner_name);
	self->pv->owner_watching_id = g_bus_watch_name_on_connection (connection,
	                                                              self->pv->owner_name,
	                                                              G_BUS_NAME_WATCHER_FLAGS_NONE,
	                                                              NULL, on_owner_vanished,
	                                                              self, NULL);

	/* And respond */
	path = begin_prompting (self, connection);
	g_dbus_method_invocation_return_value (invocation, g_variant_new ("(o)", path));
}

static void
prompter_method_finish_prompting (GcrSystemPrompter *self,
                                  GDBusMethodInvocation *invocation,
                                  const gchar *prompt)
{
	if (g_strcmp0 (prompt, self->pv->prompt_path) != 0) {
		_gcr_debug ("caller passed invalid prompt: %s != %s", prompt, self->pv->prompt_path);
		g_dbus_method_invocation_return_error_literal (invocation,
		                                               G_DBUS_ERROR,
		                                               G_DBUS_ERROR_INVALID_ARGS,
		                                               "The prompt argument is not valid");
		return;
	}

	if (self->pv->owner_name == NULL) {
		_gcr_debug ("prompting is not in progress");
		g_dbus_method_invocation_return_error_literal (invocation,
		                                               GCR_SYSTEM_PROMPT_ERROR,
		                                               GCR_SYSTEM_PROMPT_NOT_HAPPENING,
		                                               "The prompt is not in progress");
		return;
	}

	_gcr_debug ("finishing prompting owned by caller %s", self->pv->owner_name);

	/* Close a prompt that's prompting */
	if (self->pv->invocation != NULL)
		gcr_system_prompter_respond_cancelled (self);

	finish_prompting (self);

	g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
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
	gchar *path = NULL;

	g_return_if_fail (method_name != NULL);

	if (g_str_equal (method_name, GCR_DBUS_PROMPTER_METHOD_BEGIN)) {
		prompter_method_begin_prompting (self, invocation);

	} else if (g_str_equal (method_name, GCR_DBUS_PROMPTER_METHOD_FINISH)) {
		g_variant_get (parameters, "(o)", &path);
		prompter_method_finish_prompting (self, invocation, path);

	} else {
		g_return_if_reached ();
	}

	g_free (path);
}

static GDBusInterfaceVTable prompter_dbus_vtable = {
	prompter_method_call,
	prompter_get_property,
	prompter_set_property,
};

void
gcr_system_prompter_register (GcrSystemPrompter *self,
                              GDBusConnection *connection)
{
	GError *error = NULL;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));
	g_return_if_fail (G_DBUS_CONNECTION (connection));
	g_return_if_fail (self->pv->prompter_registered == 0);
	g_return_if_fail (self->pv->connection == NULL);

	_gcr_debug ("registering prompter");

	self->pv->connection = connection;
	g_object_add_weak_pointer (G_OBJECT (connection), (gpointer *)&self->pv->connection);

	self->pv->prompter_registered = g_dbus_connection_register_object (connection,
	                                                                   GCR_DBUS_PROMPTER_OBJECT_PATH,
	                                                                   _gcr_prompter_interface_info (),
	                                                                   &prompter_dbus_vtable,
	                                                                   g_object_ref (self),
	                                                                   g_object_unref,
	                                                                   &error);
	if (error != NULL) {
		g_warning ("error registering prompter %s", egg_error_message (error));
		g_clear_error (&error);
	}
}

void
gcr_system_prompter_unregister (GcrSystemPrompter *self,
                                GDBusConnection *connection)
{
	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));
	g_return_if_fail (G_DBUS_CONNECTION (connection));
	g_return_if_fail (self->pv->prompter_registered != 0);
	g_return_if_fail (self->pv->connection == connection);

	_gcr_debug ("unregistering prompter");

	if (self->pv->invocation)
		gcr_system_prompter_respond_cancelled (self);

	finish_prompting (self);

	g_dbus_connection_unregister_object (connection, self->pv->prompter_registered);
	self->pv->prompter_registered = 0;

	g_object_remove_weak_pointer (G_OBJECT (connection), (gpointer *)&self->pv->connection);
	self->pv->connection = NULL;
}

const gchar *
gcr_system_prompter_get_title (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), NULL);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_TITLE);
	g_return_val_if_fail (variant != NULL, NULL);
	return g_variant_get_string (variant, NULL);
}

const gchar *
gcr_system_prompter_get_message (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), NULL);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_MESSAGE);
	g_return_val_if_fail (variant != NULL, NULL);
	return g_variant_get_string (variant, NULL);
}

const gchar *
gcr_system_prompter_get_description (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), NULL);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_DESCRIPTION);
	g_return_val_if_fail (variant != NULL, NULL);
	return g_variant_get_string (variant, NULL);
}

const gchar *
gcr_system_prompter_get_warning (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), NULL);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_WARNING);
	g_return_val_if_fail (variant != NULL, NULL);
	return g_variant_get_string (variant, NULL);
}

void
gcr_system_prompter_set_warning (GcrSystemPrompter *self,
                                 const gchar *warning)
{
	GVariant *variant;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));

	if (warning == NULL)
		warning = "";

	variant = g_variant_new_string (warning ? warning : "");
	g_hash_table_insert (self->pv->properties,
	                     (gpointer)GCR_DBUS_PROMPT_PROPERTY_WARNING,
	                     g_variant_ref_sink (variant));

	prompt_emit_changed (self, GCR_DBUS_PROMPT_PROPERTY_WARNING);
}

gboolean
gcr_system_prompter_get_password_new (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), FALSE);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_NEW);
	g_return_val_if_fail (variant != NULL, FALSE);
	return g_variant_get_boolean (variant);
}

gint
gcr_system_prompter_get_password_strength (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), 0);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_STRENGTH);
	g_return_val_if_fail (variant != NULL, 0);
	return g_variant_get_int32 (variant);
}

void
gcr_system_prompter_set_password_strength (GcrSystemPrompter *self,
                                           gint strength)
{
	GVariant *variant;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));

	variant = g_variant_new_int32 (strength);
	g_hash_table_insert (self->pv->properties,
	                     (gpointer) GCR_DBUS_PROMPT_PROPERTY_PASSWORD_STRENGTH,
	                     g_variant_ref_sink (variant));

	prompt_emit_changed (self, GCR_DBUS_PROMPT_PROPERTY_PASSWORD_STRENGTH);
}

const gchar *
gcr_system_prompter_get_choice_label (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), NULL);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_CHOICE_LABEL);
	g_return_val_if_fail (variant != NULL, NULL);
	return g_variant_get_string (variant, NULL);
}

/**
 * gcr_system_prompter_get_choice_chosen:
 * @self: a prompter
 *
 * Used by prompter implementations to check if the choice offered by the
 * prompt should be checked or unchecked.
 *
 * Returns: whether the choice is chosen
 */
gboolean
gcr_system_prompter_get_choice_chosen (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), FALSE);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_CHOICE_CHOSEN);
	g_return_val_if_fail (variant != NULL, FALSE);
	return g_variant_get_boolean (variant);
}

/**
 * gcr_system_prompter_set_choice_chosen:
 * @self: a prompter
 * @chosen: the user's choice
 *
 * Used by prompter implementations to let the prompter know when the user
 * has checked or unchecked the choice offered by the prompt.
 */
void
gcr_system_prompter_set_choice_chosen (GcrSystemPrompter *self,
                                       gboolean chosen)
{
	GVariant *variant;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));

	variant = g_variant_new_boolean (chosen);
	g_hash_table_insert (self->pv->properties,
	                     (gpointer)GCR_DBUS_PROMPT_PROPERTY_CHOICE_CHOSEN,
	                     g_variant_ref_sink (variant));

	prompt_emit_changed (self, GCR_DBUS_PROMPT_PROPERTY_CHOICE_CHOSEN);
}

const gchar *
gcr_system_prompter_get_caller_window (GcrSystemPrompter *self)
{
	GVariant *variant;

	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (self), NULL);

	variant = g_hash_table_lookup (self->pv->properties, GCR_DBUS_PROMPT_PROPERTY_CALLER_WINDOW);
	g_return_val_if_fail (variant != NULL, NULL);
	return g_variant_get_string (variant, NULL);
}

/**
 * gcr_system_prompter_respond_cancelled:
 * @self: a prompter
 *
 * Used by prompter implementations to let the prompter know when the user
 * has cancelled the current prompt.
 */
void
gcr_system_prompter_respond_cancelled (GcrSystemPrompter *self)
{
	GDBusMethodInvocation *invocation;
	const gchar *method;
	GVariant *properties;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));
	g_return_if_fail (self->pv->invocation != NULL);

	invocation = self->pv->invocation;
	self->pv->invocation = NULL;

	/* Don't send back any changed properties on cancel */
	properties = g_variant_new_array (G_VARIANT_TYPE ("(ssv)"), NULL, 0);

	_gcr_debug ("responding to with cancelled");

	method = g_dbus_method_invocation_get_method_name (invocation);
	if (method && g_str_equal (method, GCR_DBUS_PROMPT_METHOD_PASSWORD))
		g_dbus_method_invocation_return_value (invocation,
		                                       g_variant_new ("(@a(ssv)s)",
		                                                      properties, ""));

	else if (method && g_str_equal (method, GCR_DBUS_PROMPT_METHOD_CONFIRM))
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@a(ssv)b)",
		                                                                  properties, FALSE));

	else
		g_return_if_reached ();

	g_signal_emit (self, signals[RESPONDED], 0);
}

/**
 * gcr_system_prompter_respond_confirmed:
 * @self: a prompter
 * @password: the password
 *
 * Used by prompter implementations to let the prompter know when the user
 * has entered a password and clicked 'OK'.
 */
void
gcr_system_prompter_respond_with_password (GcrSystemPrompter *self,
                                           const gchar *password)
{
	GDBusMethodInvocation *invocation;
	const gchar *method;
	GVariant *properties;
	gchar *exchange;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));
	g_return_if_fail (password != NULL);
	g_return_if_fail (self->pv->invocation != NULL);

	invocation = self->pv->invocation;
	self->pv->invocation = NULL;

	method = g_dbus_method_invocation_get_method_name (invocation);
	g_return_if_fail (method != NULL && g_str_equal (method, GCR_DBUS_PROMPT_METHOD_PASSWORD));

	/* Send back all the properties before we respond */
	properties = build_changed_properties (self);

	_gcr_debug ("responding to prompt with password");

	exchange = gcr_secret_exchange_send (self->pv->exchange, password, -1);
	g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@a(ssv)s)",
	                                                                  properties, exchange));
	g_free (exchange);

	g_signal_emit (self, signals[RESPONDED], 0);
}

/**
 * gcr_system_prompter_respond_confirmed:
 * @self: a prompter
 *
 * Used by prompter implementations to let the prompter know when the user
 * has confirmed the request, ie: clicked 'OK'.
 */
void
gcr_system_prompter_respond_confirmed (GcrSystemPrompter *self)
{
	GDBusMethodInvocation *invocation;
	GVariant *properties;
	const gchar *method;

	g_return_if_fail (GCR_IS_SYSTEM_PROMPTER (self));
	g_return_if_fail (self->pv->invocation != NULL);

	invocation = self->pv->invocation;
	self->pv->invocation = NULL;

	/* Send back all the properties before we respond */
	properties = build_changed_properties (self);

	method = g_dbus_method_invocation_get_method_name (invocation);
	g_return_if_fail (method != NULL && g_str_equal (method, GCR_DBUS_PROMPT_METHOD_CONFIRM));

	_gcr_debug ("responding to prompt with confirm");

	g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@a(ssv)b)",
	                                                                  properties, TRUE));

	g_signal_emit (self, signals[RESPONDED], 0);
}

GcrSystemPrompter *
gcr_system_prompter_new (void)
{
	return g_object_new (GCR_TYPE_SYSTEM_PROMPTER, NULL);
}
