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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_PROMPT_DIALOG_H__
#define __GCR_PROMPT_DIALOG_H__

#include <gtk/gtk.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_PROMPT_DIALOG               (gcr_prompt_dialog_get_type ())
#define GCR_PROMPT_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_PROMPT_DIALOG, GcrPromptDialog))
#define GCR_PROMPT_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_PROMPT_DIALOG, GcrPromptDialogClass))
#define GCR_IS_PROMPT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_PROMPT_DIALOG))
#define GCR_IS_PROMPT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_PROMPT_DIALOG))
#define GCR_PROMPT_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_PROMPT_DIALOG, GcrPromptDialogClass))

typedef struct _GcrPromptDialog GcrPromptDialog;
typedef struct _GcrPromptDialogClass GcrPromptDialogClass;
typedef struct _GcrPromptDialogPrivate GcrPromptDialogPrivate;

struct _GcrPromptDialog {
	GtkDialog parent;

	/*< private >*/
	GcrPromptDialogPrivate *pv;
};

struct _GcrPromptDialogClass {
	GtkDialogClass parent_class;
};

GType                gcr_prompt_dialog_get_type                  (void);

G_END_DECLS

#endif /* __GCR_PROMPT_DIALOG_H__ */
