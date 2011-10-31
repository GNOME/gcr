/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * Author: Stef Walter <stfew@collabora.co.uk>
 */

#include "config.h"

#include "gcr-mock-prompter.h"

#include "egg/egg-error.h"

#include <gobject/gvaluecollector.h>

#include <string.h>

/**
 * SECTION:gcr-mock-prompter
 * @title: GcrMockPrompter
 * @short_description: XXX
 *
 * XXXX
 */

/**
 * GcrMockPrompter:
 *
 * XXX
 */

/**
 * GcrMockPrompterClass:
 *
 * The class for #GcrMockPrompter.
 */


GType   gcr_mock_prompter_get_type        (void) G_GNUC_CONST;
#define GCR_TYPE_MOCK_PROMPTER            (gcr_mock_prompter_get_type ())
#define GCR_MOCK_PROMPTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_MOCK_PROMPTER, GcrMockPrompter))
#define GCR_IS_MOCK_PROMPTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_MOCK_PROMPTER))
#define GCR_IS_MOCK_PROMPTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_MOCK_PROMPTER))
#define GCR_MOCK_PROMPTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_MOCK_PROMPTER, GcrMockPromptClass))
#define GCR_MOCK_PROMPTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_MOCK_PROMPTER, GcrMockPromptClass))

typedef struct _GcrMockPrompter GcrMockPrompter;
typedef struct _GcrMockPrompterClass GcrMockPrompterClass;
typedef struct _GcrMockPrompterPrivate GcrMockPrompterPrivate;

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_SHOWING,
	PROP_DELAY_MSEC
};

struct _GcrMockPrompter {
	GcrSystemPrompter parent;
	GDBusConnection *connection;
	GQueue *responses;
	gboolean showing;
	guint delay_msec;
	guint delay_source;
};

struct _GcrMockPrompterClass {
	GcrSystemPrompterClass parent_class;
};

typedef struct {
	gboolean proceed;
	gchar *password;
	GList *properties;

	/* Used while responding */
	GcrMockPrompter *prompter;
} MockResponse;

typedef struct {
	/* Owned by the calling thread */
	GMutex *mutex;
	GCond *start_cond;
	GThread *thread;

	/* Owned by the prompter thread*/
	GcrMockPrompter *prompter;
	const gchar *bus_name;
	GMainLoop *loop;
} ThreadData;

static ThreadData *running = NULL;

G_DEFINE_TYPE (GcrMockPrompter, gcr_mock_prompter, GCR_TYPE_SYSTEM_PROMPTER);

static void
mock_property_free (gpointer data)
{
	GParameter *param = data;
	g_value_unset (&param->value);
	g_free (param);
}

static void
mock_response_free (gpointer data)
{
	MockResponse *response = data;
	g_free (response->password);
	g_list_free_full (response->properties, mock_property_free);
	g_clear_object (&response->prompter);
	g_free (response);
}

static void
gcr_mock_prompter_init (GcrMockPrompter *self)
{
	self->responses = g_queue_new ();
}

static void
gcr_mock_prompter_set_property (GObject *obj,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (obj);

	switch (prop_id) {
	case PROP_CONNECTION:
		self->connection = g_value_get_object (value);
		break;
	case PROP_DELAY_MSEC:
		self->delay_msec = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_mock_prompter_get_property (GObject *obj,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (obj);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, self->connection);
		break;
	case PROP_SHOWING:
		g_value_set_boolean (value, self->showing);
		break;
	case PROP_DELAY_MSEC:
		g_value_set_uint (value, self->delay_msec);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}


static void
gcr_mock_prompter_dispose (GObject *obj)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (obj);
	MockResponse *response;

	if (self->delay_source) {
		g_source_remove (self->delay_source);
		self->delay_source = 0;
	}

	if (self->connection) {
		gcr_system_prompter_unregister (GCR_SYSTEM_PROMPTER (self), self->connection);
		g_object_remove_weak_pointer (G_OBJECT (self->connection),
		                              (gpointer *)&self->connection);
		self->connection = NULL;
	}

	while ((response = g_queue_pop_head (self->responses)))
		mock_response_free (response);

	G_OBJECT_CLASS (gcr_mock_prompter_parent_class)->dispose (obj);
}

static void
gcr_mock_prompter_finalize (GObject *obj)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (obj);

	g_queue_free (self->responses);

	G_OBJECT_CLASS (gcr_mock_prompter_parent_class)->finalize (obj);
}

