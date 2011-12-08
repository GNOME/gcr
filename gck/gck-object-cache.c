/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gck.h"
#include "gck-private.h"

#include <string.h>

/**
 * SECTION:gck-object-cache
 * @title: GckObjectCache
 * @short_description: An interface which holds attributes for a PKCS\#11 object
 *
 * #GckObjectCache is an interface implemented by derived classes of
 * #GckObject to indicate which attributes they'd like an enumerator to retrieve.
 * These attributes are then cached on the object and can be retrieved through
 * the #GckObjectCache:attributes property.
 */

/**
 * GckObjectCacheIface:
 * @interface: parent interface
 * @default_types: (array length=n_default_types): attribute types that an
 *                   enumerator should retrieve
 * @n_default_types: number of attribute types to be retrieved
 *
 * Interface for #GckObjectCache. If the @default_types field is set by
 * a implementing class, then the a #GckEnumerator which has been setup using
 * gck_enumerator_set_object_type()
 */

typedef GckObjectCacheIface GckObjectCacheInterface;
G_DEFINE_INTERFACE (GckObjectCache, gck_object_cache, GCK_TYPE_OBJECT);

static void
gck_object_cache_default_init (GckObjectCacheIface *iface)
{
	static volatile gsize initialized = 0;
	if (g_once_init_enter (&initialized)) {

		/**
		 * GckObjectCache:attributes:
		 *
		 * The attributes cached on this object.
		 */
		g_object_interface_install_property (iface,
		         g_param_spec_boxed ("attributes", "Attributes", "PKCS#11 Attributes",
		                             GCK_TYPE_ATTRIBUTES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		g_once_init_leave (&initialized, 1);
	}
}

/**
 * gck_object_cache_get_attributes:
 * @object: an object with attributes
 *
 * Gets the attributes cached on this object.
 *
 * Returns: (transfer full) (allow-none): the attributes
 */
GckAttributes *
gck_object_cache_get_attributes (GckObjectCache *object)
{
	GckAttributes *attributes = NULL;
	g_return_val_if_fail (GCK_IS_OBJECT_CACHE (object), NULL);
	g_object_get (object, "attributes", &attributes, NULL);
	return attributes;
}

void
gck_object_cache_set_attributes (GckObjectCache *object,
                                 GckAttributes *attributes)
{
	g_return_if_fail (GCK_IS_OBJECT_CACHE (object));

	gck_attributes_ref_sink (attributes);
	g_object_set (object, "attributes", attributes, NULL);
	gck_attributes_unref (attributes);
}

/**
 * gck_object_cache_add_attributes:
 * @object: an object with attributes
 * @attributes: the attributes to cache
 *
 * Adds the attributes to the set cached on this object.
 */
void
gck_object_cache_add_attributes (GckObjectCache *object,
                                 GckAttributes *attributes)
{
	GckObjectCacheIface *iface;

	g_return_if_fail (GCK_IS_OBJECT_CACHE (object));
	g_return_if_fail (attributes != NULL);

	iface = GCK_OBJECT_CACHE_GET_INTERFACE (object);
	g_return_if_fail (iface->add_attributes != NULL);

	gck_attributes_ref_sink (attributes);
	(iface->add_attributes) (object, attributes);
	gck_attributes_unref (attributes);
}

gboolean
gck_object_cache_update (GckObjectCache *object,
                         const gulong *attr_types,
                         gint n_attr_types,
                         GCancellable *cancellable,
                         GError **error)
{
	GckObjectCacheIface *iface;
	GckAttributes *attrs;

	g_return_val_if_fail (GCK_IS_OBJECT_CACHE (object), FALSE);
	g_return_val_if_fail (attr_types != NULL || n_attr_types == 0, FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	iface = GCK_OBJECT_CACHE_GET_INTERFACE (object);

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
		gck_object_cache_add_attributes (object, attrs);
		gck_attributes_unref (attrs);
	}

	return attrs != NULL;
}

static void
on_cache_object_get (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GckAttributes *attrs;
	GError *error = NULL;

	attrs = gck_object_get_finish (GCK_OBJECT (source), result, &error);
	if (error == NULL) {
		gck_object_cache_add_attributes (GCK_OBJECT_CACHE (source), attrs);
		gck_attributes_unref (attrs);
	} else {
		g_simple_async_result_take_error (res, error);
	}

	g_simple_async_result_complete (res);
	g_object_unref (res);
}
void
gck_object_cache_update_async (GckObjectCache *object,
                               const gulong *attr_types,
                               gint n_attr_types,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GckObjectCacheIface *iface;
	GSimpleAsyncResult *res;

	g_return_if_fail (GCK_IS_OBJECT_CACHE (object));
	g_return_if_fail (attr_types != NULL || n_attr_types == 0);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCK_OBJECT_CACHE_GET_INTERFACE (object);

	if (attr_types == NULL) {
		attr_types = iface->default_types;
		n_attr_types = iface->n_default_types;

		if (attr_types == NULL || n_attr_types == 0) {
			g_warning ("no attribute types passed to gck_object_cache_update_async() "
			           "and no default types on object.");
			return;
		}
	}

	res = g_simple_async_result_new (G_OBJECT (object), callback, user_data,
	                                 gck_object_cache_update_async);

	gck_object_get_async (GCK_OBJECT (object), attr_types, n_attr_types,
	                      cancellable, on_cache_object_get, g_object_ref (res));

	g_object_unref (res);
}

gboolean
gck_object_cache_update_finish (GckObjectCache *object,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (GCK_IS_OBJECT_CACHE (object), FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (object),
	                      gck_object_cache_update_async), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
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

void
gck_object_cache_lookup_async (GckObject *object,
                               const gulong *attr_types,
                               gint n_attr_types,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GSimpleAsyncResult *res;
	GckAttributes *attrs;
	GckObjectCache *cache;
	gboolean have;

	g_return_if_fail (GCK_IS_OBJECT (object));
	g_return_if_fail (attr_types != NULL || n_attr_types == 0);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	if (GCK_IS_OBJECT_CACHE (object)) {
		cache = GCK_OBJECT_CACHE (object);
		attrs = gck_object_cache_get_attributes (cache);
		have = check_have_attributes (attrs, attr_types, n_attr_types);
		gck_attributes_unref (attrs);

		if (have) {
			res = g_simple_async_result_new (G_OBJECT (cache), callback, user_data,
			                                 gck_object_cache_lookup_async);
			g_simple_async_result_complete_in_idle (res);
			g_object_unref (res);
		} else {
			gck_object_cache_update_async (cache, attr_types, n_attr_types,
			                               cancellable, callback, user_data);
		}
	} else {
		gck_object_get_async (object, attr_types, n_attr_types, cancellable,
		                      callback, user_data);
	}
}

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
		if (!g_simple_async_result_is_valid (result, G_OBJECT (object),
		                                     gck_object_cache_lookup_async))
			if (!gck_object_cache_update_finish (cache, result, error))
				return NULL;
		return gck_object_cache_get_attributes (cache);
	} else {

		return gck_object_get_finish (object, result, error);
	}
}
