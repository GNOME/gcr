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

#ifndef __GCR_FILTER_COLLECTION_H__
#define __GCR_FILTER_COLLECTION_H__

#include "gcr-collection.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_FILTER_COLLECTION               (gcr_filter_collection_get_type ())
#define GCR_FILTER_COLLECTION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_FILTER_COLLECTION, GcrFilterCollection))
#define GCR_FILTER_COLLECTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_FILTER_COLLECTION, GcrFilterCollectionClass))
#define GCR_IS_FILTER_COLLECTION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_FILTER_COLLECTION))
#define GCR_IS_FILTER_COLLECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_FILTER_COLLECTION))
#define GCR_FILTER_COLLECTION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_FILTER_COLLECTION, GcrFilterCollectionClass))

typedef struct _GcrFilterCollection GcrFilterCollection;
typedef struct _GcrFilterCollectionClass GcrFilterCollectionClass;
typedef struct _GcrFilterCollectionPrivate GcrFilterCollectionPrivate;

struct _GcrFilterCollection {
	GObject parent;

	/*< private >*/
	GcrFilterCollectionPrivate *pv;
};

struct _GcrFilterCollectionClass {
	GObjectClass parent_class;
};

GType               gcr_filter_collection_get_type                (void);

typedef gboolean    (* GcrFilterCollectionFunc)                   (GObject *object,
                                                                   gpointer user_data);

GcrCollection *     gcr_filter_collection_new_with_callback       (GcrCollection *underlying,
                                                                   GcrFilterCollectionFunc callback,
                                                                   gpointer user_data,
                                                                   GDestroyNotify destroy_func);

void                gcr_filter_collection_set_callback            (GcrFilterCollection *self,
                                                                   GcrFilterCollectionFunc callback,
                                                                   gpointer user_data,
                                                                   GDestroyNotify destroy_func);

void                gcr_filter_collection_refilter                (GcrFilterCollection *self);

GcrCollection *     gcr_filter_collection_get_underlying          (GcrFilterCollection *self);

G_END_DECLS

#endif /* __GCR_FILTER_COLLECTION_H__ */
