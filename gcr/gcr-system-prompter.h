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

#ifndef __GCR_SYSTEM_PROMPTER_H__
#define __GCR_SYSTEM_PROMPTER_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-prompt.h"
#include "gcr-secret-exchange.h"
#include "gcr-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GCR_SYSTEM_PROMPTER_SINGLE,
	GCR_SYSTEM_PROMPTER_MULTIPLE
} GcrSystemPrompterMode;

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

	/*< private >*/
	GcrSystemPrompterPrivate *pv;
};

struct _GcrSystemPrompterClass {
	GObjectClass parent_class;

	/* signals */

	GcrPrompt *    (* new_prompt)     (GcrSystemPrompter *self);

	/*< private >*/
	gpointer padding[7];
};

GType                  gcr_system_prompter_get_type                (void) G_GNUC_CONST;

GcrSystemPrompter *    gcr_system_prompter_new                     (GcrSystemPrompterMode mode,
                                                                    GType prompt_type);

GcrSystemPrompterMode  gcr_system_prompter_get_mode                (GcrSystemPrompter *self);

GType                  gcr_system_prompter_get_prompt_type         (GcrSystemPrompter *self);

gboolean               gcr_system_prompter_get_prompting           (GcrSystemPrompter *self);

void                   gcr_system_prompter_register                (GcrSystemPrompter *self,
                                                                    GDBusConnection *connection);

void                   gcr_system_prompter_unregister              (GcrSystemPrompter *self,
                                                                    gboolean wait);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrSystemPrompter, g_object_unref)

G_END_DECLS

#endif /* __GCR_SYSTEM_PROMPTER_H__ */
