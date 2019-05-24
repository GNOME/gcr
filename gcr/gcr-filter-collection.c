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
#include "gcr-filter-collection.h"

#include <string.h>

/**
 * SECTION:gcr-filter-collection
 * @title: GcrFilterCollection
 * @short_description: A collection which filters a GcrCollection
 *
 * An implementation of #GcrCollection which filters objects from another
 * underlying collection. Use gcr_filter_collection_new_with_callback()
 * to create a new filter collection.
 *
 * The callback will determine the criteria for whether an object shows through
 * the filter or not.
 */

/**
 * GcrFilterCollection:
 *
 * A filter implementation of #GcrCollection.
 */

/**
 * GcrFilterCollectionClass:
 * @parent_class: the parent class
 *
 * The class for #GcrFilterCollection.
 */

/**
 * GcrFilterCollectionFunc:
 * @object: object to filter
 * @user_data: user data passed to the callback
 *
 * A function which is called by #GcrFilterCollection in order to determine
 * whether an object should show through the filter or not.
 *
 * Returns: %TRUE if an object should be included in the filtered collection
 */

enum {
	PROP_0,
	PROP_UNDERLYING
};

struct _GcrFilterCollectionPrivate {
	GHashTable *items;
	GcrCollection *underlying;
	GcrFilterCollectionFunc filter_func;
	gpointer user_data;
	GDestroyNotify destroy_func;
};

static void       gcr_filter_collection_iface       (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrFilterCollection, gcr_filter_collection, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GcrFilterCollection);
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, gcr_filter_collection_iface));

static void
add_object (GcrFilterCollection *self,
            GObject *object)
{
	g_assert (g_hash_table_lookup (self->pv->items, object) == NULL);
	g_hash_table_insert (self->pv->items, g_object_ref (object), object);
	gcr_collection_emit_added (GCR_COLLECTION (self), object);
}

static void
remove_object (GcrFilterCollection *self,
               GObject *object)
{
	g_object_ref (object);
	if (!g_hash_table_remove (self->pv->items, object))
		g_assert_not_reached ();
	gcr_collection_emit_removed (GCR_COLLECTION (self), object);
	g_object_unref (object);
}

static gboolean
filter_object (GcrFilterCollection *self,
               GObject *object)
{
	gboolean match = TRUE;

	if (self->pv->filter_func)
		match = (self->pv->filter_func) (object, self->pv->user_data);

	return match;
}

static void
on_collection_added (GcrCollection *collection,
                     GObject *object,
                     gpointer user_data)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (user_data);
	if (filter_object (self, object))
		add_object (self, object);
}

static void
on_collection_removed (GcrCollection *collection,
                       GObject *object,
                       gpointer user_data)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (user_data);
	if (g_hash_table_lookup (self->pv->items, object))
		remove_object (self, object);
}

static void
gcr_filter_collection_init (GcrFilterCollection *self)
{
	self->pv = gcr_filter_collection_get_instance_private (self);
	self->pv->items = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
}

static void
gcr_filter_collection_set_property (GObject *obj,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (obj);

	switch (property_id) {
	case PROP_UNDERLYING:
		g_return_if_fail (self->pv->underlying == NULL);
		self->pv->underlying = g_value_dup_object (value);
		g_return_if_fail (self->pv->underlying != NULL);
		g_signal_connect (self->pv->underlying, "added",
		                  G_CALLBACK (on_collection_added), self);
		g_signal_connect (self->pv->underlying, "removed",
		                  G_CALLBACK (on_collection_removed), self);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
		break;
	}
}

