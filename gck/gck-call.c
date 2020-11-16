/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-call.c - the GObject PKCS#11 wrapper library

   Copyright (C) 2008, Stefan Walter
   Copyright (C) 2020, Marco Trevisan

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <nielsen@memberwebs.com>
*/

#include "config.h"

#include "gck-private.h"

#include <string.h>

struct _GckCall {
	GObject parent;
	GTask *task;
	GckModule *module;

	/* For making the call */
	GckPerformFunc perform;
	GckCompleteFunc complete;
	GckArguments *args;
	GDestroyNotify destroy;
};

G_DEFINE_TYPE (GckCall, _gck_call, G_TYPE_OBJECT)

/* ----------------------------------------------------------------------------
 * HELPER FUNCTIONS
 */

static CK_RV
perform_call (GckPerformFunc func, GCancellable *cancellable, GckArguments *args)
{
	CK_RV rv;

	/* Double check a few things */
	g_assert (func);
	g_assert (args);

	if (cancellable) {
		if (g_cancellable_is_cancelled (cancellable)) {
			return CKR_FUNCTION_CANCELED;
		}

		/* Push for the notify callback */
		g_object_ref (cancellable);
		g_cancellable_push_current (cancellable);
	}

	rv = (func) (args);

	if (cancellable) {
		g_cancellable_pop_current (cancellable);
		g_object_unref (cancellable);
	}

	return rv;
}

static gboolean
complete_call (GckCompleteFunc func, GckArguments *args, CK_RV result)
{
	/* Double check a few things */
	g_assert (args);

	/* If no complete function, then just ignore */
	if (!func)
		return TRUE;

	return (func) (args, result);
}

static CK_RV
perform_call_chain (GckPerformFunc perform, GckCompleteFunc complete,
		    GCancellable *cancellable, GckArguments *args)
{
	CK_RV rv;

	do {
		rv = perform_call (perform, cancellable, args);
		if (rv == CKR_FUNCTION_CANCELED)
			break;
	} while (!complete_call (complete, args, rv));

	return rv;
}


static void
_gck_task_return (GTask *task, CK_RV rv)
{
	if (rv == CKR_OK) {
		g_task_return_boolean (task, TRUE);
	} else if (rv == CKR_FUNCTION_CANCELED) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
					 "Gck function call cancelled");
	} else {
		g_task_return_new_error (task, GCK_ERROR, rv, "%s",
					 gck_message_from_rv (rv));
	}
}

static void
_gck_call_thread_func (GTask        *task,
		       gpointer      source_object,
		       gpointer      task_data,
		       GCancellable *cancellable)
{
	GckCall *call = task_data;
	CK_RV rv;

	/* Double check a few things */
	g_assert (GCK_IS_CALL (call));

	rv = perform_call_chain (call->perform, call->complete, cancellable,
	                         call->args);

	_gck_task_return (task, rv);
}

/* ----------------------------------------------------------------------------
 * OBJECT
 */

static void
_gck_call_init (GckCall *call)
{
}

static void
_gck_call_finalize (GObject *obj)
{
	GckCall *call = GCK_CALL (obj);

	if (call->module)
		g_object_unref (call->module);
	call->module = NULL;

	g_clear_object (&call->task);

	if (call->destroy)
		(call->destroy) (call->args);
	call->destroy = NULL;
	call->args = NULL;

	G_OBJECT_CLASS (_gck_call_parent_class)->finalize (obj);
}

static void
_gck_call_class_init (GckCallClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;

	gobject_class->finalize = _gck_call_finalize;
}

/* ----------------------------------------------------------------------------
 * PUBLIC
 */

void
_gck_call_uninitialize (void)
{

}