static gboolean
value_equal (const GValue *a, const GValue *b)
{
	gboolean ret = FALSE;

	g_assert (G_VALUE_TYPE (a) == G_VALUE_TYPE (b));

	switch (G_VALUE_TYPE (a)) {
	case G_TYPE_BOOLEAN:
		ret = (g_value_get_boolean (a) == g_value_get_boolean (b));
		break;
	case G_TYPE_UCHAR:
		ret = (g_value_get_uchar (a) == g_value_get_uchar (b));
		break;
	case G_TYPE_INT:
		ret = (g_value_get_int (a) == g_value_get_int (b));
		break;
	case G_TYPE_UINT:
		ret = (g_value_get_uint (a) == g_value_get_uint (b));
		break;
	case G_TYPE_INT64:
		ret = (g_value_get_int64 (a) == g_value_get_int64 (b));
		break;
	case G_TYPE_UINT64:
		ret = (g_value_get_uint64 (a) == g_value_get_uint64 (b));
		break;
	case G_TYPE_DOUBLE:
		ret = (g_value_get_double (a) == g_value_get_double (b));
		break;
	case G_TYPE_STRING:
		ret = (g_strcmp0 (g_value_get_string (a), g_value_get_string (b)) == 0);
		break;
	default:
		g_critical ("no support for comparing of type %s", g_type_name (G_VALUE_TYPE (a)));
		break;
	}

	return ret;
}

static void
prompter_set_properties (GcrMockPrompter *self,
                         GList *properties)
{
	GObjectClass *object_class;
	GParameter *param;
	GParamSpec *spec;
	GList *l;

	object_class = G_OBJECT_GET_CLASS (self);
	for (l = properties; l != NULL; l = g_list_next (l)) {
		param = l->data;

		spec = g_object_class_find_property (object_class, param->name);
		g_assert (spec != NULL);

		/* A writable property, set it */
		if ((spec->flags & G_PARAM_WRITABLE)) {
			g_object_set_property (G_OBJECT (self), param->name, &param->value);

		/* Other properties get checked */
		} else {
			GValue value = G_VALUE_INIT;

			g_value_init (&value, spec->value_type);
			g_object_get_property (G_OBJECT (self), param->name, &value);
			if (!value_equal (&value, &param->value)) {
				gchar *expected = g_strdup_value_contents (&param->value);
				gchar *actual = g_strdup_value_contents (&value);

				g_critical ("expected prompt property '%s' to be %s, but it "
				            "is instead %s", param->name, expected, actual);

				g_free (expected);
				g_free (actual);
			}

			g_value_unset (&value);
		}
	}
}

static void
gcr_mock_prompter_open (GcrSystemPrompter *prompter)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (prompter);
	self->showing = TRUE;
}

static void
gcr_mock_prompter_close (GcrSystemPrompter *prompter)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (prompter);

	if (self->delay_source != 0) {
		g_source_remove (self->delay_source);
		self->delay_source = 0;
	}

	self->showing = FALSE;
}

static gboolean
on_timeout_prompt_confirm (gpointer data)
{
	MockResponse *response = data;
	GcrSystemPrompter *prompter = GCR_SYSTEM_PROMPTER (response->prompter);

	response->prompter->delay_source = 0;

	if (!response->proceed)
		gcr_system_prompter_respond_cancelled (prompter);
	else
		gcr_system_prompter_respond_confirmed (prompter);

	return FALSE;
}

static gboolean
gcr_mock_prompter_prompt_confirm (GcrSystemPrompter *prompter)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (prompter);
	MockResponse *response = g_queue_pop_head (self->responses);

	if (response == NULL) {
		g_critical ("confirmation prompt requested, but not expected");
		return FALSE;

	} else if (response->password) {
		g_critical ("confirmation prompt requested, but password prompt expected");
		mock_response_free (response);
		return FALSE;
	}

	prompter_set_properties (self, response->properties);
	response->prompter = g_object_ref (prompter);

	if (self->delay_msec > 0) {
		g_assert (!self->delay_source);
		self->delay_source = g_timeout_add_full (G_PRIORITY_DEFAULT, self->delay_msec,
		                                         on_timeout_prompt_confirm,
		                                         response, mock_response_free);
	} else {
		on_timeout_prompt_confirm (response);
		mock_response_free (response);
	}

	return TRUE;
}

