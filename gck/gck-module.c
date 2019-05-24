/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-module.c - the GObject PKCS#11 wrapper library

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

#include "gck/gck-marshal.h"

#include <glib/gi18n-lib.h>

#define P11_KIT_FUTURE_UNSTABLE_API 1
#include <p11-kit/p11-kit.h>

#include <string.h>

/**
 * SECTION:gck-module
 * @title: GckModule
 * @short_description: A loaded and initialized PKCS\#11 module.
 *
 * A GckModule object holds a loaded PKCS\#11 module. A PKCS\#11 module is a shared library.
 *
 * You can load and initialize a PKCS\#11 module with the gck_module_initialize() call. If you already
 * have a loaded and initialized module that you'd like to use with the various gck functions, then
 * you can use gck_module_new().
 */

/**
 * GckModule:
 *
 * Holds a loaded and initialized PKCS\#11 module.
 */

/**
 * GckModuleInfo:
 * @pkcs11_version_major: The major version of the module.
 * @pkcs11_version_minor: The minor version of the module.
 * @manufacturer_id: The module manufacturer.
 * @flags: The module PKCS&num;11 flags.
 * @library_description: The module description.
 * @library_version_major: The major version of the library.
 * @library_version_minor: The minor version of the library.
 *
 * Holds information about the PKCS&num;11 module.
 *
 * This structure corresponds to CK_MODULE_INFO in the PKCS\#11 standard. The
 * strings are NULL terminated for easier use.
 *
 * Use gck_module_info_free() to release this structure when done with it.
 */

/*
 * MT safe
 *
 * The only thing that can change after object initialization in
 * a GckModule is the finalized flag, which can be set
 * to 1 in dispose.
 */

enum {
	PROP_0,
	PROP_PATH,
	PROP_FUNCTIONS
};

enum {
	AUTHENTICATE_SLOT,
	AUTHENTICATE_OBJECT,
	LAST_SIGNAL
};

struct _GckModulePrivate {
	gchar *path;
	gboolean initialized;
	CK_FUNCTION_LIST_PTR funcs;
	CK_C_INITIALIZE_ARGS init_args;

	/* Modified atomically */
	gint finalized;
};

G_DEFINE_TYPE_WITH_PRIVATE (GckModule, gck_module, G_TYPE_OBJECT);

static guint signals[LAST_SIGNAL] = { 0 };

/* ----------------------------------------------------------------------------
 * OBJECT
 */

static gboolean
gck_module_real_authenticate_slot (GckModule *module, GckSlot *self, gchar *label, gchar **password)
{
	return FALSE;
}

static gboolean
gck_module_real_authenticate_object (GckModule *module, GckObject *object, gchar *label, gchar **password)
{
	return FALSE;
}

static void
gck_module_init (GckModule *self)
{
	self->pv = gck_module_get_instance_private (self);
}

static void
gck_module_get_property (GObject *obj, guint prop_id, GValue *value,
                          GParamSpec *pspec)
{
	GckModule *self = GCK_MODULE (obj);

	switch (prop_id) {
	case PROP_PATH:
		g_value_set_string (value, gck_module_get_path (self));
		break;
	case PROP_FUNCTIONS:
		g_value_set_pointer (value, gck_module_get_functions (self));
		break;
	}
}

static void
gck_module_set_property (GObject *obj, guint prop_id, const GValue *value,
                          GParamSpec *pspec)
{
	GckModule *self = GCK_MODULE (obj);

	/* Only allowed during initialization */
	switch (prop_id) {
	case PROP_PATH:
		g_return_if_fail (!self->pv->path);
		self->pv->path = g_value_dup_string (value);
		break;
	case PROP_FUNCTIONS:
		g_return_if_fail (!self->pv->funcs);
		self->pv->funcs = g_value_get_pointer (value);
		break;
	}
}

