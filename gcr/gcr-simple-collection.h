/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Stefan Walter
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
 */

#ifndef __GCR_SIMPLE_COLLECTION_H__
#define __GCR_SIMPLE_COLLECTION_H__

#include "gcr-collection.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_SIMPLE_COLLECTION               (gcr_simple_collection_get_type ())
#define GCR_SIMPLE_COLLECTION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_COLLECTION, GcrSimpleCollection))
#define GCR_SIMPLE_COLLECTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_COLLECTION, GcrSimpleCollectionClass))
#define GCR_IS_SIMPLE_COLLECTION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_COLLECTION))
#define GCR_IS_SIMPLE_COLLECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_COLLECTION))
#define GCR_SIMPLE_COLLECTION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_COLLECTION, GcrSimpleCollectionClass))

typedef struct _GcrSimpleCollection GcrSimpleCollection;
typedef struct _GcrSimpleCollectionClass GcrSimpleCollectionClass;
typedef struct _GcrSimpleCollectionPrivate GcrSimpleCollectionPrivate;

struct _GcrSimpleCollection {
	GObject parent;

	/*< private >*/
	GcrSimpleCollectionPrivate *pv;
};

struct _GcrSimpleCollectionClass {
	GObjectClass parent_class;
};

GType               gcr_simple_collection_get_type                (void);

GcrCollection*      gcr_simple_collection_new                     (void);

void                gcr_simple_collection_add                     (GcrSimpleCollection *self,
                                                                   GObject *object);

void                gcr_simple_collection_remove                  (GcrSimpleCollection *self,
                                                                   GObject *object);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrSimpleCollection, g_object_unref)

G_END_DECLS

#endif /* __GCR_SIMPLE_COLLECTION_H__ */
