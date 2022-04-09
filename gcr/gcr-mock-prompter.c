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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stfew@collabora.co.uk>
 */

#include "config.h"

#include "gcr-mock-prompter.h"
#include "gcr-prompt.h"

#include "egg/egg-error.h"

#include <gobject/gvaluecollector.h>

#include <string.h>

/**
 * GcrMockPrompter:
 *
 * A mock [class@SystemPrompter] used for testing against.
 *
 * Use [func@MockPrompter.start] to start the mock prompter in another
 * thread. The returned string is the dbus address of the mock prompter.
 * You can pass this to [func@SystemPrompt.open] as the prompter bus name.
 *
 * Use the [func@MockPrompter.expect_confirm_ok] function and friends before
 * prompting to verify that the prompts are displayed as expected, and to
 * provide a response.
 */

#define GCR_TYPE_MOCK_PROMPT (_gcr_mock_prompt_get_type ())
G_DECLARE_FINAL_TYPE (GcrMockPrompt, _gcr_mock_prompt,
                      GCR, MOCK_PROMPT,
                      GObject)

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
	PROP_CONTINUE_LABEL,
	PROP_CANCEL_LABEL,
};

typedef struct _MockProperty {
	const char *name;
	GValue value;
} MockProperty;

struct _GcrMockPrompt {
	GObject parent;
	GHashTable *properties;
	gboolean disposed;
};

typedef struct {
	gboolean close;
	gboolean proceed;
	gchar *password;
	GList *properties;
} MockResponse;

typedef struct {
	/* Owned by the calling thread */
	GMutex *mutex;
	GCond *start_cond;
	GThread *thread;

	guint delay_msec;
	GQueue responses;

	/* Owned by the prompter thread*/
	GcrSystemPrompter *prompter;
	GDBusConnection *connection;
	GMainLoop *loop;
} ThreadData;

static gint prompts_a_prompting = 0;
static ThreadData *running = NULL;

static void    gcr_mock_prompt_iface     (GcrPromptInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrMockPrompt, _gcr_mock_prompt, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_PROMPT, gcr_mock_prompt_iface);
);

static void
mock_property_free (gpointer data)
{
	MockProperty *param = data;
	g_value_unset (&param->value);
	g_free (param);
}

static void
mock_response_free (gpointer data)
{
	MockResponse *response = data;

	if (response == NULL)
		return;

	g_free (response->password);
	g_list_free_full (response->properties, mock_property_free);
	g_free (response);
}

static void
blank_string_property (GHashTable *properties,
                       const gchar *property)
{
	MockProperty *param;

	param = g_new0 (MockProperty, 1);
	param->name = property;
	g_value_init (&param->value, G_TYPE_STRING);
	g_value_set_string (&param->value, "");
	g_hash_table_insert (properties, (gpointer)param->name, param);
}


static void
blank_boolean_property (GHashTable *properties,
                        const gchar *property)
{
	MockProperty *param;

	param = g_new0 (MockProperty, 1);
	param->name = property;
	g_value_init (&param->value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&param->value, FALSE);
	g_hash_table_insert (properties, (gpointer)param->name, param);
}

static void
blank_int_property (GHashTable *properties,
                    const gchar *property)
{
	MockProperty *param;

	param = g_new0 (MockProperty, 1);
	param->name = property;
	g_value_init (&param->value, G_TYPE_INT);
	g_value_set_int (&param->value, 0);
	g_hash_table_insert (properties, (gpointer)param->name, param);
}

static void
_gcr_mock_prompt_init (GcrMockPrompt *self)
{
	g_atomic_int_add (&prompts_a_prompting, 1);

	self->properties = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                          NULL, mock_property_free);

	blank_string_property (self->properties, "title");
	blank_string_property (self->properties, "message");
	blank_string_property (self->properties, "description");
	blank_string_property (self->properties, "warning");
	blank_string_property (self->properties, "choice-label");
	blank_string_property (self->properties, "caller-window");
	blank_string_property (self->properties, "continue-label");
	blank_string_property (self->properties, "cancel-label");

	blank_boolean_property (self->properties, "choice-chosen");
	blank_boolean_property (self->properties, "password-new");

	blank_int_property (self->properties, "password-strength");
}

