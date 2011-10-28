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

#define GCR_IS_MOCK_PROMPTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_MOCK_PROMPTER))
#define GCR_MOCK_PROMPTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_MOCK_PROMPTER, GcrMockPromptClass))
#define GCR_MOCK_PROMPTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_MOCK_PROMPTER, GcrMockPromptClass))

typedef struct _GcrMockPrompterClass GcrMockPrompterClass;
typedef struct _GcrMockPrompterPrivate GcrMockPrompterPrivate;

enum {
	PROP_0,
	PROP_CONNECTION,
};

typedef struct {
	gboolean proceed;
	gchar *password;
	GList *properties;
} MockResponse;

struct _GcrMockPrompter {
	GcrSystemPrompter parent;
	GDBusConnection *connection;
	GQueue *responses;
};

struct _GcrMockPrompterClass {
	GcrSystemPrompterClass parent_class;
};

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

	if (!response->proceed)
		gcr_system_prompter_respond_cancelled (GCR_SYSTEM_PROMPTER (self));
	else
		gcr_system_prompter_respond_confirmed (GCR_SYSTEM_PROMPTER (self));

	mock_response_free (response);
	return TRUE;
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

	if (!response->proceed)
		gcr_system_prompter_respond_cancelled (GCR_SYSTEM_PROMPTER (self));
	else
		gcr_system_prompter_respond_with_password (GCR_SYSTEM_PROMPTER (self), response->password);

	mock_response_free (response);
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

	prompter_class->prompt_password= gcr_mock_prompter_prompt_password;
	prompter_class->prompt_confirm = gcr_mock_prompter_prompt_confirm;

	g_object_class_install_property (gobject_class, PROP_CONNECTION,
	            g_param_spec_object ("connection", "Connection", "DBus connection",
	                                 G_TYPE_DBUS_CONNECTION, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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

void
gcr_mock_prompter_expect_confirm_ok (GcrMockPrompter *self,
                                     const gchar *first_property_name,
                                     ...)
{
	MockResponse *response;
	va_list var_args;

	g_return_if_fail (GCR_IS_MOCK_PROMPTER (self));

	response = g_new0 (MockResponse, 1);
	response->password = NULL;
	response->proceed = TRUE;

	va_start (var_args, first_property_name);
	response->properties = build_properties (self, first_property_name, var_args);
	va_end (var_args);

	g_queue_push_tail (self->responses, response);
}

void
gcr_mock_prompter_expect_confirm_cancel (GcrMockPrompter *self)
{
	MockResponse *response;

	g_return_if_fail (GCR_IS_MOCK_PROMPTER (self));

	response = g_new0 (MockResponse, 1);
	response->password = NULL;
	response->proceed = FALSE;

	g_queue_push_tail (self->responses, response);

}

void
gcr_mock_prompter_expect_password_ok (GcrMockPrompter *self,
                                      const gchar *password,
                                      const gchar *first_property_name,
                                      ...)
{
	MockResponse *response;
	va_list var_args;

	g_return_if_fail (GCR_IS_MOCK_PROMPTER (self));
	g_return_if_fail (password != NULL);

	response = g_new0 (MockResponse, 1);
	response->password = g_strdup (password);
	response->proceed = TRUE;

	va_start (var_args, first_property_name);
	response->properties = build_properties (self, first_property_name, var_args);
	va_end (var_args);

	g_queue_push_tail (self->responses, response);
}

void
gcr_mock_prompter_expect_password_cancel (GcrMockPrompter *self)
{
	MockResponse *response;

	g_return_if_fail (GCR_IS_MOCK_PROMPTER (self));

	response = g_new0 (MockResponse, 1);
	response->password = g_strdup ("");
	response->proceed = FALSE;

	g_queue_push_tail (self->responses, response);
}

GcrMockPrompter *
gcr_mock_prompter_new ()
{
	return g_object_new (GCR_TYPE_MOCK_PROMPTER,
	                     NULL);
}
