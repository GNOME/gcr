/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-object.c - the GObject PKCS#11 wrapper library

   Copyright (C) 2008, Stefan Walter

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

   Author: Stef Walter <nielsen@memberwebs.com>
*/

#include "config.h"

#include "gck.h"
#include "gck-private.h"

#include <string.h>

/**
 * GckObject:
 *
 * Holds a handle to a PKCS11 object such as a key or certificate. Token
 * objects are stored on the token persistently. Others are transient and are
 * called session objects.
 */

/**
 * GckObjectClass:
 * @parent: derived from this
 *
 * The class for a [class@Object].
 *
 * If the @attribute_types field is set by a derived class, then the a
 * #GckEnumerator which has been setup using [method@Enumerator.set_object_type]
 * with this derived type will retrieve these attributes when enumerating. In
 * this case the class must implement an 'attributes' property of boxed type
 * `GCK_TYPE_ATTRIBUTES`.
 */

/*
 * MT safe -- Nothing in GckObjectData changes between
 * init and finalize. All GckObjectPrivate access between init
 * and finalize is locked.
 */

enum {
	PROP_0,
	PROP_MODULE,
	PROP_SESSION,
	PROP_HANDLE
};

typedef struct {
	GckModule *module;
	GckSession *session;
	CK_OBJECT_HANDLE handle;
} GckObjectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GckObject, gck_object, G_TYPE_OBJECT);

/* ----------------------------------------------------------------------------
 * OBJECT
 */

static void
gck_object_init (GckObject *self)
{
}

static void
gck_object_get_property (GObject *obj, guint prop_id, GValue *value,
                          GParamSpec *pspec)
{
	GckObject *self = GCK_OBJECT (obj);

	switch (prop_id) {
	case PROP_MODULE:
		g_value_take_object (value, gck_object_get_module (self));
		break;
	case PROP_SESSION:
		g_value_take_object (value, gck_object_get_session (self));
		break;
	case PROP_HANDLE:
		g_value_set_ulong (value, gck_object_get_handle (self));
		break;
	}
}

static void
gck_object_set_property (GObject *obj, guint prop_id, const GValue *value,
                         GParamSpec *pspec)
{
	GckObject *self = GCK_OBJECT (obj);
	GckObjectPrivate *priv = gck_object_get_instance_private (self);

	/* The sets to data below are only allowed during construction */

	switch (prop_id) {
	case PROP_MODULE:
		g_return_if_fail (!priv->module);
		priv->module = g_value_dup_object (value);
		g_return_if_fail (priv->module);
		break;
	case PROP_SESSION:
		g_return_if_fail (!priv->session);
		priv->session = g_value_dup_object (value);
		g_return_if_fail (priv->session);
		break;
	case PROP_HANDLE:
		g_return_if_fail (!priv->handle);
		priv->handle = g_value_get_ulong (value);
		break;
	}
}

static void
gck_object_finalize (GObject *obj)
{
	GckObject *self = GCK_OBJECT (obj);
	GckObjectPrivate *priv = gck_object_get_instance_private (self);

	g_clear_object (&priv->session);
	g_clear_object (&priv->module);
	priv->handle = 0;

	G_OBJECT_CLASS (gck_object_parent_class)->finalize (obj);
}


