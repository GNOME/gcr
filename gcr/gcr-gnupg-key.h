/*
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

#ifndef GCR_GNUPG_KEY_H
#define GCR_GNUPG_KEY_H

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <glib-object.h>

#include "gcr-types.h"

G_BEGIN_DECLS

#define GCR_TYPE_GNUPG_KEY               (_gcr_gnupg_key_get_type ())
#define GCR_GNUPG_KEY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_GNUPG_KEY, GcrGnupgKey))
#define GCR_GNUPG_KEY_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_GNUPG_KEY, GcrGnupgKeyClass))
#define GCR_IS_GNUPG_KEY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_GNUPG_KEY))
#define GCR_IS_GNUPG_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_GNUPG_KEY))
#define GCR_GNUPG_KEY_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_GNUPG_KEY, GcrGnupgKeyClass))

typedef struct _GcrGnupgKey GcrGnupgKey;
typedef struct _GcrGnupgKeyClass GcrGnupgKeyClass;
typedef struct _GcrGnupgKeyPrivate GcrGnupgKeyPrivate;

struct _GcrGnupgKey {
	/*< private >*/
	GObject parent;
	GcrGnupgKeyPrivate *pv;
};

struct _GcrGnupgKeyClass {
	GObjectClass parent_class;
};

GType               _gcr_gnupg_key_get_type                      (void);

GcrGnupgKey*        _gcr_gnupg_key_new                           (GPtrArray *pubset,
                                                                  GPtrArray *secset);

const gchar*        _gcr_gnupg_key_get_keyid                     (GcrGnupgKey *self);

GPtrArray*          _gcr_gnupg_key_get_public_records            (GcrGnupgKey *self);

void                _gcr_gnupg_key_set_public_records            (GcrGnupgKey *self,
                                                                  GPtrArray *records);

GPtrArray*          _gcr_gnupg_key_get_secret_records            (GcrGnupgKey *self);

void                _gcr_gnupg_key_set_secret_records            (GcrGnupgKey *self,
                                                                  GPtrArray *records);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrGnupgKey, g_object_unref)

G_END_DECLS

#endif /* __GCR_GNUPG_KEY_H__ */
