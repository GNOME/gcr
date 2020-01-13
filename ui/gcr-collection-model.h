/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Stefan Walter
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

#ifndef __GCR_COLLECTION_MODEL_H__
#define __GCR_COLLECTION_MODEL_H__

#include <gtk/gtk.h>

#include "gcr/gcr-collection.h"
#include "gcr/gcr-column.h"

typedef enum {
	GCR_COLLECTION_MODEL_LIST = 0,
	GCR_COLLECTION_MODEL_TREE
} GcrCollectionModelMode;

#define GCR_TYPE_COLLECTION_MODEL               (gcr_collection_model_get_type ())
#define GCR_COLLECTION_MODEL(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_COLLECTION_MODEL, GcrCollectionModel))
#define GCR_COLLECTION_MODEL_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_COLLECTION_MODEL, GcrCollectionModelClass))
#define GCR_IS_COLLECTION_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_COLLECTION_MODEL))
#define GCR_IS_COLLECTION_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_COLLECTION_MODEL))
#define GCR_COLLECTION_MODEL_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_COLLECTION_MODEL, GcrCollectionModelClass))

typedef struct _GcrCollectionModel GcrCollectionModel;
typedef struct _GcrCollectionModelClass GcrCollectionModelClass;
typedef struct _GcrCollectionModelPrivate GcrCollectionModelPrivate;

struct _GcrCollectionModel {
	GObject parent;

	/*< private >*/
	GcrCollectionModelPrivate *pv;
};

struct _GcrCollectionModelClass {
	GObjectClass parent_class;
};

GType                 gcr_collection_model_get_type            (void);

GcrCollectionModel*   gcr_collection_model_new                 (GcrCollection *collection,
                                                                GcrCollectionModelMode mode,
                                                                ...) G_GNUC_NULL_TERMINATED;

GcrCollectionModel*   gcr_collection_model_new_full            (GcrCollection *collection,
                                                                GcrCollectionModelMode mode,
                                                                const GcrColumn *columns);

guint                 gcr_collection_model_set_columns         (GcrCollectionModel *self,
                                                                const GcrColumn *columns);

GcrCollection *       gcr_collection_model_get_collection      (GcrCollectionModel *self);

void                  gcr_collection_model_set_collection      (GcrCollectionModel *self,
                                                                GcrCollection *collection);

GObject*              gcr_collection_model_object_for_iter     (GcrCollectionModel *self,
                                                                const GtkTreeIter *iter);

gboolean              gcr_collection_model_iter_for_object     (GcrCollectionModel *self,
                                                                GObject *object,
                                                                GtkTreeIter *iter);

gint                  gcr_collection_model_column_for_selected (GcrCollectionModel *self);

void                  gcr_collection_model_toggle_selected     (GcrCollectionModel *self,
                                                                GtkTreeIter *iter);

void                  gcr_collection_model_change_selected     (GcrCollectionModel *self,
                                                                GtkTreeIter *iter,
                                                                gboolean selected);

gboolean              gcr_collection_model_is_selected         (GcrCollectionModel *self,
                                                                GtkTreeIter *iter);

GList*                gcr_collection_model_get_selected_objects  (GcrCollectionModel *self);

void                  gcr_collection_model_set_selected_objects  (GcrCollectionModel *self,
                                                                  GList *selected);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrCollectionModel, g_object_unref)

#endif /* __GCR_COLLECTION_MODEL_H__ */
