/* gck-object-cache.c - the GObject PKCS#11 wrapper library

   Copyright (C) 2011 Collabora Ltd.

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gck.h"
#include "gck-private.h"

#include <string.h>

/**
 * GckObjectCache:
 *
 * An interface implemented by derived classes of [class@Object] to indicate
 * which attributes they'd like an enumerator to retrieve.
 *
 * These attributes are then cached on the object and can be retrieved through
 * the [property@ObjectCache:attributes] property.
 */

/**
 * GckObjectCacheInterface:
 * @interface: parent interface
 * @default_types: (array length=n_default_types): attribute types that an
 *                   enumerator should retrieve
 * @n_default_types: number of attribute types to be retrieved
 * @fill: virtual method to add attributes to the cache
 *
 * Interface for [iface@ObjectCache]. If the @default_types field is
 * implemented by a implementing class, then that will be used by a
 * [class@Enumerator] which has been setup using
 * [method@Enumerator.set_object_type]
 *
 * The implementation for @populate should add the attributes to the
 * cache. It must be thread safe.
 */

G_DEFINE_INTERFACE (GckObjectCache, gck_object_cache, GCK_TYPE_OBJECT);

static void
gck_object_cache_default_init (GckObjectCacheInterface *iface)
{
	/**
	 * GckObjectCache:attributes:
	 *
	 * The attributes cached on this object.
	 */
	g_object_interface_install_property (iface,
	         g_param_spec_boxed ("attributes", "Attributes", "PKCS#11 Attributes",
	                             GCK_TYPE_ATTRIBUTES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * gck_object_cache_get_attributes: (skip):
 * @object: an object with an attribute cache
 *
 * Gets the attributes cached on this object.
 *
 * Returns: (transfer full) (nullable): the attributes
 */
GckAttributes *
gck_object_cache_get_attributes (GckObjectCache *object)
{
	GckAttributes *attributes = NULL;
	g_return_val_if_fail (GCK_IS_OBJECT_CACHE (object), NULL);
	g_object_get (object, "attributes", &attributes, NULL);
	return attributes;
}

/**
 * gck_object_cache_set_attributes:
 * @object: an object with an attribute cache
 * @attrs: (nullable): the attributes to set
 *
 * Sets the attributes cached on this object.
 */
void
gck_object_cache_set_attributes (GckObjectCache *object,
                                 GckAttributes *attrs)
{
	g_return_if_fail (GCK_IS_OBJECT_CACHE (object));

	g_object_set (object, "attributes", attrs, NULL);
}

/**
 * gck_object_cache_fill:
 * @object: an object with the cache
 * @attrs: the attributes to cache
 *
 * Adds the attributes to the set cached on this object. If an attribute is
 * already present in the cache it will be overridden by this value.
 *
 * This will be done in a thread-safe manner.
 */
void
gck_object_cache_fill (GckObjectCache *object,
                       GckAttributes *attrs)
{
	GckObjectCacheInterface *iface;

	g_return_if_fail (GCK_IS_OBJECT_CACHE (object));
	g_return_if_fail (attrs != NULL);

	iface = GCK_OBJECT_CACHE_GET_IFACE (object);
	g_return_if_fail (iface->fill != NULL);

	(iface->fill) (object, attrs);
}

/**
 * gck_object_cache_update:
 * @object: the object with the cache
 * @attr_types: (array length=n_attr_types): the types of attributes to update
 * @n_attr_types: the number of attribute types
 * @cancellable: optional cancellation object
 * @error: location to place an error
 *
 * Update the object cache with given attributes. If an attribute already
 * exists in the cache, it will be updated, and if it doesn't it will be added.
 *
 * This may block, use the asynchronous version when this is not desirable
 *
 * Returns: whether the cache update was successful
 */
gboolean
gck_object_cache_update (GckObjectCache *object,
                         const gulong *attr_types,
                         gint n_attr_types,
                         GCancellable *cancellable,
                         GError **error)
{
	GckObjectCacheInterface *iface;
	GckAttributes *attrs;

	g_return_val_if_fail (GCK_IS_OBJECT_CACHE (object), FALSE);
	g_return_val_if_fail (attr_types != NULL || n_attr_types == 0, FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	iface = GCK_OBJECT_CACHE_GET_IFACE (object);

	if (attr_types == NULL) {
		attr_types = iface->default_types;
		n_attr_types = iface->n_default_types;

		if (attr_types == NULL || n_attr_types == 0) {
			g_warning ("no attribute types passed to gck_object_cache_update() "
			           "and no default types on object.");
			return FALSE;
		}
	}

	attrs = gck_object_get_full (GCK_OBJECT (object),
	                             attr_types, n_attr_types,
	                             cancellable, error);

	if (attrs != NULL) {
		gck_object_cache_fill (object, attrs);
		gck_attributes_unref (attrs);
	}

	return attrs != NULL;
}

static void
on_cache_object_get (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GckAttributes *attrs;
	GError *error = NULL;

	attrs = gck_object_get_finish (GCK_OBJECT (source), result, &error);
	if (error == NULL) {
		gck_object_cache_fill (GCK_OBJECT_CACHE (source), attrs);
		gck_attributes_unref (attrs);
		g_task_return_boolean (task, TRUE);
	} else {
		g_task_return_error (task, g_steal_pointer (&error));
	}

	g_clear_object (&task);
}

/**
 * gck_object_cache_update_async:
 * @object: the object with the cache
 * @attr_types: (array length=n_attr_types): the types of attributes to update
 * @n_attr_types: the number of attribute types
 * @cancellable: optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to be passed to the callback
 *
 * Update the object cache with given attributes. If an attribute already
 * exists in the cache, it will be updated, and if it doesn't it will be added.
 *
 * This call will return immediately and complete asynchronously.
 */
void
gck_object_cache_update_async (GckObjectCache *object,
                               const gulong *attr_types,
                               gint n_attr_types,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GckObjectCacheInterface *iface;
	GTask *task;

	g_return_if_fail (GCK_IS_OBJECT_CACHE (object));
	g_return_if_fail (attr_types != NULL || n_attr_types == 0);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCK_OBJECT_CACHE_GET_IFACE (object);

	if (attr_types == NULL) {
		attr_types = iface->default_types;
		n_attr_types = iface->n_default_types;

		if (attr_types == NULL || n_attr_types == 0) {
			g_warning ("no attribute types passed to gck_object_cache_update_async() "
			           "and no default types on object.");
			return;
		}
	}

	task = g_task_new (object, cancellable, callback, user_data);
	g_task_set_source_tag (task, gck_object_cache_update_async);

	gck_object_get_async (GCK_OBJECT (object), attr_types, n_attr_types,
	                      cancellable, on_cache_object_get, g_steal_pointer (&task));

	g_clear_object (&task);
}

/**
 * gck_object_cache_update_finish:
 * @object: the object with the cache
 * @result: the asynchronous result passed to the callback
 * @error: location to place an error
 *
 * Complete an asynchronous operation to update the object cache with given
 * attributes.
 *
 * Returns: whether the cache update was successful
 */
gboolean
gck_object_cache_update_finish (GckObjectCache *object,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (GCK_IS_OBJECT_CACHE (object), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, object), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
check_have_attributes (GckAttributes *attrs,
                       const gulong *attr_types,
                       gint n_attr_types)
{
	gint i;

	if (attrs == NULL)
		return FALSE;

	for (i = 0; i < n_attr_types; i++) {
		if (!gck_attributes_find (attrs, attr_types[i]))
			return FALSE;
	}

	return TRUE;
}

/**
 * gck_object_cache_lookup:
 * @object: the object
 * @attr_types: (array length=n_attr_types): the types of attributes to update
 * @n_attr_types: the number of attribute types
 * @cancellable: optional cancellation object
 * @error: location to place an error
 *
 * Lookup attributes in the cache, or retrieve them from the object if necessary.
 *
 * If @object is a #GckObjectCache then this will lookup the attributes there
 * first if available, otherwise will read them from the object and update
 * the cache.
 *
 * If @object is not a #GckObjectCache, then the attributes will simply be
 * read from the object.
 *
 * This may block, use the asynchronous version when this is not desirable
 *
 * Returns: (transfer full): the attributes retrieved or %NULL on failure
 */
GckAttributes *
gck_object_cache_lookup (GckObject *object,
                         const gulong *attr_types,
                         gint n_attr_types,
                         GCancellable *cancellable,
                         GError **error)
{
	GckAttributes *attrs;
	GckObjectCache *cache;

	g_return_val_if_fail (GCK_IS_OBJECT (object), NULL);
	g_return_val_if_fail (attr_types != NULL || n_attr_types == 0, NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (GCK_IS_OBJECT_CACHE (object)) {
		cache = GCK_OBJECT_CACHE (object);
		attrs = gck_object_cache_get_attributes (cache);
		if (check_have_attributes (attrs, attr_types, n_attr_types))
			return attrs;
		gck_attributes_unref (attrs);

		if (!gck_object_cache_update (cache, attr_types, n_attr_types,
		                              cancellable, error))
			return NULL;

		return gck_object_cache_get_attributes (cache);

	} else {
		return gck_object_get_full (object, attr_types, n_attr_types,
		                            cancellable, error);
	}
}

/**
 * gck_object_cache_lookup_async:
 * @object: the object
 * @attr_types: (array length=n_attr_types): the types of attributes to update
 * @n_attr_types: the number of attribute types
 * @cancellable: optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Lookup attributes in the cache, or retrieve them from the object if necessary.
 *
 * If @object is a #GckObjectCache then this will lookup the attributes there
 * first if available, otherwise will read them from the object and update
 * the cache.
 *
 * If @object is not a #GckObjectCache, then the attributes will simply be
 * read from the object.
 *
 * This will return immediately and complete asynchronously
 */
void
gck_object_cache_lookup_async (GckObject *object,
                               const gulong *attr_types,
                               gint n_attr_types,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	g_return_if_fail (GCK_IS_OBJECT (object));
	g_return_if_fail (attr_types != NULL || n_attr_types == 0);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	if (GCK_IS_OBJECT_CACHE (object)) {
		GckObjectCache *cache;
		GckAttributes *attrs;
		gboolean have;

		cache = GCK_OBJECT_CACHE (object);
		attrs = gck_object_cache_get_attributes (cache);
		have = check_have_attributes (attrs, attr_types, n_attr_types);
		gck_attributes_unref (attrs);

		if (have) {
			GTask *task;

			task = g_task_new (cache, cancellable, callback, user_data);
			g_task_set_source_tag (task, gck_object_cache_lookup_async);
			g_task_return_boolean (task, TRUE);
			g_clear_object (&task);
		} else {
			gck_object_cache_update_async (cache, attr_types, n_attr_types,
			                               cancellable, callback, user_data);
		}
	} else {
		gck_object_get_async (object, attr_types, n_attr_types, cancellable,
		                      callback, user_data);
	}
}

/**
 * gck_object_cache_lookup_finish:
 * @object: the object
 * @result: the asynchrounous result passed to the callback
 * @error: location to place an error
 *
 * Complete an operation to lookup attributes in the cache or retrieve them
 * from the object if necessary.
 *
 * Returns: (transfer full): the attributes retrieved or %NULL on failure
 */
GckAttributes *
gck_object_cache_lookup_finish (GckObject *object,
                                GAsyncResult *result,
                                GError **error)
{
	GckObjectCache *cache;

	g_return_val_if_fail (GCK_IS_OBJECT (object), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (GCK_IS_OBJECT_CACHE (object)) {
		cache = GCK_OBJECT_CACHE (object);
		if (!g_task_is_valid (result, object))
			if (!gck_object_cache_update_finish (cache, result, error))
				return NULL;
		return gck_object_cache_get_attributes (cache);
	}

	return gck_object_get_finish (object, result, error);
}