static void
gcr_filter_collection_get_property (GObject *obj,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (obj);

	switch (property_id) {
	case PROP_UNDERLYING:
		g_value_set_object (value, gcr_filter_collection_get_underlying (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
		break;
	}
}

static void
gcr_filter_collection_finalize (GObject *obj)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (obj);

	if (self->pv->underlying) {
		g_signal_handlers_disconnect_by_func (self->pv->underlying,
		                                      on_collection_added, self);
		g_signal_handlers_disconnect_by_func (self->pv->underlying,
		                                      on_collection_removed, self);
		g_object_unref (self->pv->underlying);
	}

	if (self->pv->destroy_func)
		(self->pv->destroy_func) (self->pv->user_data);

	g_assert (self->pv->items);
	g_hash_table_destroy (self->pv->items);
	self->pv->items = NULL;

	G_OBJECT_CLASS (gcr_filter_collection_parent_class)->finalize (obj);
}

static void
gcr_filter_collection_class_init (GcrFilterCollectionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_filter_collection_get_property;
	gobject_class->set_property = gcr_filter_collection_set_property;
	gobject_class->finalize = gcr_filter_collection_finalize;

	g_object_class_install_property (gobject_class, PROP_UNDERLYING,
	            g_param_spec_object ("underlying", "Underlying", "Underlying collection",
	                                 GCR_TYPE_COLLECTION, G_PARAM_STATIC_STRINGS |
	                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static guint
gcr_filter_collection_get_length (GcrCollection *coll)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (coll);
	return g_hash_table_size (self->pv->items);
}

static GList*
gcr_filter_collection_get_objects (GcrCollection *coll)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (coll);
	return g_hash_table_get_keys (self->pv->items);
}

static gboolean
gcr_filter_collection_contains (GcrCollection *collection,
                                GObject *object)
{
	GcrFilterCollection *self = GCR_FILTER_COLLECTION (collection);
	return g_hash_table_lookup (self->pv->items, object) ? TRUE : FALSE;
}

static void
gcr_filter_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = gcr_filter_collection_get_length;
	iface->get_objects = gcr_filter_collection_get_objects;
	iface->contains = gcr_filter_collection_contains;
}

/**
 * gcr_filter_collection_new_with_callback:
 * @underlying: the underlying collection
 * @callback: (allow-none): function to call for each object
 * @user_data: data to pass to the callback
 * @destroy_func: called for user_data when it is no longer needed
 *
 * Create a new #GcrFilterCollection.
 *
 * The callback should return %TRUE if an object should appear in the
 * filtered collection.
 *
 * If a %NULL callback is set, then all underlynig objects will appear in the
 * filtered collection.
 *
 * Returns: (transfer full) (type Gcr.FilterCollection): a newly allocated
 *          filtered collection, which should be freed with g_object_unref()
 */
GcrCollection *
gcr_filter_collection_new_with_callback (GcrCollection *underlying,
                                         GcrFilterCollectionFunc callback,
                                         gpointer user_data,
                                         GDestroyNotify destroy_func)
{
	GcrCollection *collection;

	collection = g_object_new (GCR_TYPE_FILTER_COLLECTION,
	                           "underlying", underlying,
	                           NULL);
	gcr_filter_collection_set_callback (GCR_FILTER_COLLECTION (collection),
	                                    callback, user_data, destroy_func);

	return collection;
}

/**
 * gcr_filter_collection_set_callback:
 * @self: a filter collection
 * @callback: (allow-none): function to call for each object
 * @user_data: data to pass to the callback
 * @destroy_func: called for user_data when it is no longer needed
 *
 * Set the callback used to filter the objects in the underlying collection.
 * The callback should return %TRUE if an object should appear in the
 * filtered collection.
 *
 * If a %NULL callback is set, then all underlynig objects will appear in the
 * filtered collection.
 *
 * This will refilter the collection.
 */
void
gcr_filter_collection_set_callback (GcrFilterCollection *self,
                                    GcrFilterCollectionFunc callback,
                                    gpointer user_data,
                                    GDestroyNotify destroy_func)
{
	g_return_if_fail (GCR_IS_FILTER_COLLECTION (self));

	if (self->pv->destroy_func)
		(self->pv->destroy_func) (self->pv->user_data);
	self->pv->filter_func = callback;
	self->pv->user_data = user_data;
	self->pv->destroy_func = destroy_func;

	gcr_filter_collection_refilter (self);
}

/**
 * gcr_filter_collection_refilter:
 * @self: a filter collection
 *
 * Refilter all objects in the underlying collection. Call this function if
 * the filter callback function changes its filtering criteria.
 */
void
gcr_filter_collection_refilter (GcrFilterCollection *self)
{
	GList *objects = NULL;
	GHashTable *snapshot;
	GHashTableIter iter;
	GObject *object;
	gboolean have;
	gboolean should;
	GList *l;

	g_return_if_fail (GCR_IS_FILTER_COLLECTION (self));

	snapshot = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_hash_table_iter_init (&iter, self->pv->items);
	while (g_hash_table_iter_next (&iter, (gpointer *)&object, NULL))
		g_hash_table_insert (snapshot, object, object);

	if (self->pv->underlying)
		objects = gcr_collection_get_objects (self->pv->underlying);

	for (l = objects; l != NULL; l = g_list_next (l)) {
		have = g_hash_table_remove (snapshot, l->data);
		should = filter_object (self, l->data);
		if (have && !should)
			remove_object (self, l->data);
		else if (!have && should)
			add_object (self, l->data);
	}

	g_hash_table_iter_init (&iter, snapshot);
	while (g_hash_table_iter_next (&iter, (gpointer *)&object, NULL))
		remove_object (self, object);
	g_hash_table_destroy (snapshot);

	g_list_free (objects);
}

/**
 * gcr_filter_collection_get_underlying:
 * @self: a filter collection
 *
 * Get the collection that is being filtered by this filter collection.
 *
 * Returns: (transfer none): the underlying collection
 */
GcrCollection *
gcr_filter_collection_get_underlying (GcrFilterCollection *self)
{
	g_return_val_if_fail (GCR_IS_FILTER_COLLECTION (self), NULL);
	return self->pv->underlying;
}
