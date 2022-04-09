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

#ifndef __GCR_PROMPT_H__
#define __GCR_PROMPT_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GCR_PROMPT_REPLY_CANCEL = 0,
	GCR_PROMPT_REPLY_CONTINUE = 1,
} GcrPromptReply;

#define GCR_TYPE_PROMPT gcr_prompt_get_type ()
G_DECLARE_INTERFACE (GcrPrompt, gcr_prompt, GCR, PROMPT, GObject)

struct _GcrPromptInterface {
	GTypeInterface parent_iface;

	void               (* prompt_password_async)    (GcrPrompt *prompt,
	                                                 GCancellable *cancellable,
	                                                 GAsyncReadyCallback callback,
	                                                 gpointer user_data);

	const gchar *      (* prompt_password_finish)   (GcrPrompt *prompt,
	                                                 GAsyncResult *result,
	                                                 GError **error);

	void               (* prompt_confirm_async)     (GcrPrompt *prompt,
	                                                 GCancellable *cancellable,
	                                                 GAsyncReadyCallback callback,
	                                                 gpointer user_data);

	GcrPromptReply     (* prompt_confirm_finish)    (GcrPrompt *prompt,
	                                                 GAsyncResult *result,
	                                                 GError **error);

	void               (* prompt_close)             (GcrPrompt *prompt);
};

void                 gcr_prompt_reset                     (GcrPrompt *prompt);

gchar *              gcr_prompt_get_title                 (GcrPrompt *prompt);

void                 gcr_prompt_set_title                 (GcrPrompt *prompt,
                                                           const gchar *title);

gchar *              gcr_prompt_get_message               (GcrPrompt *prompt);

void                 gcr_prompt_set_message               (GcrPrompt *prompt,
                                                           const gchar *message);

gchar *              gcr_prompt_get_description           (GcrPrompt *prompt);

void                 gcr_prompt_set_description           (GcrPrompt *prompt,
                                                           const gchar *description);

gchar *              gcr_prompt_get_warning               (GcrPrompt *prompt);

void                 gcr_prompt_set_warning               (GcrPrompt *prompt,
                                                           const gchar *warning);

gchar *              gcr_prompt_get_choice_label          (GcrPrompt *prompt);

void                 gcr_prompt_set_choice_label          (GcrPrompt *prompt,
                                                           const gchar *choice_label);

gboolean             gcr_prompt_get_choice_chosen         (GcrPrompt *prompt);

void                 gcr_prompt_set_choice_chosen         (GcrPrompt *prompt,
                                                           gboolean chosen);

gboolean             gcr_prompt_get_password_new          (GcrPrompt *prompt);

void                 gcr_prompt_set_password_new          (GcrPrompt *prompt,
                                                           gboolean new_password);

gint                 gcr_prompt_get_password_strength     (GcrPrompt *prompt);

gchar *              gcr_prompt_get_caller_window         (GcrPrompt *prompt);

void                 gcr_prompt_set_caller_window         (GcrPrompt *prompt,
                                                           const gchar *window_id);

gchar *              gcr_prompt_get_continue_label        (GcrPrompt *prompt);

void                 gcr_prompt_set_continue_label        (GcrPrompt *prompt,
                                                           const gchar *continue_label);

gchar *              gcr_prompt_get_cancel_label          (GcrPrompt *prompt);

void                 gcr_prompt_set_cancel_label          (GcrPrompt *prompt,
                                                           const gchar *cancel_label);

void                 gcr_prompt_password_async            (GcrPrompt *prompt,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

const gchar *        gcr_prompt_password_finish           (GcrPrompt *prompt,
                                                           GAsyncResult *result,
                                                           GError **error);

const gchar *        gcr_prompt_password                  (GcrPrompt *prompt,
                                                           GCancellable *cancellable,
                                                           GError **error);

const gchar *        gcr_prompt_password_run              (GcrPrompt *prompt,
                                                           GCancellable *cancellable,
                                                           GError **error);

void                 gcr_prompt_confirm_async             (GcrPrompt *prompt,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

GcrPromptReply       gcr_prompt_confirm_finish            (GcrPrompt *prompt,
                                                           GAsyncResult *result,
                                                           GError **error);

GcrPromptReply       gcr_prompt_confirm                   (GcrPrompt *prompt,
                                                           GCancellable *cancellable,
                                                           GError **error);

GcrPromptReply       gcr_prompt_confirm_run               (GcrPrompt *prompt,
                                                           GCancellable *cancellable,
                                                           GError **error);

void                 gcr_prompt_close                     (GcrPrompt *prompt);

G_END_DECLS

#endif /* __GCR_PROMPT_H__ */