static gboolean
on_timeout_prompt_password (gpointer data)
{
	MockResponse *response = data;
	GcrSystemPrompter *prompter = GCR_SYSTEM_PROMPTER (response->prompter);

	response->prompter->delay_source = 0;

	if (!response->proceed)
		gcr_system_prompter_respond_cancelled (prompter);
	else
		gcr_system_prompter_respond_with_password (prompter, response->password);

	return FALSE;
}

static gboolean
gcr_mock_prompter_prompt_password (GcrSystemPrompter *prompter)
{
	GcrMockPrompter *self = GCR_MOCK_PROMPTER (prompter);
	MockResponse *response = g_queue_pop_head (self->responses);

	if (response == NULL) {
		g_critical ("password prompt requested, but not expected");
		return FALSE;

	} else if (!response->password) {
		g_critical ("password prompt requested, but confirmation prompt expected");
		mock_response_free (response);
		return FALSE;

	}

	prompter_set_properties (self, response->properties);
	response->prompter = g_object_ref (prompter);

	if (self->delay_msec > 0) {
		g_assert (!self->delay_source);
		self->delay_source = g_timeout_add_full (G_PRIORITY_DEFAULT, self->delay_msec,
		                                         on_timeout_prompt_password,
		                                         response, mock_response_free);
	} else {
		on_timeout_prompt_password (response);
		mock_response_free (response);
	}

	return TRUE;
}

