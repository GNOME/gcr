/*
 * Copyright (C) 2010 Stefan Walter
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
 */

#include "config.h"

#include "gcr-key-mechanisms.h"

#include <glib/gi18n-lib.h>

static gboolean
check_have_attributes (GckAttributes *attrs,
                       const gulong *types,
                       gsize n_types)
{
	gsize i;

	for (i = 0; i < n_types; i++) {
		if (!gck_attributes_find (attrs, types[i]))
			return FALSE;
	}

	return TRUE;
}

static gulong
find_first_usable_mechanism (GckObject *key,
                             GckAttributes *attrs,
                             const gulong *mechanisms,
                             gsize n_mechanisms,
                             gulong action_attr_type)
{
	GckSession *session;
	GckSlot *slot;
	GArray *mechs;
	gboolean can;
	gsize i;

	if (gck_attributes_find_boolean (attrs, action_attr_type, &can) && !can) {
		g_debug ("key not capable of needed action");
		return GCK_INVALID;
	}

	session = gck_object_get_session (key);
	slot = gck_session_get_slot (session);
	mechs = gck_slot_get_mechanisms (slot);
	g_object_unref (slot);
	g_object_unref (session);

	if (!mechs) {
		g_debug ("couldn't get slot mechanisms");
		return GCK_INVALID;
	}

	for (i = 0; i < n_mechanisms; i++) {
		if (gck_mechanisms_check (mechs, mechanisms[i], GCK_INVALID))
			break;
	}

	g_array_unref (mechs);

	if (i < n_mechanisms)
		return mechanisms[i];
	return GCK_INVALID;
}

gulong
_gcr_key_mechanisms_check (GckObject *key,
                           const gulong *mechanisms,
                           gsize n_mechanisms,
                           gulong action_attr_type,
                           GCancellable *cancellable,
                           GError **error)
{
	gulong attr_types[] = { action_attr_type };
	GckAttributes *attrs = NULL;
	gulong result;

	g_return_val_if_fail (GCK_IS_OBJECT (key), GCK_INVALID);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), GCK_INVALID);
	g_return_val_if_fail (error == NULL || *error == NULL, GCK_INVALID);

	if (GCK_IS_OBJECT_CACHE (key)) {
		attrs = gck_object_cache_get_attributes (GCK_OBJECT_CACHE (key));
		if (!check_have_attributes (attrs, attr_types, G_N_ELEMENTS (attr_types))) {
			gck_attributes_unref (attrs);
			attrs = NULL;
		}
	}

	if (attrs == NULL) {
		attrs = gck_object_get_full (key, attr_types, G_N_ELEMENTS (attr_types),
		                             cancellable, error);
	}

	if (!attrs)
		return GCK_INVALID;

	result = find_first_usable_mechanism (key, attrs, mechanisms, n_mechanisms, action_attr_type);
	gck_attributes_unref (attrs);
	return result;
}

typedef struct {
	gulong *mechanisms;
	gsize n_mechanisms;
	gulong action_attr_type;
} CheckClosure;

static void
check_closure_free (gpointer data)
{
	CheckClosure *closure = data;
	g_free (closure->mechanisms);
	g_free (closure);
}

static void
on_check_get_attributes (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GckAttributes *attrs;
	GError *error = NULL;

	attrs = gck_object_cache_lookup_finish (GCK_OBJECT (source), result, &error);
	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_pointer (task, attrs, gck_attributes_unref);

	g_clear_object (&task);
}

void
_gcr_key_mechanisms_check_async (GckObject *key,
                                 const gulong *mechanisms,
                                 gsize n_mechanisms,
                                 gulong action_attr_type,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	gulong attr_types[] = { action_attr_type };
	CheckClosure *closure;
	GTask *task;

	g_return_if_fail (GCK_IS_OBJECT (key));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (key, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gcr_key_mechanisms_check_async);
	closure = g_new0 (CheckClosure, 1);
	closure->mechanisms = g_memdup2 (mechanisms, n_mechanisms * sizeof (gulong));
	closure->n_mechanisms = n_mechanisms;
	closure->action_attr_type = action_attr_type;
	g_task_set_task_data (task, closure, check_closure_free);

	gck_object_cache_lookup_async (key, attr_types, G_N_ELEMENTS (attr_types),
	                               cancellable, on_check_get_attributes,
	                               g_steal_pointer (&task));

	g_clear_object (&task);
}

gulong
_gcr_key_mechanisms_check_finish (GckObject *key,
                                  GAsyncResult *result,
                                  GError **error)
{
	CheckClosure *closure;
	GckAttributes *attrs;
	gulong ret = GCK_INVALID;

	g_return_val_if_fail (GCK_IS_OBJECT (key), GCK_INVALID);
	g_return_val_if_fail (error == NULL || *error == NULL, GCK_INVALID);

	g_return_val_if_fail (g_task_is_valid (result, key), GCK_INVALID);
	g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) ==
	                      _gcr_key_mechanisms_check_async, GCK_INVALID);

	closure = g_task_get_task_data (G_TASK (result));

	attrs = g_task_propagate_pointer (G_TASK (result), error);
	if (!attrs)
		return GCK_INVALID;

	ret = find_first_usable_mechanism (GCK_OBJECT (key), attrs,
	                                   closure->mechanisms, closure->n_mechanisms,
	                                   closure->action_attr_type);

	gck_attributes_unref (attrs);
	return ret;
}
