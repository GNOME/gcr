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

#ifndef __GCR_SYSTEM_PROMPT_H__
#define __GCR_SYSTEM_PROMPT_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-types.h"
#include "gcr-secret-exchange.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GCR_SYSTEM_PROMPT_IN_PROGRESS = 1,
} GcrSystemPromptError;

#define GCR_SYSTEM_PROMPT_ERROR              (gcr_system_prompt_error_get_domain ())

GQuark           gcr_system_prompt_error_get_domain     (void) G_GNUC_CONST;

#define GCR_TYPE_SYSTEM_PROMPT               (gcr_system_prompt_get_type ())
#define GCR_SYSTEM_PROMPT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_SYSTEM_PROMPT, GcrSystemPrompt))
#define GCR_SYSTEM_PROMPT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_SYSTEM_PROMPT, GcrSystemPromptClass))
#define GCR_IS_SYSTEM_PROMPT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_SYSTEM_PROMPT))
#define GCR_IS_SYSTEM_PROMPT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_SYSTEM_PROMPT))
#define GCR_SYSTEM_PROMPT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_SYSTEM_PROMPT, GcrSystemPromptClass))

typedef struct _GcrSystemPrompt GcrSystemPrompt;
typedef struct _GcrSystemPromptClass GcrSystemPromptClass;
typedef struct _GcrSystemPromptPrivate GcrSystemPromptPrivate;

struct _GcrSystemPrompt {
	GObject parent;

	/*< private >*/
	GcrSystemPromptPrivate *pv;
};

struct _GcrSystemPromptClass {
	GObjectClass parent_class;
};

GType                gcr_system_prompt_get_type                  (void);

GcrSecretExchange *  gcr_system_prompt_get_secret_exchange       (GcrSystemPrompt *self);

void                 gcr_system_prompt_open_async                (gint timeout_seconds,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

void                 gcr_system_prompt_open_for_prompter_async   (const gchar *prompter_name,
                                                                  gint timeout_seconds,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);


GcrPrompt *          gcr_system_prompt_open_finish               (GAsyncResult *result,
                                                                  GError **error);

GcrPrompt *          gcr_system_prompt_open_for_prompter         (const gchar *prompter_name,
                                                                  gint timeout_seconds,
                                                                  GCancellable *cancellable,
                                                                  GError **error);

GcrPrompt *          gcr_system_prompt_open                      (gint timeout_seconds,
                                                                  GCancellable *cancellable,
                                                                  GError **error);

gboolean             gcr_system_prompt_close                     (GcrSystemPrompt *self,
                                                                  GCancellable *cancellable,
                                                                  GError **error);

void                 gcr_system_prompt_close_async               (GcrSystemPrompt *self,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

gboolean             gcr_system_prompt_close_finish              (GcrSystemPrompt *self,
                                                                  GAsyncResult *result,
                                                                  GError **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrSystemPrompt, g_object_unref)

G_END_DECLS

#endif /* __GCR_SYSTEM_PROMPT_H__ */
