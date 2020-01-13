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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef GCR_GNUPG_PROCESS_H
#define GCR_GNUPG_PROCESS_H

#include "gcr-record.h"

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_GNUPG_PROCESS               (_gcr_gnupg_process_get_type ())
#define GCR_GNUPG_PROCESS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_GNUPG_PROCESS, GcrGnupgProcess))
#define GCR_GNUPG_PROCESS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_GNUPG_PROCESS, GcrGnupgProcessClass))
#define GCR_IS_GNUPG_PROCESS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_GNUPG_PROCESS))
#define GCR_IS_GNUPG_PROCESS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_GNUPG_PROCESS))
#define GCR_GNUPG_PROCESS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_GNUPG_PROCESS, GcrGnupgProcessClass))

typedef struct _GcrGnupgProcess GcrGnupgProcess;
typedef struct _GcrGnupgProcessClass GcrGnupgProcessClass;
typedef struct _GcrGnupgProcessPrivate GcrGnupgProcessPrivate;

struct _GcrGnupgProcess {
	/*< private >*/
	GObject parent;
	GcrGnupgProcessPrivate *pv;
};

struct _GcrGnupgProcessClass {
	GObjectClass parent_class;

	/* signals */
	gboolean (*error_line) (GcrGnupgProcess *self, const gchar *line);

	gboolean (*status_record) (GcrGnupgProcess *self, GcrRecord *record);
};

typedef enum {
	GCR_GNUPG_PROCESS_NONE              = 0,
	GCR_GNUPG_PROCESS_RESPECT_LOCALE    = 1 << 0,
	GCR_GNUPG_PROCESS_WITH_STATUS       = 1 << 1,
	GCR_GNUPG_PROCESS_WITH_ATTRIBUTES   = 1 << 2
} GcrGnupgProcessFlags;

GType               _gcr_gnupg_process_get_type                (void) G_GNUC_CONST;

GcrGnupgProcess*    _gcr_gnupg_process_new                     (const gchar *directory,
                                                                const gchar *executable);

const gchar *       _gcr_gnupg_process_get_directory           (GcrGnupgProcess *self);

GInputStream *      _gcr_gnupg_process_get_input_stream        (GcrGnupgProcess *self);

void                _gcr_gnupg_process_set_input_stream        (GcrGnupgProcess *self,
                                                                GInputStream *input);

GOutputStream *     _gcr_gnupg_process_get_output_stream       (GcrGnupgProcess *self);

void                _gcr_gnupg_process_set_output_stream       (GcrGnupgProcess *self,
                                                                GOutputStream *output);

GOutputStream *     _gcr_gnupg_process_get_attribute_stream    (GcrGnupgProcess *self);

void                _gcr_gnupg_process_set_attribute_stream    (GcrGnupgProcess *self,
                                                                GOutputStream *output);

void                _gcr_gnupg_process_run_async               (GcrGnupgProcess *self,
                                                                const gchar **argv,
                                                                const gchar **envp,
                                                                GcrGnupgProcessFlags flags,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

gboolean            _gcr_gnupg_process_run_finish              (GcrGnupgProcess *self,
                                                                GAsyncResult *result,
                                                                GError **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrGnupgProcess, g_object_unref)

G_END_DECLS

#endif /* GCR_GNUPG_PROCESS_H */
