/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Collabora Ltd
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "gcr-dialog-util.h"

#include <string.h>

typedef struct {
	GtkDialog *dialog;
	gboolean was_modal;
	gboolean destroyed;
	gulong response_sig;
	gulong unmap_sig;
	gulong delete_sig;
	gulong destroy_sig;
} DialogRunClosure;

static void
dialog_run_closure_free (gpointer data)
{
	DialogRunClosure *closure = data;
	g_object_unref (closure->dialog);
	g_assert (closure->response_sig == 0);
	g_assert (closure->unmap_sig == 0);
	g_assert (closure->delete_sig == 0);
	g_assert (closure->destroy_sig == 0);
	g_free (closure);
}

static void
complete_task (GTask *task, int response_id)
{
	DialogRunClosure *closure = g_task_get_task_data (task);

	g_object_ref (task);

	if (!closure->destroyed) {
		if (!closure->was_modal)
			gtk_window_set_modal (GTK_WINDOW (closure->dialog), FALSE);

		g_signal_handler_disconnect (closure->dialog, closure->response_sig);
		closure->response_sig = 0;
		g_signal_handler_disconnect (closure->dialog, closure->unmap_sig);
		closure->unmap_sig = 0;
		g_signal_handler_disconnect (closure->dialog, closure->delete_sig);
		closure->delete_sig = 0;
		g_signal_handler_disconnect (closure->dialog, closure->destroy_sig);
		closure->destroy_sig = 0;
	}

	g_task_return_int (task, response_id);
	g_clear_object (&task);
}

static void
on_dialog_unmap (GtkDialog *dialog,
                 gpointer user_data)
{
	GTask *task = G_TASK (user_data);

	complete_task (task, GTK_RESPONSE_NONE);
}

static void
on_dialog_response (GtkDialog *dialog,
                    gint response_id,
                    gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	complete_task (task, response_id);
}

static gint
on_dialog_delete (GtkDialog *dialog,
                  GdkEventAny *event,
                  gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	complete_task (task, GTK_RESPONSE_NONE);
	return TRUE; /* Do not destroy */
}

static void
on_dialog_destroy (GtkDialog *dialog,
                   gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	DialogRunClosure *closure = g_task_get_task_data (task);

	/* complete will be called by run_unmap_handler */
	closure->destroyed = TRUE;
}

void
_gcr_dialog_util_run_async (GtkDialog *dialog,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
	GTask *task;
	DialogRunClosure *closure;

	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (dialog, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gcr_dialog_util_run_async);

	closure = g_new0 (DialogRunClosure, 1);
	closure->dialog = g_object_ref (dialog);
	closure->was_modal = gtk_window_get_modal (GTK_WINDOW (dialog));
	if (!closure->was_modal)
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	if (!gtk_widget_get_visible (GTK_WIDGET (dialog)))
		gtk_widget_show (GTK_WIDGET (dialog));

	g_task_set_task_data (task, closure, dialog_run_closure_free);

	closure->response_sig = g_signal_connect_data (dialog, "response",
	                                               G_CALLBACK (on_dialog_response),
	                                               g_object_ref (task),
	                                               (GClosureNotify)g_object_unref, 0);

	closure->unmap_sig = g_signal_connect_data (dialog, "unmap",
	                                            G_CALLBACK (on_dialog_unmap),
	                                            g_object_ref (task),
	                                            (GClosureNotify)g_object_unref, 0);

	closure->delete_sig = g_signal_connect_data (dialog, "delete-event",
	                                             G_CALLBACK (on_dialog_delete),
	                                             g_object_ref (task),
	                                             (GClosureNotify)g_object_unref, 0);

	closure->destroy_sig = g_signal_connect_data (dialog, "destroy",
	                                              G_CALLBACK (on_dialog_destroy),
	                                              g_object_ref (task),
	                                              (GClosureNotify)g_object_unref, 0);

	g_clear_object (&task);
}


gint
_gcr_dialog_util_run_finish (GtkDialog *dialog,
                             GAsyncResult *result)
{
	g_return_val_if_fail (g_task_is_valid (result, dialog), GTK_RESPONSE_NONE);

	return g_task_propagate_int (G_TASK (result), NULL);
}
