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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_SYSTEM_PROMPTER_H__
#define __GCR_SYSTEM_PROMPTER_H__

#include "gcr-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_SYSTEM_PROMPTER               (gcr_system_prompter_get_type ())
#define GCR_SYSTEM_PROMPTER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_SYSTEM_PROMPTER, GcrSystemPrompter))
#define GCR_SYSTEM_PROMPTER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_SYSTEM_PROMPTER, GcrSystemPrompterClass))
#define GCR_IS_SYSTEM_PROMPTER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_SYSTEM_PROMPTER))
#define GCR_IS_SYSTEM_PROMPTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_SYSTEM_PROMPTER))
#define GCR_SYSTEM_PROMPTER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_SYSTEM_PROMPTER, GcrSystemPrompterClass))

typedef struct _GcrSystemPrompter GcrSystemPrompter;
typedef struct _GcrSystemPrompterClass GcrSystemPrompterClass;
typedef struct _GcrSystemPrompterPrivate GcrSystemPrompterPrivate;

struct _GcrSystemPrompter {
	GObject parent;
	GcrSystemPrompterPrivate *pv;
};

struct _GcrSystemPrompterClass {
	GObjectClass parent_class;

	void         (*show_prompt)             (GcrSystemPrompter *self);

	gboolean     (*prompt_password)         (GcrSystemPrompter *self);

	gboolean     (*prompt_confirm)          (GcrSystemPrompter *self);

	void         (*responded)               (GcrSystemPrompter *self);

	void         (*hide_prompt)             (GcrSystemPrompter *self);
};

GType                gcr_system_prompter_get_type                  (void) G_GNUC_CONST;

GcrSystemPrompter *  gcr_system_prompter_new                       (void);

void                 gcr_system_prompter_register                  (GcrSystemPrompter *self,
                                                                    GDBusConnection *connection);

void                 gcr_system_prompter_unregister                (GcrSystemPrompter *self,
                                                                    GDBusConnection *connection);

const gchar *        gcr_system_prompter_get_title                 (GcrSystemPrompter *self);

const gchar *        gcr_system_prompter_get_message               (GcrSystemPrompter *self);

const gchar *        gcr_system_prompter_get_description           (GcrSystemPrompter *self);

const gchar *        gcr_system_prompter_get_warning               (GcrSystemPrompter *self);

void                 gcr_system_prompter_set_warning               (GcrSystemPrompter *self,
                                                                    const gchar *warning);

gboolean             gcr_system_prompter_get_password_new          (GcrSystemPrompter *self);

gint                 gcr_system_prompter_get_password_strength     (GcrSystemPrompter *self);

void                 gcr_system_prompter_set_password_strength     (GcrSystemPrompter *self,
                                                                    gint strength);

const gchar *        gcr_system_prompter_get_choice_label          (GcrSystemPrompter *self);

gboolean             gcr_system_prompter_get_choice_chosen         (GcrSystemPrompter *self);

void                 gcr_system_prompter_set_choice_chosen         (GcrSystemPrompter *self,
                                                                    gboolean chosen);

const gchar *        gcr_system_prompter_get_caller_window         (GcrSystemPrompter *self);

void                 gcr_system_prompter_respond_cancelled         (GcrSystemPrompter *self);

void                 gcr_system_prompter_respond_with_password     (GcrSystemPrompter *self,
                                                                    const gchar *password);

void                 gcr_system_prompter_respond_confirmed         (GcrSystemPrompter *self);

G_END_DECLS

#endif /* __GCR_SYSTEM_PROMPTER_H__ */