static void
gck_module_dispose (GObject *obj)
{
	GckModule *self = GCK_MODULE (obj);
	gboolean finalize = FALSE;
	CK_RV rv;

	if (self->pv->initialized && self->pv->funcs) {
		if (g_atomic_int_compare_and_exchange (&self->pv->finalized, 0, 1))
			finalize = TRUE;
	}

	/* Must be careful when accessing funcs */
	if (finalize) {
		rv = p11_kit_module_finalize (self->pv->funcs);
		if (rv != CKR_OK) {
			g_warning ("C_Finalize on module '%s' failed: %s",
			           self->pv->path, gck_message_from_rv (rv));
		}
	}

	G_OBJECT_CLASS (gck_module_parent_class)->dispose (obj);
}

static void
gck_module_finalize (GObject *obj)
{
	GckModule *self = GCK_MODULE (obj);

	if (self->pv->initialized && self->pv->funcs)
		p11_kit_module_release (self->pv->funcs);
	self->pv->funcs = NULL;

	g_free (self->pv->path);
	self->pv->path = NULL;

	G_OBJECT_CLASS (gck_module_parent_class)->finalize (obj);
}


static void
gck_module_class_init (GckModuleClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	gck_module_parent_class = g_type_class_peek_parent (klass);

	gobject_class->get_property = gck_module_get_property;
	gobject_class->set_property = gck_module_set_property;
	gobject_class->dispose = gck_module_dispose;
	gobject_class->finalize = gck_module_finalize;

	klass->authenticate_object = gck_module_real_authenticate_object;
	klass->authenticate_slot = gck_module_real_authenticate_slot;

	/**
	 * GckModule:path:
	 *
	 * The PKCS&num;11 module file path.
	 *
	 * This may be set to NULL if this object was created from an already
	 * initialized module via the gck_module_new() function.
	 */
	g_object_class_install_property (gobject_class, PROP_PATH,
		g_param_spec_string ("path", "Module Path", "Path to the PKCS11 Module",
		                     NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GckModule:functions:
	 *
	 * The raw PKCS&num;11 function list for the module.
	 *
	 * This points to a CK_FUNCTION_LIST structure.
	 */
	g_object_class_install_property (gobject_class, PROP_FUNCTIONS,
		g_param_spec_pointer ("functions", "Function List", "PKCS11 Function List",
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GckModule::authenticate-slot:
	 * @module: The module
	 * @slot: The slot to be authenticated.
	 * @string: A displayable label which describes the object.
	 * @password: A gchar** where a password should be returned.
	 *
	 * Use gck_session_set_interaction() instead of connecting to this signal.
	 *
	 * Deprecated: Since 3.4
	 */
	signals[AUTHENTICATE_SLOT] = g_signal_new ("authenticate-slot", GCK_TYPE_MODULE,
			G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GckModuleClass, authenticate_slot),
			g_signal_accumulator_true_handled, NULL, _gck_marshal_BOOLEAN__OBJECT_STRING_POINTER,
			G_TYPE_BOOLEAN, 3, GCK_TYPE_SLOT, G_TYPE_STRING, G_TYPE_POINTER);

	/**
	 * GckModule::authenticate-object:
	 * @module: The module.
	 * @object: The object to be authenticated.
	 * @label: A displayable label which describes the object.
	 * @password: A gchar** where a password should be returned.
	 *
	 * Use gck_session_set_interaction() instead of connecting to this signal.
	 *
	 * Deprecated: Since 3.4
	 */
	signals[AUTHENTICATE_OBJECT] = g_signal_new ("authenticate-object", GCK_TYPE_MODULE,
			G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GckModuleClass, authenticate_object),
			g_signal_accumulator_true_handled, NULL, _gck_marshal_BOOLEAN__OBJECT_STRING_POINTER,
			G_TYPE_BOOLEAN, 3, GCK_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_POINTER);
}

GType
gck_module_info_get_type (void)
{
	static volatile gsize initialized = 0;
	static GType type = 0;
	if (g_once_init_enter (&initialized)) {
		type = g_boxed_type_register_static ("GckModuleInfo",
		                                     (GBoxedCopyFunc)gck_module_info_copy,
		                                     (GBoxedFreeFunc)gck_module_info_free);
		g_once_init_leave (&initialized, 1);
	}
	return type;
}

/**
 * gck_module_info_copy:
 * @module_info: a module info
 *
 * Make a copy of the module info.
 *
 * Returns: (transfer full): a newly allocated copy module info
 */
GckModuleInfo *
gck_module_info_copy (GckModuleInfo *module_info)
{
	if (module_info == NULL)
		return NULL;

	module_info = g_memdup (module_info, sizeof (GckModuleInfo));
	module_info->manufacturer_id = g_strdup (module_info->manufacturer_id);
	module_info->library_description = g_strdup (module_info->library_description);
	return module_info;

}

/**
 * gck_module_info_free:
 * @module_info: The module info to free, or NULL.
 *
 * Free a GckModuleInfo structure.
 **/
void
gck_module_info_free (GckModuleInfo *module_info)
{
	if (!module_info)
		return;
	g_free (module_info->library_description);
	g_free (module_info->manufacturer_id);
	g_free (module_info);
}

typedef struct {
	GckArguments base;
	gchar *path;
	GckModule *result;
	GError *error;
} Initialize;

static CK_RV
perform_initialize (Initialize *args)
{
	CK_FUNCTION_LIST_PTR funcs;
	GckModule *result;
	CK_RV rv;

	funcs = p11_kit_module_load (args->path, P11_KIT_MODULE_CRITICAL);
	if (funcs == NULL) {
		g_set_error (&args->error, GCK_ERROR, (int)CKR_GCK_MODULE_PROBLEM,
		             _("Error loading PKCS#11 module: %s"), p11_kit_message ());
		return CKR_GCK_MODULE_PROBLEM;
	}

	result = g_object_new (GCK_TYPE_MODULE,
	                       "functions", funcs,
	                       "path", args->path,
	                       NULL);

	/* Now initialize the module */
	rv = p11_kit_module_initialize (funcs);
	if (rv != CKR_OK) {
		p11_kit_module_release (funcs);
		g_set_error (&args->error, GCK_ERROR, rv,
		             _("Couldn’t initialize PKCS#11 module: %s"),
		             gck_message_from_rv (rv));
		g_object_unref (result);
		return rv;
	}

	result->pv->initialized = TRUE;
	args->result = result;
	return CKR_OK;
}

static void
free_initialize (Initialize *args)
{
	g_free (args->path);
	g_clear_error (&args->error);
	g_clear_object (&args->result);
	g_free (args);
}

/**
 * gck_module_initialize:
 * @path: The file system path to the PKCS\#11 module to load.
 * @cancellable: (allow-none): optional cancellation object
 * @error: A location to store an error resulting from a failed load.
 *
 * Load and initialize a PKCS\#11 module represented by a GckModule object.
 *
 * Return value: (transfer full): The loaded PKCS\#11 module or %NULL if failed.
 **/
GckModule*
gck_module_initialize (const gchar *path,
                       GCancellable *cancellable,
                       GError **error)
{
	Initialize args = { GCK_ARGUMENTS_INIT, 0,  };

	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	args.path = g_strdup (path);

	if (!_gck_call_sync (NULL, perform_initialize, NULL, &args, cancellable, error)) {

		/* A custom error from perform_initialize */
		if (args.error) {
			g_clear_error (error);
			g_propagate_error (error, args.error);
			args.error = NULL;
		}
	}

	g_free (args.path);
	g_clear_error (&args.error);
	return args.result;
}

/**
 * gck_module_initialize_async:
 * @path: the file system path to the PKCS\#11 module to load
 * @cancellable: (allow-none): optional cancellation object
 * @callback: a callback which will be called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Asynchronously load and initialize a PKCS\#11 module represented by a
 * #GckModule object.
 **/
void
gck_module_initialize_async (const gchar *path,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	Initialize *args;

	g_return_if_fail (path != NULL);

	args =  _gck_call_async_prep (NULL, NULL, perform_initialize, NULL,
	                              sizeof (*args), free_initialize);
	args->path = g_strdup (path);

	_gck_call_async_ready_go (args, cancellable, callback, user_data);
}

/**
 * gck_module_initialize_finish:
 * @result: the asynchronous result
 * @error: location to place an error on failure
 *
 * Finishes the asynchronous initialize operation.
 *
 * Returns: (transfer full) (allow-none): The initialized module, or NULL
 */
GckModule *
gck_module_initialize_finish (GAsyncResult *result,
                              GError **error)
{
	GckModule *module = NULL;
	Initialize *args;

	args = _gck_call_arguments (result, Initialize);
	if (_gck_call_basic_finish (result, error)) {
		module = args->result;
		args->result = NULL;

	} else {
		/* A custom error from perform_initialize */
		if (args->error) {
			g_clear_error (error);
			g_propagate_error (error, args->error);
			args->error = NULL;
		}
	}

	return module;
}

/**
 * gck_module_new: (skip)
 * @funcs: Initialized PKCS\#11 function list pointer
 *
 * Create a GckModule representing a PKCS\#11 module. It is assumed that
 * this the module is already initialized. In addition it will not be
 * finalized when complete.
 *
 * Return value: The new PKCS\#11 module.
 **/
GckModule*
gck_module_new (CK_FUNCTION_LIST_PTR funcs)
{
	g_return_val_if_fail (funcs != NULL, NULL);
	return g_object_new (GCK_TYPE_MODULE, "functions", funcs, NULL);
}

GckModule*
_gck_module_new_initialized (CK_FUNCTION_LIST_PTR funcs)
{
	GckModule *module = gck_module_new (funcs);
	module->pv->initialized = TRUE; /* As if we initialized it */
	return module;
}

/**
 * gck_module_equal:
 * @module1: (type Gck.Module): a pointer to the first #GckModule
 * @module2: (type Gck.Module): a pointer to the second #GckModule
 *
 * Checks equality of two modules. Two GckModule objects can point to the same
 * underlying PKCS\#11 module.
 *
 * Return value: TRUE if module1 and module2 are equal. FALSE if either is not a GckModule.
 **/
gboolean
gck_module_equal (gconstpointer module1, gconstpointer module2)
{
	GckModule *mod1, *mod2;

	if (module1 == module2)
		return TRUE;
	if (!GCK_IS_MODULE (module1) || !GCK_IS_MODULE (module2))
		return FALSE;

	mod1 = GCK_MODULE (module1);
	mod2 = GCK_MODULE (module2);

	return mod1->pv->funcs == mod2->pv->funcs;
}

/**
 * gck_module_hash:
 * @module: (type Gck.Module): a pointer to a #GckModule
 *
 * Create a hash value for the GckModule.
 *
 * This function is intended for easily hashing a GckModule to add to
 * a GHashTable or similar data structure.
 *
 * Return value: An integer that can be used as a hash value, or 0 if invalid.
 **/
guint
gck_module_hash (gconstpointer module)
{
	GckModule *self;

	g_return_val_if_fail (GCK_IS_MODULE (module), 0);
	self = GCK_MODULE (module);
	return g_direct_hash (self->pv->funcs);
}

GckModuleInfo*
_gck_module_info_from_pkcs11 (CK_INFO_PTR info)
{
	GckModuleInfo *modinfo;

	modinfo = g_new0 (GckModuleInfo, 1);
	modinfo->flags = info->flags;
	modinfo->library_description = gck_string_from_chars (info->libraryDescription,
	                                                       sizeof (info->libraryDescription));
	modinfo->manufacturer_id = gck_string_from_chars (info->manufacturerID,
	                                                   sizeof (info->manufacturerID));
	modinfo->library_version_major = info->libraryVersion.major;
	modinfo->library_version_minor = info->libraryVersion.minor;
	modinfo->pkcs11_version_major = info->cryptokiVersion.major;
	modinfo->pkcs11_version_minor = info->cryptokiVersion.minor;

	return modinfo;
}

void
_gck_module_info_to_pkcs11 (GckModuleInfo* module_info, CK_INFO_PTR info)
{
	info->flags = module_info->flags;
	if (!gck_string_to_chars (info->libraryDescription,
	                          sizeof (info->libraryDescription),
	                          module_info->library_description))
		g_return_if_reached ();
	if (!gck_string_to_chars (info->manufacturerID,
	                          sizeof (info->manufacturerID),
	                          module_info->manufacturer_id))
		g_return_if_reached ();

	info->libraryVersion.major = module_info->library_version_major;
	info->libraryVersion.minor = module_info->library_version_minor;
	info->cryptokiVersion.major = module_info->pkcs11_version_major;
	info->cryptokiVersion.minor = module_info->pkcs11_version_minor;
}

/**
 * gck_module_get_info:
 * @self: The module to get info for.
 *
 * Get the info about a PKCS\#11 module.
 *
 * Returns: (transfer full): the module info; release this with gck_module_info_free()
 **/
GckModuleInfo*
gck_module_get_info (GckModule *self)
{
	CK_INFO info;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_MODULE (self), NULL);
	g_return_val_if_fail (self->pv->funcs, NULL);

	memset (&info, 0, sizeof (info));
	rv = (self->pv->funcs->C_GetInfo (&info));
	if (rv != CKR_OK) {
		g_warning ("couldn't get module info: %s", gck_message_from_rv (rv));
		return NULL;
	}

	return _gck_module_info_from_pkcs11 (&info);
}

/**
 * gck_module_get_slots:
 * @self: The module for which to get the slots.
 * @token_present: Whether to limit only to slots with a token present.
 *
 * Get the GckSlot objects for a given module.
 *
 * Return value: (element-type Gck.Slot) (transfer full): The possibly empty
 *               list of slots. Release this with gck_list_unref_free().
 */
GList*
gck_module_get_slots (GckModule *self, gboolean token_present)
{
	CK_SLOT_ID_PTR slot_list;
	CK_ULONG count, i;
	GList *result;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_MODULE (self), NULL);
	g_return_val_if_fail (self->pv->funcs, NULL);

	rv = (self->pv->funcs->C_GetSlotList) (token_present ? CK_TRUE : CK_FALSE, NULL, &count);
	if (rv != CKR_OK) {
		g_warning ("couldn't get slot count: %s", gck_message_from_rv (rv));
		return NULL;
	}

	if (!count)
		return NULL;

	slot_list = g_new (CK_SLOT_ID, count);
	rv = (self->pv->funcs->C_GetSlotList) (token_present ? CK_TRUE : CK_FALSE, slot_list, &count);
	if (rv != CKR_OK) {
		g_warning ("couldn't get slot list: %s", gck_message_from_rv (rv));
		g_free (slot_list);
		return NULL;
	}

	result = NULL;
	for (i = 0; i < count; ++i) {
		result = g_list_prepend (result, g_object_new (GCK_TYPE_SLOT,
		                                               "handle", slot_list[i],
		                                               "module", self, NULL));
	}

	g_free (slot_list);
	return g_list_reverse (result);
}