gboolean
_gck_call_sync (gpointer object, gpointer perform, gpointer complete,
                 gpointer data, GCancellable *cancellable, GError **err)
{
	GckArguments *args = (GckArguments*)data;
	GckModule *module = NULL;
	CK_RV rv;

	g_assert (!object || G_IS_OBJECT (object));
	g_assert (perform);
	g_assert (args);

	if (object) {
		g_object_get (object, "module", &module, "handle", &args->handle, NULL);
		g_assert (GCK_IS_MODULE (module));

		/* We now hold a reference to module until below */
		args->pkcs11 = gck_module_get_functions (module);
		g_assert (args->pkcs11);
	}

	rv = perform_call_chain (perform, complete, cancellable, args);

	if (module)
		g_object_unref (module);

	if (rv == CKR_OK)
		return TRUE;

	g_set_error (err, GCK_ERROR, rv, "%s", gck_message_from_rv (rv));
	return FALSE;
}

GckCall*
_gck_call_async_prep (gpointer object, gpointer perform, gpointer complete,
                       gsize args_size, gpointer destroy)
{
	GckArguments *args;
	GckCall *call;

	g_assert (!object || G_IS_OBJECT (object));
	g_assert (perform);

	if (!destroy)
		destroy = g_free;

	if (args_size == 0)
		args_size = sizeof (GckArguments);
	g_assert (args_size >= sizeof (GckArguments));

	args = g_malloc0 (args_size);
	call = g_object_new (GCK_TYPE_CALL, NULL);
	call->destroy = (GDestroyNotify)destroy;
	call->perform = (GckPerformFunc)perform;
	call->complete = (GckCompleteFunc)complete;

	/* Hook the two together */
	call->args = args;

	/* Setup call object if available */
	if (object != NULL)
		_gck_call_async_object (call, object);

	return call;
}

void
_gck_call_async_object (GckCall *call, gpointer object)
{
	g_assert (GCK_IS_CALL (call));
	g_assert (call->args);

	if (call->module)
		g_object_unref (call->module);
	call->module = NULL;

	g_object_get (object, "module", &call->module, "handle", &call->args->handle, NULL);
	g_assert (GCK_IS_MODULE (call->module));
	call->args->pkcs11 = gck_module_get_functions (call->module);

	/* We now hold a reference on module until finalize */
}

GckCall*
_gck_call_async_ready (GckCall *call, gpointer cb_object,
                       GCancellable *cancellable, GAsyncReadyCallback callback,
                       gpointer user_data)
{
	GTask* task;

	g_assert (GCK_IS_CALL (call));
	g_assert (call->args && "GckCall not prepared");
	g_assert (!cb_object || G_IS_OBJECT (cb_object));

	g_object_ref (call);

	task = g_task_new (cb_object, cancellable, callback, user_data);
	g_task_set_task_data (task, call, g_object_unref);
	g_set_object (&call->task, task);

	g_object_unref (task);
	g_object_unref (call);

	return call;
}

void
_gck_call_async_go (GckCall *call)
{
	g_assert (GCK_IS_CALL (call));
	g_assert (G_IS_TASK (call->task));

	g_task_run_in_thread (call->task, _gck_call_thread_func);
	g_clear_object (&call->task);
}

void
_gck_call_async_ready_go (GckCall *call, gpointer cb_object,
                           GCancellable *cancellable,
			   GAsyncReadyCallback callback, gpointer user_data)
{
	_gck_call_async_ready (call, cb_object, cancellable, callback, user_data);
	_gck_call_async_go (call);
}

gboolean
_gck_call_basic_finish (GAsyncResult *result, GError **err)
{
	g_return_val_if_fail (G_IS_TASK (result), FALSE);

	return g_task_propagate_boolean (G_TASK (result), err);
}

void
_gck_call_async_short (GckCall *call, CK_RV rv)
{
	g_assert (GCK_IS_CALL (call));

	/* Already complete, so just push it for processing in main loop */
	_gck_task_return (call->task, rv);
	g_clear_object (&call->task);

	g_main_context_wakeup (NULL);
}

gpointer
_gck_call_get_arguments (GckCall *call)
{
	g_assert (GCK_IS_CALL (call));
	return call->args;
}