static void
gcr_mock_prompter_class_init (GcrMockPrompterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GcrSystemPrompterClass *prompter_class = GCR_SYSTEM_PROMPTER_CLASS (klass);

	gobject_class->get_property = gcr_mock_prompter_get_property;
	gobject_class->set_property = gcr_mock_prompter_set_property;
	gobject_class->dispose = gcr_mock_prompter_dispose;
	gobject_class->finalize = gcr_mock_prompter_finalize;

	prompter_class->open = gcr_mock_prompter_open;
	prompter_class->close = gcr_mock_prompter_close;
	prompter_class->prompt_password= gcr_mock_prompter_prompt_password;
	prompter_class->prompt_confirm = gcr_mock_prompter_prompt_confirm;

	g_object_class_install_property (gobject_class, PROP_CONNECTION,
	            g_param_spec_object ("connection", "Connection", "DBus connection",
	                                 G_TYPE_DBUS_CONNECTION, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_SHOWING,
	            g_param_spec_boolean ("showing", "Showing", "Whether showing a prompt",
	                                  FALSE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_DELAY_MSEC,
	              g_param_spec_uint ("delay-msec", "Delay msec", "Prompt delay in milliseconds",
	                                 0, G_MAXUINT, 0, G_PARAM_READWRITE));
}

static GList *
build_properties (GcrMockPrompter *self,
                  const gchar *first_property,
                  va_list var_args)
{
	GObjectClass *object_class;
	GList *result = NULL;
	const gchar *name;

	object_class = G_OBJECT_GET_CLASS (self);

	name = first_property;
	while (name) {
		GValue value = G_VALUE_INIT;
		GParameter *parameter;
		GParamSpec *spec;
		gchar *error = NULL;

		spec = g_object_class_find_property (object_class, name);
		if (spec == NULL) {
			g_warning ("%s object class has no property named '%s'",
			           G_OBJECT_TYPE_NAME (self), name);
			break;
		}

		if ((spec->flags & G_PARAM_CONSTRUCT_ONLY) && !(spec->flags & G_PARAM_READABLE)) {
			g_warning ("%s property '%s' can't be set after construction",
			           G_OBJECT_TYPE_NAME (self), name);
			break;
		}

		G_VALUE_COLLECT_INIT (&value, spec->value_type, var_args, 0, &error);
		if (error != NULL) {
			g_warning ("%s", error);
			g_free (error);
			g_value_unset (&value);
			break;
		}

		parameter = g_new0 (GParameter, 1);
		parameter->name = g_intern_string (name);
		memcpy (&parameter->value, &value, sizeof (value));
		result = g_list_prepend (result, parameter);

		name = va_arg (var_args, gchar *);
	}

	return result;
}

gboolean
gcr_mock_prompter_get_showing (void)
{
	gboolean showing = FALSE;

	g_assert (running != NULL);
	g_mutex_lock (running->mutex);
	g_object_get (running->prompter, "showing", &showing, NULL);
	g_mutex_unlock (running->mutex);

	return showing;
}

guint
gcr_mock_prompter_get_delay_msec (void)
{
	guint delay_msec;

	g_assert (running != NULL);
	g_mutex_lock (running->mutex);
	g_object_get (running->prompter, "delay-msec", &delay_msec, NULL);
	g_mutex_unlock (running->mutex);

	return delay_msec;
}

void
gcr_mock_prompter_set_delay_msec (guint delay_msec)
{
	g_assert (running != NULL);
	g_mutex_lock (running->mutex);
	g_object_set (running->prompter, "delay-msec", delay_msec, NULL);
	g_mutex_unlock (running->mutex);
}

void
gcr_mock_prompter_expect_confirm_ok (const gchar *first_property_name,
                                     ...)
{
	MockResponse *response;
	va_list var_args;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = NULL;
	response->proceed = TRUE;

	va_start (var_args, first_property_name);
	response->properties = build_properties (running->prompter, first_property_name, var_args);
	va_end (var_args);

	g_queue_push_tail (running->prompter->responses, response);
	g_mutex_unlock (running->mutex);
}

void
gcr_mock_prompter_expect_confirm_cancel (void)
{
	MockResponse *response;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = NULL;
	response->proceed = FALSE;

	g_queue_push_tail (running->prompter->responses, response);

	g_mutex_unlock (running->mutex);
}

void
gcr_mock_prompter_expect_password_ok (const gchar *password,
                                      const gchar *first_property_name,
                                      ...)
{
	MockResponse *response;
	va_list var_args;

	g_assert (running != NULL);
	g_assert (password != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = g_strdup (password);
	response->proceed = TRUE;

	va_start (var_args, first_property_name);
	response->properties = build_properties (running->prompter, first_property_name, var_args);
	va_end (var_args);

	g_queue_push_tail (running->prompter->responses, response);

	g_mutex_unlock (running->mutex);
}

void
gcr_mock_prompter_expect_password_cancel (void)
{
	MockResponse *response;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = g_strdup ("");
	response->proceed = FALSE;

	g_queue_push_tail (running->prompter->responses, response);

	g_mutex_unlock (running->mutex);
}

static gpointer
mock_prompter_thread (gpointer data)
{
	ThreadData *thread_data = data;
	GDBusConnection *connection;
	GMainContext *context;
	GError *error = NULL;
	gchar *address;

	g_mutex_lock (thread_data->mutex);
	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	thread_data->prompter = g_object_new (GCR_TYPE_MOCK_PROMPTER, NULL);

	address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error == NULL) {
		connection = g_dbus_connection_new_for_address_sync (address,
		                                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
		                                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
		                                                     NULL, NULL, &error);
		if (error == NULL) {
			gcr_system_prompter_register (GCR_SYSTEM_PROMPTER (thread_data->prompter),
			                              connection);
			thread_data->bus_name = g_dbus_connection_get_unique_name (connection);
		}

		g_free (address);
	}

	if (error != NULL) {
		g_critical ("mock prompter couldn't get session bus address: %s",
		            egg_error_message (error));
		g_clear_error (&error);
	}

	thread_data->loop = g_main_loop_new (context, FALSE);
	g_cond_signal (thread_data->start_cond);
	g_mutex_unlock (thread_data->mutex);

	g_main_loop_run (thread_data->loop);

	g_mutex_lock (thread_data->mutex);
	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);

	if (connection) {
		gcr_system_prompter_unregister (GCR_SYSTEM_PROMPTER (thread_data->prompter),
		                                connection);
		g_object_unref (connection);
	}

	g_mutex_unlock (thread_data->mutex);
	return thread_data;
}

const gchar *
gcr_mock_prompter_start (void)
{
	GError *error = NULL;

	g_assert (running == NULL);

	running = g_new0 (ThreadData, 1);
	running->mutex = g_mutex_new ();
	running->start_cond = g_cond_new ();

	g_mutex_lock (running->mutex);
	running->thread = g_thread_create (mock_prompter_thread, running, TRUE, &error);

	if (error != NULL)
		g_error ("mock prompter couldn't start thread: %s", error->message);

	g_cond_wait (running->start_cond, running->mutex);
	g_assert (running->loop);
	g_assert (running->prompter);
	g_mutex_unlock (running->mutex);

	return running->bus_name;
}

void
gcr_mock_prompter_stop (void)
{
	ThreadData *check;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);
	g_assert (running->loop != NULL);
	g_main_loop_quit (running->loop);
	g_mutex_unlock (running->mutex);

	check = g_thread_join (running->thread);
	g_assert (check == running);

	g_cond_free (running->start_cond);
	g_mutex_free (running->mutex);
	g_free (running);
	running = NULL;
}