/**
 * gck_module_get_path:
 * @self: The module for which to get the path.
 *
 * Get the file path of this module. This may not be an absolute path, and
 * usually reflects the path passed to gck_module_initialize().
 *
 * Return value: The path, do not modify or free this value.
 **/
const gchar*
gck_module_get_path (GckModule *self)
{
	g_return_val_if_fail (GCK_IS_MODULE (self), NULL);
	return self->pv->path;
}

/**
 * gck_module_get_functions: (skip)
 * @self: The module for which to get the function list.
 *
 * Get the PKCS\#11 function list for the module.
 *
 * Return value: The function list, do not modify this structure.
 **/
CK_FUNCTION_LIST_PTR
gck_module_get_functions (GckModule *self)
{
	g_return_val_if_fail (GCK_IS_MODULE (self), NULL);
	return self->pv->funcs;
}

/**
 * gck_module_match:
 * @self: the module to match
 * @uri: the uri to match against the module
 *
 * Check whether the PKCS\#11 URI matches the module
 *
 * Returns: whether the URI matches or not
 */
gboolean
gck_module_match (GckModule *self,
                  GckUriData *uri)
{
	gboolean match = TRUE;
	GckModuleInfo *info;

	g_return_val_if_fail (GCK_IS_MODULE (self), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (uri->any_unrecognized)
		match = FALSE;

	if (match && uri->module_info) {
		info = gck_module_get_info (self);
		match = _gck_module_info_match (uri->module_info, info);
		gck_module_info_free (info);
	}

	return match;
}