static void
gck_object_class_init (GckObjectClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	gck_object_parent_class = g_type_class_peek_parent (klass);

	gobject_class->get_property = gck_object_get_property;
	gobject_class->set_property = gck_object_set_property;
	gobject_class->finalize = gck_object_finalize;

	/**
	 * GckObject:module:
	 *
	 * The GckModule that this object belongs to.
	 */
	g_object_class_install_property (gobject_class, PROP_MODULE,
		g_param_spec_object ("module", "Module", "PKCS11 Module",
		                     GCK_TYPE_MODULE,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GckObject:handle:
	 *
	 * The raw PKCS11 handle for this object.
	 */
	g_object_class_install_property (gobject_class, PROP_HANDLE,
		g_param_spec_ulong ("handle", "Object Handle", "PKCS11 Object Handle",
		                   0, G_MAXULONG, 0,
		                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GckObject:session:
	 *
	 * The PKCS11 session to make calls on when this object needs to
	 * perform operations on itself.
	 *
	 * If this is NULL then a new session is opened for each operation,
	 * such as gck_object_get(), gck_object_set() or gck_object_destroy().
	 */
	g_object_class_install_property (gobject_class, PROP_SESSION,
		g_param_spec_object ("session", "session", "PKCS11 Session to make calls on",
		                     GCK_TYPE_SESSION,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

/* ----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gck_object_from_handle: (constructor)
 * @session: The session through which this object is accessed or created.
 * @object_handle: The raw `CK_OBJECT_HANDLE` of the object.
 *
 * Initialize a GckObject from a raw PKCS#11 handle. Normally you would use
 * [method@Session.create_object] or [method@Session.find_objects] to access
 * objects.
 *
 * Return value: (transfer full): The new object
 **/
GckObject *
gck_object_from_handle (GckSession *session,
                        gulong object_handle)
{
	GckModule *module = NULL;
	GckObject *object;

	g_return_val_if_fail (GCK_IS_SESSION (session), NULL);

	module = gck_session_get_module (session);
	object = g_object_new (GCK_TYPE_OBJECT,
	                       "module", module,
	                       "handle", object_handle,
	                       "session", session,
	                       NULL);
	g_object_unref (module);

	return object;
}

/**
 * gck_objects_from_handle_array:
 * @session: The session for these objects
 * @object_handles: (array length=n_object_handles): The raw object handles.
 * @n_object_handles: The number of raw object handles.
 *
 * Initialize a list of GckObject from raw PKCS#11 handles. The handles argument must contain
 * contiguous CK_OBJECT_HANDLE handles in an array.
 *
 * Returns: (transfer full) (element-type Gck.Object): The list of #GckObject
 *          objects.
 **/
GList *
gck_objects_from_handle_array (GckSession *session,
                               gulong *object_handles,
                               gulong n_object_handles)
{
	GList *results = NULL;
	CK_ULONG i;

	g_return_val_if_fail (GCK_IS_SESSION (session), NULL);
	g_return_val_if_fail (n_object_handles == 0 || object_handles != NULL, NULL);

	for (i = 0; i < n_object_handles; ++i)
		results = g_list_prepend (results, gck_object_from_handle (session, object_handles[i]));
	return g_list_reverse (results);
}

/**
 * gck_object_equal:
 * @object1: a pointer to the first #GckObject
 * @object2: a pointer to the second #GckObject
 *
 * Checks equality of two objects. Two GckObject objects can point to the same
 * underlying PKCS#11 object.
 *
 * Return value: %TRUE if object1 and object2 are equal.
 *               %FALSE if either is not a GckObject.
 **/
gboolean
gck_object_equal (GckObject *object1, GckObject *object2)
{
	GckObjectPrivate *priv1 = gck_object_get_instance_private (object1);
	GckObjectPrivate *priv2 = gck_object_get_instance_private (object2);
	GckSlot *slot1, *slot2;
	gboolean ret;

	if (object1 == object2)
		return TRUE;
	if (!GCK_IS_OBJECT (object1) || !GCK_IS_OBJECT (object2))
		return FALSE;

	slot1 = gck_session_get_slot (priv1->session);
	slot2 = gck_session_get_slot (priv2->session);

	ret = priv1->handle == priv2->handle &&
	      gck_slot_equal (slot1, slot2);

	g_object_unref (slot1);
	g_object_unref (slot2);

	return ret;
}

/**
 * gck_object_hash:
 * @object: (type Gck.Object): a pointer to a #GckObject
 *
 * Create a hash value for the GckObject.
 *
 * This function is intended for easily hashing a GckObject to add to
 * a GHashTable or similar data structure.
 *
 * Return value: An integer that can be used as a hash value, or 0 if invalid.
 **/
guint
gck_object_hash (GckObject *object)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (object);
	GckSlot *slot;
	guint hash;

	g_return_val_if_fail (GCK_IS_OBJECT (object), 0);

	slot = gck_session_get_slot (priv->session);

	hash = _gck_ulong_hash (&priv->handle) ^
	       gck_slot_hash (slot);

	g_object_unref (slot);

	return hash;
}


/**
 * gck_object_get_handle:
 * @self: The object.
 *
 * Get the raw PKCS#11 handle of a GckObject.
 *
 * Return value: the raw CK_OBJECT_HANDLE object handle
 **/
gulong
gck_object_get_handle (GckObject *self)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_OBJECT (self), (CK_OBJECT_HANDLE)-1);

	return priv->handle;
}

/**
 * gck_object_get_module:
 * @self: The object.
 *
 * Get the PKCS#11 module to which this object belongs.
 *
 * Returns: (transfer full): the module, which should be unreffed after use
 **/
GckModule *
gck_object_get_module (GckObject *self)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (GCK_IS_MODULE (priv->module), NULL);

	return g_object_ref (priv->module);
}


/**
 * gck_object_get_session:
 * @self: The object
 *
 * Get the PKCS#11 session assigned to make calls on when operating
 * on this object.
 *
 * This will only return a session if it was set explitly on this
 * object. By default an object will open and close sessions
 * appropriate for its calls.
 *
 * Returns: (transfer full): the assigned session, which must be unreffed after use
 **/
GckSession *
gck_object_get_session (GckObject *self)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (GCK_IS_SESSION (priv->session), NULL);

	return g_object_ref (priv->session);
}

/* --------------------------------------------------------------------------------------
 * DESTROY
 */

typedef struct _Destroy {
	GckArguments base;
	CK_OBJECT_HANDLE object;
} Destroy;

static CK_RV
perform_destroy (Destroy *args)
{
	g_assert (args);
	return (args->base.pkcs11->C_DestroyObject) (args->base.handle, args->object);
}

/**
 * gck_object_destroy:
 * @self: The object to destroy.
 * @cancellable: Optional cancellable object, or %NULL to ignore.
 * @error: A location to return an error.
 *
 * Destroy a PKCS#11 object, deleting it from storage or the session.
 * This call may block for an indefinite period.
 *
 * Return value: Whether the call was successful or not.
 **/
gboolean
gck_object_destroy (GckObject *self, GCancellable *cancellable, GError **error)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	Destroy args = { GCK_ARGUMENTS_INIT, 0 };

	g_return_val_if_fail (GCK_IS_OBJECT (self), FALSE);
	g_return_val_if_fail (GCK_IS_SESSION (priv->session), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	args.object = priv->handle;
	return _gck_call_sync (priv->session, perform_destroy, NULL, &args, cancellable, error);
}

/**
 * gck_object_destroy_async:
 * @self: The object to destroy.
 * @cancellable: Optional cancellable object, or %NULL to ignore.
 * @callback: Callback which is called when operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Destroy a PKCS#11 object, deleting it from storage or the session.
 * This call will return immediately and complete asynchronously.
 **/
void
gck_object_destroy_async (GckObject *self, GCancellable *cancellable,
                           GAsyncReadyCallback callback, gpointer user_data)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GckCall *call;
	Destroy* args;

	g_return_if_fail (GCK_IS_OBJECT (self));
	g_return_if_fail (GCK_IS_SESSION (priv->session));

	call = _gck_call_async_prep (priv->session, perform_destroy, NULL, sizeof (*args), NULL);
	args = _gck_call_get_arguments (call);
	args->object = priv->handle;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_object_destroy_finish:
 * @self: The object being destroyed.
 * @result: The result of the destory operation passed to the callback.
 * @error: A location to store an error.
 *
 * Get the status of the operation to destroy a PKCS#11 object, begun with
 * gck_object_destroy_async().
 *
 * Return value: Whether the object was destroyed successfully or not.
 */
gboolean
gck_object_destroy_finish (GckObject *self, GAsyncResult *result, GError **error)
{
	g_return_val_if_fail (GCK_IS_OBJECT (self), FALSE);
	g_return_val_if_fail (G_IS_TASK (result), FALSE);
	return _gck_call_basic_finish (result, error);
}

/* --------------------------------------------------------------------------------------
 * SET ATTRIBUTES
 */

typedef struct _SetAttributes {
	GckArguments base;
	GckAttributes *attrs;
	CK_OBJECT_HANDLE object;
} SetAttributes;

static CK_RV
perform_set_attributes (SetAttributes *args)
{
	CK_ATTRIBUTE_PTR attrs;
	CK_ULONG n_attrs;

	g_assert (args);
	attrs = _gck_attributes_commit_out (args->attrs, &n_attrs);

	return (args->base.pkcs11->C_SetAttributeValue) (args->base.handle, args->object,
	                                                 attrs, n_attrs);
}

static void
free_set_attributes (SetAttributes *args)
{
	g_assert (args);
	gck_attributes_unref (args->attrs);
	g_free (args);
}

/**
 * gck_object_set:
 * @self: The object to set attributes on.
 * @attrs: The attributes to set on the object.
 * @cancellable: Optional cancellable object, or %NULL to ignore.
 * @error: A location to return an error.
 *
 * Set PKCS#11 attributes on an object. This call may block for an indefinite period.
 *
 * Return value: Whether the call was successful or not.
 **/
gboolean
gck_object_set (GckObject *self, GckAttributes *attrs,
                GCancellable *cancellable, GError **error)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	SetAttributes args;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCK_IS_OBJECT (self), FALSE);
	g_return_val_if_fail (attrs != NULL, FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	memset (&args, 0, sizeof (args));
	args.attrs = attrs;
	args.object = priv->handle;

	ret = _gck_call_sync (priv->session, perform_set_attributes, NULL, &args, cancellable, error);

	return ret;
}

/**
 * gck_object_set_async:
 * @self: The object to set attributes on.
 * @attrs: The attributes to set on the object.
 * @cancellable: Optional cancellable object, or %NULL to ignore.
 * @callback: Callback which is called when operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Set PKCS#11 attributes on an object. This call will return
 * immediately and completes asynchronously.
 **/
void
gck_object_set_async (GckObject *self, GckAttributes *attrs, GCancellable *cancellable,
                       GAsyncReadyCallback callback, gpointer user_data)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GckCall *call;
	SetAttributes *args;

	g_return_if_fail (GCK_IS_OBJECT (self));
	g_return_if_fail (attrs != NULL);

	call = _gck_call_async_prep (priv->session, perform_set_attributes,
	                             NULL, sizeof (*args), free_set_attributes);

	args = _gck_call_get_arguments (call);
	args->attrs = gck_attributes_ref (attrs);
	args->object = priv->handle;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_object_set_finish:
 * @self: The object to set attributes on.
 * @result: The result of the destory operation passed to the callback.
 * @error: A location to store an error.
 *
 * Get the status of the operation to set attributes on a PKCS#11 object,
 * begun with gck_object_set_async().
 *
 * Return value: Whether the attributes were successfully set on the object or not.
 */
gboolean
gck_object_set_finish (GckObject *self, GAsyncResult *result, GError **error)
{
	SetAttributes *args;

	g_return_val_if_fail (GCK_IS_OBJECT (self), FALSE);
	g_return_val_if_fail (G_IS_TASK (result), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	/* Unlock the attributes we were using */
	args = _gck_call_async_result_arguments (result, SetAttributes);
	g_assert (args->attrs);

	return _gck_call_basic_finish (result, error);
}

/* ------------------------------------------------------------------------------------
 * GET ATTRIBUTES
 */

typedef struct _GetAttributes {
	GckArguments base;
	CK_OBJECT_HANDLE object;
	GckBuilder builder;
} GetAttributes;

static CK_RV
perform_get_attributes (GetAttributes *args)
{
	CK_ATTRIBUTE_PTR attrs;
	CK_ULONG n_attrs;
	CK_RV rv;

	g_assert (args != NULL);

	/* Prepare all the attributes */
	attrs = _gck_builder_prepare_in (&args->builder, &n_attrs);

	/* Get the size of each value */
	rv = (args->base.pkcs11->C_GetAttributeValue) (args->base.handle, args->object,
	                                               attrs, n_attrs);
	if (!GCK_IS_GET_ATTRIBUTE_RV_OK (rv))
		return rv;

	/* Allocate memory for each value */
	attrs = _gck_builder_commit_in (&args->builder, &n_attrs);

	/* Now get the actual values */
	rv = (args->base.pkcs11->C_GetAttributeValue) (args->base.handle, args->object,
	                                               attrs, n_attrs);

	if (GCK_IS_GET_ATTRIBUTE_RV_OK (rv))
		rv = CKR_OK;

	return rv;
}

static void
free_get_attributes (GetAttributes *args)
{
	g_assert (args != NULL);
	gck_builder_clear (&args->builder);
	g_free (args);
}


/**
 * gck_object_get:
 * @self: The object to get attributes from.
 * @cancellable: A #GCancellable or %NULL
 * @error: A location to store an error.
 * @...: The attribute types to get.
 *
 * Get the specified attributes from the object. This call may
 * block for an indefinite period.
 *
 * Returns: (transfer full): the resulting PKCS#11 attributes, or %NULL if an
 *          error occurred; the result must be unreffed when you're finished
 *          with it
 **/
GckAttributes *
gck_object_get (GckObject *self, GCancellable *cancellable, GError **error, ...)
{
	GckAttributes *attrs;
	GArray *array;
	va_list va;
	gulong type;

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	array = g_array_new (FALSE, TRUE, sizeof (gulong));
	va_start (va, error);
	for (;;) {
		type = va_arg (va, gulong);
		if (type == GCK_INVALID)
			break;
		g_array_append_val (array, type);
	}
	va_end (va);

	attrs = gck_object_get_full (self, (gulong*)array->data, array->len, cancellable, error);
	g_array_free (array, TRUE);

	return attrs;
}

/**
 * gck_object_get_full:
 * @self: The object to get attributes from.
 * @attr_types: (array length=n_attr_types): the types of the attributes to get
 * @n_attr_types: the number of attr_types
 * @cancellable: optional cancellation object, or %NULL
 * @error: A location to store an error.
 *
 * Get the specified attributes from the object. This call may
 * block for an indefinite period.
 *
 * No extra references are added to the returned attributes pointer.
 * During this call you may not access the attributes in any way.
 *
 * Returns: (transfer full): a pointer to the filled in attributes if successful,
 *          or %NULL if not
 **/
GckAttributes *
gck_object_get_full (GckObject *self,
                     const gulong *attr_types,
                     guint n_attr_types,
                     GCancellable *cancellable,
                     GError **error)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GetAttributes args;
	gboolean ret;
	guint i;

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	memset (&args, 0, sizeof (args));

	gck_builder_init (&args.builder);
	for (i = 0; i < n_attr_types; ++i)
		gck_builder_add_empty (&args.builder, attr_types[i]);

	args.object = priv->handle;

	ret = _gck_call_sync (priv->session, perform_get_attributes, NULL, &args, cancellable, error);

	if (ret) {
		return gck_builder_end (&args.builder);
	} else {
		gck_builder_clear (&args.builder);
		return NULL;
	}
}

/**
 * gck_object_get_async:
 * @self: The object to get attributes from.
 * @attr_types: (array length=n_attr_types): the types of the attributes to get
 * @n_attr_types: the number of attr_types
 * @cancellable: optional cancellation object, or %NULL
 * @callback: A callback which is called when the operation completes.
 * @user_data: Data to be passed to the callback.
 *
 * Get the specified attributes from the object. The attributes will be cleared
 * of their current values, and new attributes will be stored. The attributes
 * should not be accessed in any way except for referencing and unreferencing
 * them until gck_object_get_finish() is called.
 *
 * This call returns immediately and completes asynchronously.
 **/
void
gck_object_get_async (GckObject *self,
                      const gulong *attr_types,
                      guint n_attr_types,
                      GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GckCall *call;
	GetAttributes *args;
	guint i;

	g_return_if_fail (GCK_IS_OBJECT (self));

	call = _gck_call_async_prep (priv->session, perform_get_attributes,
	                             NULL, sizeof (*args), free_get_attributes);

	args = _gck_call_get_arguments (call);
	gck_builder_init (&args->builder);
	for (i = 0; i < n_attr_types; ++i)
		gck_builder_add_empty (&args->builder, attr_types[i]);

	args->object = priv->handle;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_object_get_finish:
 * @self: The object to get attributes from.
 * @result: The result passed to the callback.
 * @error: A location to store an error.
 *
 * Get the result of a get operation and return specified attributes from
 * the object.
 *
 * No extra references are added to the returned attributes pointer.
 *
 * Return value: The filled in attributes structure if successful or
 * %NULL if not successful.
 **/
GckAttributes*
gck_object_get_finish (GckObject *self, GAsyncResult *result, GError **error)
{
	GetAttributes *args;

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (G_IS_TASK (result), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	args = _gck_call_async_result_arguments (result, GetAttributes);

	if (!_gck_call_basic_finish (result, error))
		return NULL;

	return gck_builder_end (&args->builder);
}

/* ---------------------------------------------------------------------------------
 * GET ATTRIBUTE DATA
 */

typedef struct _GetAttributeData {
	GckArguments base;
	CK_OBJECT_HANDLE object;
	CK_ATTRIBUTE_TYPE type;
	GckAllocator allocator;
	guchar *result;
	gsize n_result;
} GetAttributeData;

static CK_RV
perform_get_attribute_data (GetAttributeData *args)
{
	CK_ATTRIBUTE attr;
	CK_RV rv;

	g_assert (args);
	g_assert (args->allocator);

	attr.type = args->type;
	attr.ulValueLen = 0;
	attr.pValue = 0;

	/* Get the size of the value */
	rv = (args->base.pkcs11->C_GetAttributeValue) (args->base.handle, args->object,
	                                               &attr, 1);
	if (rv != CKR_OK)
		return rv;

	/* Allocate memory for the value */
	args->result = (args->allocator) (NULL, attr.ulValueLen + 1);
	g_assert (args->result);
	attr.pValue = args->result;

	/* Now get the actual value */
	rv = (args->base.pkcs11->C_GetAttributeValue) (args->base.handle, args->object,
	                                               &attr, 1);

	if (rv == CKR_OK) {
		args->n_result = attr.ulValueLen;
		args->result[args->n_result] = 0;
	}

	return rv;
}

static void
free_get_attribute_data (GetAttributeData *args)
{
	g_assert (args);
	g_free (args->result);
	g_free (args);
}

/**
 * gck_object_get_data:
 * @self: The object to get attribute data from.
 * @attr_type: The attribute to get data for.
 * @cancellable: A #GCancellable or %NULL
 * @n_data: The length of the resulting data.
 * @error: A location to store an error.
 *
 * Get the data for the specified attribute from the object. For convenience
 * the returned data has a null terminator.
 *
 * This call may block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_data): the resulting PKCS#11
 *          attribute data, or %NULL if an error occurred
 **/
guchar *
gck_object_get_data (GckObject *self,
                     gulong attr_type,
                     GCancellable *cancellable,
                     gsize *n_data,
                     GError **error)
{
	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (n_data, NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	return gck_object_get_data_full (self, attr_type, g_realloc, cancellable, n_data, error);
}

/**
 * gck_object_get_data_full: (skip)
 * @self: The object to get attribute data from.
 * @attr_type: The attribute to get data for.
 * @allocator: An allocator with which to allocate memory for the data, or %NULL for default.
 * @cancellable: Optional cancellation object, or %NULL.
 * @n_data: The length of the resulting data.
 * @error: A location to store an error.
 *
 * Get the data for the specified attribute from the object. For convenience
 * the returned data has an extra null terminator, not included in the returned length.
 *
 * This call may block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_data): The resulting PKCS#11
 *          attribute data, or %NULL if an error occurred.
 **/
guchar *
gck_object_get_data_full (GckObject *self, gulong attr_type, GckAllocator allocator,
                           GCancellable *cancellable, gsize *n_data, GError **error)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GetAttributeData args;
	gboolean ret;

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (n_data, NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (!allocator)
		allocator = g_realloc;

	memset (&args, 0, sizeof (args));
	args.allocator = allocator;
	args.object = priv->handle;
	args.type = attr_type;

	ret = _gck_call_sync (priv->session, perform_get_attribute_data, NULL, &args, cancellable, error);

	/* Free any value if failed */
	if (!ret) {
		if (args.result)
			(allocator) (args.result, 0);
		return NULL;
	}

	*n_data = args.n_result;
	return args.result;
}

/**
 * gck_object_get_data_async:
 * @self: The object to get attribute data from.
 * @attr_type: The attribute to get data for.
 * @allocator: (skip): An allocator with which to allocate memory for the data, or %NULL for default.
 * @cancellable: Optional cancellation object, or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to be passed to the callback.
 *
 * Get the data for the specified attribute from the object.
 *
 * This call will return immediately and complete asynchronously.
 **/
void
gck_object_get_data_async (GckObject *self, gulong attr_type, GckAllocator allocator,
                            GCancellable *cancellable, GAsyncReadyCallback callback,
                            gpointer user_data)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GckCall *call;
	GetAttributeData *args;

	g_return_if_fail (GCK_IS_OBJECT (self));

	if (!allocator)
		allocator = g_realloc;

	call = _gck_call_async_prep (priv->session, perform_get_attribute_data,
	                             NULL, sizeof (*args), free_get_attribute_data);

	args = _gck_call_get_arguments (call);
	args->allocator = allocator;
	args->object = priv->handle;
	args->type = attr_type;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_object_get_data_finish:
 * @self: The object to get an attribute from.
 * @result: The result passed to the callback.
 * @n_data: The length of the resulting data.
 * @error: A location to store an error.
 *
 * Get the result of an operation to get attribute data from
 * an object. For convenience the returned data has an extra null terminator,
 * not included in the returned length.
 *
 * Returns: (transfer full) (array length=n_data): The PKCS#11 attribute data
 *          or %NULL if an error occurred.
 **/
guchar *
gck_object_get_data_finish (GckObject *self,
                            GAsyncResult *result,
                            gsize *n_data,
                            GError **error)
{
	GetAttributeData *args;
	guchar *data;

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (G_IS_TASK (result), NULL);
	g_return_val_if_fail (n_data, NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (!_gck_call_basic_finish (result, error))
		return NULL;

	args = _gck_call_async_result_arguments (result, GetAttributeData);

	*n_data = args->n_result;
	data = args->result;
	args->result = NULL;

	return data;
}

/* ---------------------------------------------------------------------------------------
 * SET TEMPLATE
 */

typedef struct _set_template_args {
	GckArguments base;
	CK_OBJECT_HANDLE object;
	CK_ATTRIBUTE_TYPE type;
	GckAttributes *attrs;
} set_template_args;

static CK_RV
perform_set_template (set_template_args *args)
{
	CK_ATTRIBUTE attr;
	CK_ULONG n_attrs;

	g_assert (args);

	attr.type = args->type;
	attr.pValue = _gck_attributes_commit_out (args->attrs, &n_attrs);
	attr.ulValueLen = n_attrs * sizeof (CK_ATTRIBUTE);

	return (args->base.pkcs11->C_SetAttributeValue) (args->base.handle, args->object, &attr, 1);
}

static void
free_set_template (set_template_args *args)
{
	g_assert (args);
	gck_attributes_unref (args->attrs);
	g_free (args);
}

/**
 * gck_object_set_template:
 * @self: The object to set an attribute template on.
 * @attr_type: The attribute template type.
 * @attrs: The attribute template.
 * @cancellable: Optional cancellation object, or %NULL.
 * @error: A location to store an error.
 *
 * Set an attribute template on the object. The attr_type must be for
 * an attribute which contains a template.
 *
 * If the @attrs #GckAttributes is floating, it is consumed.
 *
 * This call may block for an indefinite period.
 *
 * Return value: %TRUE if the operation succeeded.
 **/
gboolean
gck_object_set_template (GckObject *self, gulong attr_type, GckAttributes *attrs,
                         GCancellable *cancellable, GError **error)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	set_template_args args;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCK_IS_OBJECT (self), FALSE);
	g_return_val_if_fail (attrs, FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	memset (&args, 0, sizeof (args));
	args.attrs = attrs;
	args.type = attr_type;
	args.object = priv->handle;

	ret = _gck_call_sync (priv->session, perform_set_template, NULL, &args, cancellable, error);

	return ret;
}

/**
 * gck_object_set_template_async:
 * @self: The object to set an attribute template on.
 * @attr_type: The attribute template type.
 * @attrs: The attribute template.
 * @cancellable: Optional cancellation object, or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to be passed to the callback.
 *
 * Set an attribute template on the object. The attr_type must be for
 * an attribute which contains a template.
 *
 * If the @attrs #GckAttributes is floating, it is consumed.
 *
 * This call will return immediately and complete asynchronously.
 **/
void
gck_object_set_template_async (GckObject *self, gulong attr_type, GckAttributes *attrs,
                                GCancellable *cancellable, GAsyncReadyCallback callback,
                                gpointer user_data)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GckCall *call;
	set_template_args *args;

	g_return_if_fail (GCK_IS_OBJECT (self));
	g_return_if_fail (attrs);

	call = _gck_call_async_prep (priv->session, perform_set_template,
	                             NULL, sizeof (*args), free_set_template);

	args = _gck_call_get_arguments (call);
	args->attrs = gck_attributes_ref (attrs);
	args->type = attr_type;
	args->object = priv->handle;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_object_set_template_finish:
 * @self: The object to set an attribute template on.
 * @result: The result passed to the callback.
 * @error: A location to store an error.
 *
 * Get the result of an operation to set attribute template on
 * an object.
 *
 * Return value: %TRUE if the operation succeeded.
 **/
gboolean
gck_object_set_template_finish (GckObject *self, GAsyncResult *result, GError **error)
{
	set_template_args *args;

	g_return_val_if_fail (GCK_IS_OBJECT (self), FALSE);
	g_return_val_if_fail (G_IS_TASK (result), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	/* Unlock the attributes we were using */
	args = _gck_call_async_result_arguments (result, set_template_args);
	g_assert (args->attrs);

	return _gck_call_basic_finish (result, error);
}

/* ---------------------------------------------------------------------------------------
 * GET TEMPLATE
 */

typedef struct _get_template_args {
	GckArguments base;
	CK_OBJECT_HANDLE object;
	CK_ATTRIBUTE_TYPE type;
	GckBuilder builder;
} get_template_args;

static CK_RV
perform_get_template (get_template_args *args)
{
	CK_ATTRIBUTE attr;
	CK_ULONG n_attrs, i;
	CK_RV rv;

	g_assert (args);

	gck_builder_init (&args->builder);
	attr.type = args->type;
	attr.ulValueLen = 0;
	attr.pValue = 0;

	/* Get the length of the entire template */
	rv = (args->base.pkcs11->C_GetAttributeValue) (args->base.handle, args->object, &attr, 1);
	if (rv != CKR_OK)
		return rv;

	/* Number of attributes, rounded down */
	n_attrs = (attr.ulValueLen / sizeof (CK_ATTRIBUTE));
	for (i = 0; i < n_attrs; ++i)
		gck_builder_add_empty (&args->builder, 0);

	/* Prepare all the attributes */
	attr.pValue = _gck_builder_prepare_in (&args->builder, &n_attrs);

	/* Get the size of each value */
	rv = (args->base.pkcs11->C_GetAttributeValue) (args->base.handle, args->object, &attr, 1);
	if (rv != CKR_OK)
		return rv;

	/* Allocate memory for each value */
	attr.pValue = _gck_builder_commit_in (&args->builder, &n_attrs);

	/* Now get the actual values */
	return (args->base.pkcs11->C_GetAttributeValue) (args->base.handle, args->object, &attr, 1);
}

static void
free_get_template (get_template_args *args)
{
	g_assert (args != NULL);
	gck_builder_clear (&args->builder);
	g_free (args);
}

/**
 * gck_object_get_template:
 * @self: The object to get an attribute template from.
 * @attr_type: The template attribute type.
 * @cancellable: Optional cancellation object, or %NULL.
 * @error: A location to store an error.
 *
 * Get an attribute template from the object. The attr_type must be for
 * an attribute which returns a template.
 *
 * This call may block for an indefinite period.
 *
 * Returns: (transfer full): the resulting PKCS#11 attribute template, or %NULL
 *          if an error occurred
 **/
GckAttributes *
gck_object_get_template (GckObject *self, gulong attr_type,
                         GCancellable *cancellable, GError **error)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	get_template_args args;
	gboolean ret;

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	memset (&args, 0, sizeof (args));
	args.object = priv->handle;
	args.type = attr_type;

	ret = _gck_call_sync (priv->session, perform_get_template, NULL, &args, cancellable, error);

	/* Free any value if failed */
	if (!ret) {
		gck_builder_clear (&args.builder);
		return NULL;
	}

	return gck_builder_end (&args.builder);
}

/**
 * gck_object_get_template_async:
 * @self: The object to get an attribute template from.
 * @attr_type: The template attribute type.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to be passed to the callback.
 *
 * Get an attribute template from the object. The @attr_type must be for
 * an attribute which returns a template.
 *
 * This call will return immediately and complete asynchronously.
 **/
void
gck_object_get_template_async (GckObject *self, gulong attr_type,
                               GCancellable *cancellable, GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GckObjectPrivate *priv = gck_object_get_instance_private (self);
	GckCall *call;
	get_template_args *args;

	g_return_if_fail (GCK_IS_OBJECT (self));

	call = _gck_call_async_prep (priv->session, perform_get_template,
	                             NULL, sizeof (*args), free_get_template);

	args = _gck_call_get_arguments (call);
	args->object = priv->handle;
	args->type = attr_type;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_object_get_template_finish:
 * @self: The object to get an attribute from.
 * @result: The result passed to the callback.
 * @error: A location to store an error.
 *
 * Get the result of an operation to get attribute template from
 * an object.
 *
 * Returns: (transfer full): the resulting PKCS#11 attribute template, or %NULL
 *          if an error occurred
 **/
GckAttributes *
gck_object_get_template_finish (GckObject *self, GAsyncResult *result,
                                GError **error)
{
	get_template_args *args;

	g_return_val_if_fail (GCK_IS_OBJECT (self), NULL);
	g_return_val_if_fail (G_IS_TASK (result), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (!_gck_call_basic_finish (result, error))
		return NULL;

	args = _gck_call_async_result_arguments (result, get_template_args);
	return gck_builder_end (&args->builder);
}