static void
_gcr_mock_prompt_set_property (GObject *obj,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	GcrMockPrompt *self = GCR_MOCK_PROMPT (obj);
	MockProperty *param;

	switch (prop_id) {
	case PROP_TITLE:
	case PROP_MESSAGE:
	case PROP_DESCRIPTION:
	case PROP_WARNING:
	case PROP_PASSWORD_NEW:
	case PROP_CHOICE_LABEL:
	case PROP_CHOICE_CHOSEN:
	case PROP_CALLER_WINDOW:
	case PROP_CONTINUE_LABEL:
	case PROP_CANCEL_LABEL:
		param = g_new0 (MockProperty, 1);
		param->name = pspec->name;
		g_value_init (&param->value, pspec->value_type);
		g_value_copy (value, &param->value);
		g_hash_table_replace (self->properties, (gpointer)param->name, param);
		g_object_notify (G_OBJECT (self), param->name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
_gcr_mock_prompt_get_property (GObject *obj,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	GcrMockPrompt *self = GCR_MOCK_PROMPT (obj);
	MockProperty *param;

	switch (prop_id) {
	case PROP_TITLE:
	case PROP_MESSAGE:
	case PROP_DESCRIPTION:
	case PROP_WARNING:
	case PROP_PASSWORD_NEW:
	case PROP_PASSWORD_STRENGTH:
	case PROP_CHOICE_LABEL:
	case PROP_CHOICE_CHOSEN:
	case PROP_CALLER_WINDOW:
	case PROP_CONTINUE_LABEL:
	case PROP_CANCEL_LABEL:
		param = g_hash_table_lookup (self->properties, pspec->name);
		g_return_if_fail (param != NULL);
		g_value_copy (&param->value, value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}


static void
_gcr_mock_prompt_dispose (GObject *obj)
{
	GcrMockPrompt *self = GCR_MOCK_PROMPT (obj);

	if (!self->disposed) {
		g_atomic_int_add (&prompts_a_prompting, -1);
		self->disposed = TRUE;
	}

	G_OBJECT_CLASS (_gcr_mock_prompt_parent_class)->dispose (obj);
}

static void
_gcr_mock_prompt_finalize (GObject *obj)
{
	GcrMockPrompt *self = GCR_MOCK_PROMPT (obj);

	g_hash_table_destroy(self->properties);

	G_OBJECT_CLASS (_gcr_mock_prompt_parent_class)->finalize (obj);
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
prompt_set_or_check_properties (GcrMockPrompt *self,
                                GList *properties)
{
	GValue value = G_VALUE_INIT;
	GObjectClass *object_class;
	MockProperty *param;
	GParamSpec *spec;
	GList *l;

	object_class = G_OBJECT_GET_CLASS (self);
	for (l = properties; l != NULL; l = g_list_next (l)) {
		param = l->data;

		spec = g_object_class_find_property (object_class, param->name);
		g_assert (spec != NULL);

		/* For these we set the value */
		if (g_str_equal (param->name, "choice-chosen")) {
			g_object_set_property (G_OBJECT (self), param->name, &param->value);

		/* For others we check that the value is correct */
		} else {
			g_value_init (&value, G_VALUE_TYPE (&param->value));
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
_gcr_mock_prompt_class_init (GcrMockPromptClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = _gcr_mock_prompt_get_property;
	gobject_class->set_property = _gcr_mock_prompt_set_property;
	gobject_class->dispose = _gcr_mock_prompt_dispose;
	gobject_class->finalize = _gcr_mock_prompt_finalize;

	g_object_class_override_property (gobject_class, PROP_TITLE, "title");
	g_object_class_override_property (gobject_class, PROP_MESSAGE, "message");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
	g_object_class_override_property (gobject_class, PROP_WARNING, "warning");
	g_object_class_override_property (gobject_class, PROP_CALLER_WINDOW, "caller-window");
	g_object_class_override_property (gobject_class, PROP_CHOICE_LABEL, "choice-label");
	g_object_class_override_property (gobject_class, PROP_CHOICE_CHOSEN, "choice-chosen");
	g_object_class_override_property (gobject_class, PROP_PASSWORD_NEW, "password-new");
	g_object_class_override_property (gobject_class, PROP_PASSWORD_STRENGTH, "password-strength");
	g_object_class_override_property (gobject_class, PROP_CONTINUE_LABEL, "continue-label");
	g_object_class_override_property (gobject_class, PROP_CANCEL_LABEL, "cancel-label");
}

static gboolean
on_timeout_complete (gpointer data)
{
	GSimpleAsyncResult *res = data;
	g_simple_async_result_complete (res);
	return FALSE;
}

static gboolean
on_timeout_complete_and_close (gpointer data)
{
	GSimpleAsyncResult *res = data;
	GcrPrompt *prompt = GCR_PROMPT (g_async_result_get_source_object (data));
	g_simple_async_result_complete (res);
	gcr_prompt_close (prompt);
	g_object_unref (prompt);
	return FALSE;
}

static void
destroy_unref_source (gpointer source)
{
	if (!g_source_is_destroyed (source))
		g_source_destroy (source);
	g_source_unref (source);
}

static void
gcr_mock_prompt_confirm_async (GcrPrompt *prompt,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GcrMockPrompt *self = GCR_MOCK_PROMPT (prompt);
	GSourceFunc complete_func = on_timeout_complete;
	GSimpleAsyncResult *res;
	MockResponse *response;
	GSource *source;
	guint delay_msec;

	g_mutex_lock (running->mutex);
	delay_msec = running->delay_msec;
	response = g_queue_pop_head (&running->responses);
	g_mutex_unlock (running->mutex);

	res = g_simple_async_result_new (G_OBJECT (prompt), callback, user_data,
	                                 gcr_mock_prompt_confirm_async);

	if (response == NULL) {
		g_critical ("password prompt requested, but not expected");
		g_simple_async_result_set_op_res_gboolean (res, FALSE);

	} else if (response->close) {
		complete_func = on_timeout_complete_and_close;
		g_simple_async_result_set_op_res_gboolean (res, FALSE);

	} else if (response->password) {
		g_critical ("confirmation prompt requested, but password prompt expected");
		g_simple_async_result_set_op_res_gboolean (res, FALSE);

	} else {
		prompt_set_or_check_properties (self, response->properties);
		g_simple_async_result_set_op_res_gboolean (res, response->proceed);
	}

	if (delay_msec > 0)
		source = g_timeout_source_new (delay_msec);
	else
		source = g_idle_source_new ();

	g_source_set_callback (source, complete_func, g_object_ref (res), g_object_unref);
	g_source_attach (source, g_main_context_get_thread_default ());
	g_object_set_data_full (G_OBJECT (self), "delay-source", source, destroy_unref_source);

	mock_response_free (response);
	g_object_unref (res);
}

static GcrPromptReply
gcr_mock_prompt_confirm_finish (GcrPrompt *prompt,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
	                      gcr_mock_prompt_confirm_async), GCR_PROMPT_REPLY_CANCEL);

	return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (result)) ?
	               GCR_PROMPT_REPLY_CONTINUE : GCR_PROMPT_REPLY_CANCEL;
}

static void
ensure_password_strength (GcrMockPrompt *self,
                          const gchar *password)
{
	MockProperty *param;
	gint strength;

	strength = strlen (password) > 0 ? 1 : 0;
	param = g_new0 (MockProperty, 1);
	param->name = "password-strength";
	g_value_init (&param->value, G_TYPE_INT);
	g_value_set_int (&param->value, strength);
	g_hash_table_replace (self->properties, (gpointer)param->name, param);
	g_object_notify (G_OBJECT (self), param->name);
}

static void
gcr_mock_prompt_password_async (GcrPrompt *prompt,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	GcrMockPrompt *self = GCR_MOCK_PROMPT (prompt);
	GSourceFunc complete_func = on_timeout_complete;
	GSimpleAsyncResult *res;
	MockResponse *response;
	GSource *source;
	guint delay_msec;

	g_mutex_lock (running->mutex);
	delay_msec = running->delay_msec;
	response = g_queue_pop_head (&running->responses);
	g_mutex_unlock (running->mutex);

	res = g_simple_async_result_new (G_OBJECT (prompt), callback, user_data,
	                                 gcr_mock_prompt_password_async);

	if (response == NULL) {
		g_critical ("password prompt requested, but not expected");
		g_simple_async_result_set_op_res_gpointer (res, NULL, NULL);

	} else if (response->close) {
		g_simple_async_result_set_op_res_gpointer (res, NULL, NULL);
		complete_func = on_timeout_complete_and_close;

	} else if (!response->password) {
		g_critical ("password prompt requested, but confirmation prompt expected");
		g_simple_async_result_set_op_res_gpointer (res, NULL, NULL);

	} else if (!response->proceed) {
		prompt_set_or_check_properties (self, response->properties);
		g_simple_async_result_set_op_res_gpointer (res, NULL, NULL);

	} else {
		ensure_password_strength (self, response->password);
		prompt_set_or_check_properties (self, response->properties);
		g_simple_async_result_set_op_res_gpointer (res, response->password, g_free);
		response->password = NULL;
	}

	mock_response_free (response);

	if (delay_msec > 0)
		source = g_timeout_source_new (delay_msec);
	else
		source = g_idle_source_new ();

	g_source_set_callback (source, complete_func, g_object_ref (res), g_object_unref);
	g_source_attach (source, g_main_context_get_thread_default ());
	g_object_set_data_full (G_OBJECT (self), "delay-source", source, destroy_unref_source);

	g_object_unref (res);
}


static const gchar *
gcr_mock_prompt_password_finish (GcrPrompt *prompt,
                                 GAsyncResult *result,
                                 GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
	                      gcr_mock_prompt_password_async), NULL);

	return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

}

static void
gcr_mock_prompt_iface (GcrPromptInterface *iface)
{
	iface->prompt_confirm_async = gcr_mock_prompt_confirm_async;
	iface->prompt_confirm_finish = gcr_mock_prompt_confirm_finish;
	iface->prompt_password_async = gcr_mock_prompt_password_async;
	iface->prompt_password_finish = gcr_mock_prompt_password_finish;
}

static GList *
build_properties (GObjectClass *object_class,
                  const gchar *first_property,
                  va_list var_args)
{
	GList *result = NULL;
	const gchar *name;

	name = first_property;
	while (name) {
		GValue value = G_VALUE_INIT;
		MockProperty *parameter;
		GParamSpec *spec;
		gchar *error = NULL;

		spec = g_object_class_find_property (object_class, name);
		if (spec == NULL) {
			g_warning ("prompt object class has no property named '%s'", name);
			break;
		}

		if ((spec->flags & G_PARAM_CONSTRUCT_ONLY) && !(spec->flags & G_PARAM_READABLE)) {
			g_warning ("prompt property '%s' can't be set after construction", name);
			break;
		}

		G_VALUE_COLLECT_INIT (&value, spec->value_type, var_args, 0, &error);
		if (error != NULL) {
			g_warning ("%s", error);
			g_free (error);
			g_value_unset (&value);
			break;
		}

		parameter = g_new0 (MockProperty, 1);
		parameter->name = g_intern_string (name);
		memcpy (&parameter->value, &value, sizeof (value));
		result = g_list_prepend (result, parameter);

		name = va_arg (var_args, gchar *);
	}

	return result;
}

/**
 * gcr_mock_prompter_is_prompting:
 *
 * Check if the mock prompter is showing any prompts.
 *
 * Returns: whether prompting
 */
gboolean
gcr_mock_prompter_is_prompting (void)
{
	return g_atomic_int_get (&prompts_a_prompting) > 0;
}

/**
 * gcr_mock_prompter_get_delay_msec:
 *
 * Get the delay in milliseconds before the mock prompter completes
 * an expected prompt.
 *
 * Returns: the delay
 */
guint
gcr_mock_prompter_get_delay_msec (void)
{
	guint delay_msec;

	g_assert (running != NULL);
	g_mutex_lock (running->mutex);
	delay_msec = running->delay_msec;
	g_mutex_unlock (running->mutex);

	return delay_msec;
}

/**
 * gcr_mock_prompter_set_delay_msec:
 * @delay_msec: prompt response delay in milliseconds
 *
 * Set the delay in milliseconds before the mock prompter completes
 * an expected prompt.
 */
void
gcr_mock_prompter_set_delay_msec (guint delay_msec)
{
	g_assert (running != NULL);
	g_mutex_lock (running->mutex);
	running->delay_msec = delay_msec;
	g_mutex_unlock (running->mutex);
}

/**
 * gcr_mock_prompter_expect_confirm_ok:
 * @first_property_name: the first property name in the argument list or %NULL
 * @...: properties to expect
 *
 * Queue an expected response on the mock prompter.
 *
 * Expects a confirmation prompt, and then confirms that prompt by
 * simulating a click on the ok button.
 *
 * Additional property pairs for the prompt can be added in the argument
 * list, in the same way that you would with g_object_new().
 *
 * If the "choice-chosen" property is specified then that value will be
 * set on the prompt as if the user had changed the value.
 *
 * All other properties will be checked against the prompt, and an error
 * will occur if they do not match the value set on the prompt.
 */
void
gcr_mock_prompter_expect_confirm_ok (const gchar *first_property_name,
                                     ...)
{
	MockResponse *response;
	gpointer klass;
	va_list var_args;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = NULL;
	response->proceed = TRUE;

	klass = g_type_class_ref (GCR_TYPE_MOCK_PROMPT);

	va_start (var_args, first_property_name);
	response->properties = build_properties (G_OBJECT_CLASS (klass), first_property_name, var_args);
	va_end (var_args);

	g_type_class_unref (klass);
	g_queue_push_tail (&running->responses, response);
	g_mutex_unlock (running->mutex);
}

/**
 * gcr_mock_prompter_expect_confirm_cancel:
 *
 * Queue an expected response on the mock prompter.
 *
 * Expects a confirmation prompt, and then cancels that prompt.
 */
void
gcr_mock_prompter_expect_confirm_cancel (void)
{
	MockResponse *response;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = NULL;
	response->proceed = FALSE;

	g_queue_push_tail (&running->responses, response);

	g_mutex_unlock (running->mutex);
}

/**
 * gcr_mock_prompter_expect_password_ok:
 * @password: the password to return from the prompt
 * @first_property_name: the first property name in the argument list or %NULL
 * @...: properties to expect
 *
 * Queue an expected response on the mock prompter.
 *
 * Expects a password prompt, and returns @password as if the user had entered
 * it and clicked the ok button.
 *
 * Additional property pairs for the prompt can be added in the argument
 * list, in the same way that you would with g_object_new().
 *
 * If the "choice-chosen" property is specified then that value will be
 * set on the prompt as if the user had changed the value.
 *
 * All other properties will be checked against the prompt, and an error
 * will occur if they do not match the value set on the prompt.
 */
void
gcr_mock_prompter_expect_password_ok (const gchar *password,
                                      const gchar *first_property_name,
                                      ...)
{
	MockResponse *response;
	gpointer klass;
	va_list var_args;

	g_assert (running != NULL);
	g_assert (password != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = g_strdup (password);
	response->proceed = TRUE;

	klass = g_type_class_ref (GCR_TYPE_MOCK_PROMPT);

	va_start (var_args, first_property_name);
	response->properties = build_properties (G_OBJECT_CLASS (klass), first_property_name, var_args);
	va_end (var_args);

	g_type_class_unref (klass);
	g_queue_push_tail (&running->responses, response);

	g_mutex_unlock (running->mutex);
}

/**
 * gcr_mock_prompter_expect_password_cancel:
 *
 * Queue an expected response on the mock prompter.
 *
 * Expects a password prompt, and then cancels that prompt.
 */
void
gcr_mock_prompter_expect_password_cancel (void)
{
	MockResponse *response;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->password = g_strdup ("");
	response->proceed = FALSE;

	g_queue_push_tail (&running->responses, response);

	g_mutex_unlock (running->mutex);
}

/**
 * gcr_mock_prompter_expect_close:
 *
 * Queue an expected response on the mock prompter.
 *
 * Expects any prompt, and closes the prompt when it gets it.
 */
void
gcr_mock_prompter_expect_close (void)
{
	MockResponse *response;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	response = g_new0 (MockResponse, 1);
	response->close = TRUE;

	g_queue_push_tail (&running->responses, response);

	g_mutex_unlock (running->mutex);
}

/**
 * gcr_mock_prompter_is_expecting:
 *
 * Check if the mock prompter is expecting a response. This will be %TRUE
 * when one of the <literal>gcr_mock_prompter_expect_xxx<!-- -->()</literal>
 * functions have been used to queue an expected prompt, but that prompt
 * response has not be 'used' yet.
 *
 * Returns: whether expecting a prompt
 */
gboolean
gcr_mock_prompter_is_expecting (void)
{
	gboolean expecting;

	g_assert (running != NULL);

	g_mutex_lock (running->mutex);

	expecting = !g_queue_is_empty (&running->responses);

	g_mutex_unlock (running->mutex);

	return expecting;
}

static gboolean
on_idle_signal_cond (gpointer user_data)
{
	GCond *cond = user_data;
	g_cond_signal (cond);
	return FALSE; /* Don't run again */
}

/*
 * These next few functions test the new-prompt signals of
 * GcrSystemPrompter. They should probably be in tests, but
 * don't fit there nicely.
 */
static GcrPrompt *
on_new_prompt_skipped (GcrSystemPrompter *prompter,
                       gpointer user_data)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (prompter), NULL);
	return NULL;
}

static GcrPrompt *
on_new_prompt_creates (GcrSystemPrompter *prompter,
                       gpointer user_data)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (prompter), NULL);
	return g_object_new (GCR_TYPE_MOCK_PROMPT, NULL);
}

static GcrPrompt *
on_new_prompt_not_called (GcrSystemPrompter *prompter,
                          gpointer user_data)
{
	g_return_val_if_fail (GCR_IS_SYSTEM_PROMPTER (prompter), NULL);
	g_return_val_if_reached (NULL);
}

static gpointer
mock_prompter_thread (gpointer data)
{
	ThreadData *thread_data = data;
	GDBusConnection *connection = NULL;
	GMainContext *context;
	GError *error = NULL;
	GSource *idle;
	gchar *address;

	g_mutex_lock (thread_data->mutex);
	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	/*
	 * Random choice between signals, and prompt-gtype style of creating
	 * GcrPrompt objects.
	 */

	if (g_random_boolean ()) {
		/* Allows GcrSystemPrompter to create the prompts directly */
		thread_data->prompter = gcr_system_prompter_new (GCR_SYSTEM_PROMPTER_SINGLE,
		                                                 GCR_TYPE_MOCK_PROMPT);

	} else {
		/* Create the prompt objects in signal handler */
		thread_data->prompter = gcr_system_prompter_new (GCR_SYSTEM_PROMPTER_SINGLE, 0);
		g_signal_connect (thread_data->prompter, "new-prompt", G_CALLBACK (on_new_prompt_skipped), NULL);
		g_signal_connect (thread_data->prompter, "new-prompt", G_CALLBACK (on_new_prompt_creates), NULL);
		g_signal_connect (thread_data->prompter, "new-prompt", G_CALLBACK (on_new_prompt_not_called), NULL);
	}

	address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error == NULL) {
		connection = g_dbus_connection_new_for_address_sync (address,
		                                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
		                                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
		                                                     NULL, NULL, &error);
		if (error == NULL) {
			thread_data->connection = connection;
			gcr_system_prompter_register (GCR_SYSTEM_PROMPTER (thread_data->prompter),
			                              connection);
		} else {
			g_critical ("couldn't create connection: %s", error->message);
			g_error_free (error);
		}

		g_free (address);
	}

	if (error != NULL) {
		g_critical ("mock prompter couldn't get session bus address: %s",
		            egg_error_message (error));
		g_clear_error (&error);
	}

	thread_data->loop = g_main_loop_new (context, FALSE);
	g_mutex_unlock (thread_data->mutex);

	idle = g_idle_source_new ();
	g_source_set_callback (idle, on_idle_signal_cond, thread_data->start_cond, NULL);
	g_source_attach (idle, context);
	g_source_unref (idle);

	g_main_loop_run (thread_data->loop);

	g_mutex_lock (thread_data->mutex);
	g_main_context_pop_thread_default (context);

	gcr_system_prompter_unregister (thread_data->prompter, TRUE);
	g_object_unref (thread_data->prompter);
	thread_data->prompter = NULL;

	if (connection) {
		thread_data->connection = NULL;

		if (!g_dbus_connection_is_closed (connection)) {
			if (!g_dbus_connection_flush_sync (connection, NULL, &error)) {
				g_critical ("connection flush failed: %s", error->message);
				g_error_free (error);
			}
			if (!g_dbus_connection_close_sync (connection, NULL, &error)) {
				g_critical ("connection close failed: %s", error->message);
				g_error_free (error);
			}
		}

		g_object_unref (connection);
	}

	while (g_main_context_iteration (context, FALSE));

	g_main_context_unref (context);
	g_main_loop_unref (thread_data->loop);
	thread_data->loop = NULL;

	g_mutex_unlock (thread_data->mutex);
	return thread_data;
}

