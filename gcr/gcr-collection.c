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

#include "config.h"

#include "gcr-collection.h"

/**
 * GcrCollection:
 *
 * A #GcrCollection is used to group a set of objects.
 *
 * This is an abstract interface which can be used to determine which objects
 * show up in a selector or other user interface element.
 *
 * Use [ctor@SimpleCollection.new] to create a concrete implementation of this
 * interface which you can add objects to.
 */

enum {
	ADDED,
	REMOVED,
	LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };


typedef GcrCollectionIface GcrCollectionInterface;

G_DEFINE_INTERFACE (GcrCollection, gcr_collection, G_TYPE_OBJECT);

static void
gcr_collection_default_init (GcrCollectionIface *iface)
{
	static size_t initialized = 0;

	if (g_once_init_enter (&initialized)) {

		/**
		 * GcrCollection::added:
		 * @self: the collection
		 * @object: (type GObject.Object): object that was added
		 *
		 * This signal is emitted when an object is added to the collection.
		 */
		signals[ADDED] = g_signal_new ("added", GCR_TYPE_COLLECTION,
		                               G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GcrCollectionIface, added),
		                               NULL, NULL, NULL,
		                               G_TYPE_NONE, 1, G_TYPE_OBJECT);

		/**
		 * GcrCollection::removed:
		 * @self: the collection
		 * @object: (type GObject.Object): object that was removed
		 *
		 * This signal is emitted when an object is removed from the collection.
		 */
		signals[REMOVED] = g_signal_new ("removed", GCR_TYPE_COLLECTION,
		                                 G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GcrCollectionIface, removed),
		                                 NULL, NULL, NULL,
		                                 G_TYPE_NONE, 1, G_TYPE_OBJECT);

		g_once_init_leave (&initialized, 1);
	}
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */


/**
 * gcr_collection_get_length:
 * @self: The collection
 *
 * Get the number of objects in this collection.
 *
 * Returns: The number of objects.
 */
guint
gcr_collection_get_length (GcrCollection *self)
{
	g_return_val_if_fail (GCR_IS_COLLECTION (self), 0);
	g_return_val_if_fail (GCR_COLLECTION_GET_INTERFACE (self)->get_length, 0);
	return GCR_COLLECTION_GET_INTERFACE (self)->get_length (self);
}

/**
 * gcr_collection_get_objects:
 * @self: The collection
 *
 * Get a list of the objects in this collection.
 *
 * Returns: (transfer container) (element-type GObject.Object): a list of the objects
 *          in this collection, which should be freed with g_list_free()
 */
GList*
gcr_collection_get_objects (GcrCollection *self)
{
	g_return_val_if_fail (GCR_IS_COLLECTION (self), 0);
	g_return_val_if_fail (GCR_COLLECTION_GET_INTERFACE (self)->get_objects, 0);
	return GCR_COLLECTION_GET_INTERFACE (self)->get_objects (self);
}

/**
 * gcr_collection_contains:
 * @self: the collection
 * @object: object to check
 *
 * Check whether the collection contains an object or not.
 *
 * Returns: whether the collection contains this object
 */
gboolean
gcr_collection_contains (GcrCollection *self,
                         GObject *object)
{
	g_return_val_if_fail (GCR_IS_COLLECTION (self), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	g_return_val_if_fail (GCR_COLLECTION_GET_INTERFACE (self)->contains, FALSE);
	return GCR_COLLECTION_GET_INTERFACE (self)->contains (self, object);
}

/**
 * gcr_collection_emit_added:
 * @self: The collection
 * @object: The object that was added
 *
 * Emit the #GcrCollection::added signal for the given object. This function
 * is used by implementors of this interface.
 */
void
gcr_collection_emit_added (GcrCollection *self, GObject *object)
{
	g_return_if_fail (GCR_IS_COLLECTION (self));
	g_signal_emit (self, signals[ADDED], 0, object);
}

/**
 * gcr_collection_emit_removed:
 * @self: The collection
 * @object: The object that was removed
 *
 * Emit the #GcrCollection::removed signal for the given object. This function
 * is used by implementors of this interface.
 */
void
gcr_collection_emit_removed (GcrCollection *self, GObject *object)
{
	g_return_if_fail (GCR_IS_COLLECTION (self));
	g_signal_emit (self, signals[REMOVED], 0, object);
}