/**
 * gcr_mock_prompter_start:
 *
 * Start the mock prompter. This is often used from the
 * <literal>setup<!-- -->()</literal> function of tests.
 *
 * Starts the mock prompter in an additional thread. Use the returned DBus bus
 * name with gcr_system_prompt_open_for_prompter() to connect to this prompter.
 *
 * Returns: the bus name that the mock prompter is listening on
 */
const gchar *
gcr_mock_prompter_start (void)
{
	GError *error = NULL;

	g_assert (running == NULL);

	running = g_new0 (ThreadData, 1);
	running->mutex = g_new0 (GMutex, 1);
	g_mutex_init (running->mutex);
	running->start_cond = g_new0 (GCond, 1);
	g_cond_init (running->start_cond);
	g_queue_init (&running->responses);
	g_mutex_lock (running->mutex);

	running->thread = g_thread_new ("mock-prompter", mock_prompter_thread, running);
	if (error != NULL)
		g_error ("mock prompter couldn't start thread: %s", error->message);

	g_cond_wait (running->start_cond, running->mutex);
	g_assert (running->loop);
	g_assert (running->prompter);
	g_mutex_unlock (running->mutex);

	return g_dbus_connection_get_unique_name (running->connection);
}

/**
 * gcr_mock_prompter_disconnect:
 *
 * Disconnect the mock prompter
 */
void
gcr_mock_prompter_disconnect (void)
{
	GError *error = NULL;

	g_assert (running != NULL);
	g_assert (running->connection);

	g_dbus_connection_close_sync (running->connection, NULL, &error);
	if (error != NULL) {
		g_critical ("disconnect connection close failed: %s", error->message);
		g_error_free (error);
	}
}

/**
 * gcr_mock_prompter_stop:
 *
 * Stop the mock prompter. This is often used from the
 * <literal>teardown<!-- -->()</literal> function of tests.
 */
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

	g_queue_foreach (&running->responses, (GFunc)mock_response_free, NULL);
	g_queue_clear (&running->responses);

	g_cond_clear (running->start_cond);
	g_free (running->start_cond);
	g_mutex_clear (running->mutex);
	g_free (running->mutex);

	g_free (running);
	running = NULL;
}
