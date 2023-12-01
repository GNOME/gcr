/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "gcr-internal.h"
#include "gcr-library.h"
#include "gcr-types.h"

#include "egg/egg-error.h"

#ifdef WITH_GCRYPT
#include "egg/egg-libgcrypt.h"
#endif

#include "egg/egg-secure-memory.h"

#include <p11-kit/p11-kit.h>

#include <gck/gck.h>

#include <glib/gi18n-lib.h>

/**
 * GCR_CHECK_VERSION:
 * @major: the major version to check for
 * @minor: the minor version to check for
 * @micro: the micro version to check for
 *
 * Checks the version of the Gcr library that is being compiled
 * against.
 *
 * ```
 * #if !GCR_CHECK_VERSION (3, 0, 0)
 * #warning Old Gcr version, disabling functionality
 * #endif
 * ```
 *
 * Returns: %TRUE if the version of the Gcr header files
 * is the same as or newer than the passed-in version.
 */

/**
 * GCR_MAJOR_VERSION:
 *
 * The major version number of the Gcr library.
 */

/**
 * GCR_MINOR_VERSION:
 *
 * The minor version number of the Gcr library.
 */

/**
 * GCR_MICRO_VERSION:
 *
 * The micro version number of the Gcr library.
 */

G_LOCK_DEFINE_STATIC (modules);
static GList *all_modules = NULL;
static gboolean initialized_modules = FALSE;

G_LOCK_DEFINE_STATIC (uris);
static gboolean initialized_uris = FALSE;
static gchar *trust_store_uri = NULL;
static gchar **trust_lookup_uris = NULL;

/* -----------------------------------------------------------------------------
 * ERRORS
 */

GQuark
gcr_data_error_get_domain (void)
{
	static GQuark domain = 0;
	if (domain == 0)
		domain = g_quark_from_static_string ("gcr-parser-error");
	return domain;
}

/* -----------------------------------------------------------------------------
 * INITIALIZATION
 */

void
_gcr_uninitialize_library (void)
{
	G_LOCK (modules);

	g_clear_list (&all_modules, g_object_unref);
	initialized_modules = FALSE;

	G_UNLOCK (modules);

	G_LOCK (uris);

	initialized_uris = FALSE;
	g_free (trust_store_uri);
	trust_store_uri = NULL;
	g_strfreev (trust_lookup_uris);
	trust_lookup_uris = NULL;

	G_UNLOCK (uris);
}
void
_gcr_initialize_library (void)
{
	static gint gcr_initialize = 0;

	if (g_atomic_int_add (&gcr_initialize, 1) == 0)
		return;

#ifdef WITH_GCRYPT
	/* Initialize the libgcrypt library if needed */
	egg_libgcrypt_initialize ();
#endif

	g_debug ("initialized library");
}

static void
initialize_uris (void)
{
	GPtrArray *uris;
	GList *l;
	gchar *uri;
	gchar *debug;

	if (initialized_uris)
		return;

	if (!initialized_modules) {
		g_debug ("modules not initialized");
		return;
	}

	G_LOCK (uris);

	if (!initialized_uris) {
		/* Ask for the global x-trust-store option */
		trust_store_uri = p11_kit_config_option (NULL, "x-trust-store");
		for (l = all_modules; !trust_store_uri && l != NULL; l = g_list_next (l)) {
			trust_store_uri = p11_kit_config_option (gck_module_get_functions (l->data),
			                                         "x-trust-store");
		}

		uris = g_ptr_array_new ();
		uri = p11_kit_config_option (NULL, "x-trust-lookup");
		if (uri != NULL)
			g_ptr_array_add (uris, uri);
		for (l = all_modules; l != NULL; l = g_list_next (l)) {
			uri = p11_kit_config_option (gck_module_get_functions (l->data),
			                             "x-trust-lookup");
			if (uri != NULL)
				g_ptr_array_add (uris, uri);
		}
		g_ptr_array_add (uris, NULL);

		trust_lookup_uris = (gchar**)g_ptr_array_free (uris, FALSE);

		g_debug ("trust store uri is: %s", trust_store_uri);
		debug = g_strjoinv (" ", trust_lookup_uris);
		g_debug ("trust lookup uris are: %s", debug);
		g_free (debug);

		initialized_uris = TRUE;
	}

	G_UNLOCK (uris);
}

static void
on_initialize_registered (GObject *object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GError *error = NULL;
	GList *results;

	results = gck_modules_initialize_registered_finish (result, &error);
	if (error != NULL) {
		g_debug ("failed %s", error->message);
		g_task_return_error (task, g_steal_pointer (&error));
		g_clear_object (&task);
		return;
	}

	G_LOCK (modules);
	if (!initialized_modules) {
		all_modules = g_list_concat (all_modules, results);
		results = NULL;
		initialized_modules = TRUE;
	}
	G_UNLOCK (modules);

	g_clear_list (&results, g_object_unref);

	g_debug ("completed initialize of registered modules");
	g_task_return_boolean (task, TRUE);
	g_clear_object (&task);
}

/**
 * gcr_pkcs11_initialize_async:
 * @cancellable: (nullable): optional cancellable used to cancel the operation
 * @callback: callback which will be called when the operation completes
 * @user_data: data passed to the callback
 *
 * Asynchronously initialize the registered PKCS#11 modules.
 */
void
gcr_pkcs11_initialize_async (GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	GTask *task;

	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_pkcs11_initialize_async);

	if (initialized_modules) {
		g_debug ("already initialized, no need to async");
		g_task_return_boolean (task, TRUE);
	} else {
		gck_modules_initialize_registered_async (cancellable,
		                                         on_initialize_registered,
		                                         g_steal_pointer (&task));
		g_debug ("starting initialize of registered modules");
	}

	g_clear_object (&task);
}

/**
 * gcr_pkcs11_initialize_finish:
 * @result: the asynchronous result
 * @error: location to place an error on failure
 *
 * Complete the asynchronous operation to initialize the registered PKCS#11
 * modules.
 *
 * Returns: whether the operation was successful or not.
 */
gboolean
gcr_pkcs11_initialize_finish (GAsyncResult *result,
                              GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * gcr_pkcs11_initialize:
 * @cancellable: optional cancellable used to cancel the operation
 * @error: location to place an error on failure
 *
 * Asynchronously initialize the registered PKCS#11 modules.
 *
 * Returns: whether the operation was successful or not.
 */
gboolean
gcr_pkcs11_initialize (GCancellable *cancellable,
                       GError **error)
{
	GList *results;
	GError *err = NULL;

	if (initialized_modules)
		return TRUE;

	results = gck_modules_initialize_registered (cancellable, &err);
	if (err == NULL) {

		g_debug ("registered module initialize succeeded: %d modules",
		         g_list_length (results));

		G_LOCK (modules);

		if (!initialized_modules) {
			all_modules = g_list_concat (all_modules, results);
			results = NULL;
			initialized_modules = TRUE;
		}

		G_UNLOCK (modules);

	} else {
		g_debug ("registered module initialize failed: %s", err->message);
		g_propagate_error (error, err);
	}

	g_clear_list (&results, g_object_unref);
	return (err == NULL);
}

/**
 * gcr_pkcs11_get_modules:
 *
 * List all the PKCS#11 modules that are used by the GCR library.
 * Each module is a [class@Gck.Module] object.
 *
 * An empty list of modules will be returned if [func@pkcs11_set_modules],
 * or [func@pkcs11_initialize] has not yet run.
 *
 * Returns: (transfer full) (element-type Gck.Module): a newly allocated list
 *          of #GckModule objects
 */
GList*
gcr_pkcs11_get_modules (void)
{
	if (!initialized_modules)
		g_debug ("pkcs11 not yet initialized");
	else if (!all_modules)
		g_debug ("no modules loaded");
	return g_list_copy_deep (all_modules, (GCopyFunc) g_object_ref, NULL);
}

/**
 * gcr_pkcs11_set_modules:
 * @modules: (element-type Gck.Module): a list of PKCS#11 modules
 *
 * Set the list of PKCS#11 modules that are used by the GCR library.
 * Each module in the list is a [class@Gck.Module] object.
 *
 * It is not normally necessary to call this function. The available
 * PKCS#11 modules installed on the system are automatically loaded
 * by the GCR library.
 */
void
gcr_pkcs11_set_modules (GList *modules)
{
	for (GList *l = modules; l; l = g_list_next (l))
		g_return_if_fail (GCK_IS_MODULE (l->data));

	modules = g_list_copy_deep (modules, (GCopyFunc) g_object_ref, NULL);
	g_clear_list (&all_modules, g_object_unref);
	all_modules = modules;
	initialized_modules = TRUE;
}

/**
 * gcr_pkcs11_add_module:
 * @module: a #GckModule
 *
 * Add a #GckModule to the list of PKCS#11 modules that are used by the
 * GCR library.
 *
 * It is not normally necessary to call this function. The available
 * PKCS#11 modules installed on the system are automatically loaded
 * by the GCR library.
 */
void
gcr_pkcs11_add_module (GckModule *module)
{
	g_return_if_fail (GCK_IS_MODULE (module));
	all_modules = g_list_append (all_modules, g_object_ref (module));
}

/**
 * gcr_pkcs11_add_module_from_file:
 * @module_path: the full file path of the PKCS#11 module
 * @unused: unused
 * @error: a #GError or %NULL
 *
 * Initialize a PKCS#11 module and add it to the modules that are
 * used by the GCR library. Note that is an error to initialize the same
 * PKCS#11 module twice.
 *
 * It is not normally necessary to call this function. The available
 * PKCS#11 modules installed on the system are automatically loaded
 * by the GCR library.
 *
 * Returns: whether the module was sucessfully added.
 */
gboolean
gcr_pkcs11_add_module_from_file (const gchar *module_path, gpointer unused,
                                 GError **error)
{
	GckModule *module;
	GError *err = NULL;

	g_return_val_if_fail (module_path, FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	module = gck_module_initialize (module_path, NULL, &err);
	if (module == NULL) {
		g_debug ("initializing module failed: %s: %s",
		            module_path, err->message);
		g_propagate_error (error, err);
		return FALSE;
	}

	gcr_pkcs11_add_module (module);

	g_debug ("initialized and added module: %s", module_path);
	g_object_unref (module);
	return TRUE;
}

/**
 * gcr_pkcs11_get_trust_store_slot:
 *
 * Selects an appropriate PKCS#11 slot to store trust assertions. The slot
 * to use is normally configured automatically by the system.
 *
 * This will only return a valid result after the [func@pkcs11_initialize]
 * method has been called.
 *
 * When done with the #GckSlot, use g_object_unref() to release it.
 *
 * Returns: (transfer full) (nullable): the #GckSlot to use for trust
 *          assertions, or null if not initialized or no appropriate
 *          trust store could be found.
 */
GckSlot *
gcr_pkcs11_get_trust_store_slot (void)
{
	GckSlot *slot;
	GError *error = NULL;

	if (!initialized_modules)
		return NULL;

	initialize_uris ();
	if (!trust_store_uri) {
		g_warning ("no slot available for storing assertions");
		return NULL;
	}

	slot = gck_modules_token_for_uri (all_modules, trust_store_uri, &error);
	if (!slot) {
		if (error) {
			g_warning ("error finding slot to store trust assertions: %s: %s",
			           trust_store_uri, egg_error_message (error));
			g_clear_error (&error);
		} else {
			g_debug ("no trust store slot found");
		}
	}

	return slot;
}

/**
 * gcr_pkcs11_get_trust_lookup_slots:
 *
 * List all the PKCS#11 slots that are used by the GCR library for lookup
 * of trust assertions. Each slot is a [class@Gck.Slot] object.
 *
 * This will return an empty list if the [func@pkcs11_initialize] function has
 * not yet been called.
 *
 * Returns: (transfer full) (element-type Gck.Slot): a list of #GckSlot
 *          objects to use for lookup of trust, or the empty list if not
 *          initialized or no appropriate trust stores could be found.
 */
GList*
gcr_pkcs11_get_trust_lookup_slots (void)
{
	GList *results = NULL;
	GError *error = NULL;
	gchar **uri;

	if (!initialized_modules)
		return NULL;

	initialize_uris ();
	if (!trust_lookup_uris) {
		g_warning ("no slots available for assertion lookup");
		return NULL;
	}

	for (uri = trust_lookup_uris; uri && *uri; ++uri) {
		results = g_list_concat (results, gck_modules_tokens_for_uri (all_modules, *uri, &error));
		if (error != NULL) {
			g_warning ("error finding slot for trust assertions: %s: %s",
			           *uri, egg_error_message (error));
			g_clear_error (&error);
		}
	}

	if (results == NULL)
		g_debug ("no trust lookup slots found");

	return results;
}

/**
 * gcr_pkcs11_get_trust_store_uri:
 *
 * Get the PKCS#11 URI that is used to identify which slot to use for
 * storing trust storage.
 *
 * Returns: (nullable): the uri which identifies trust storage slot
 */
const gchar*
gcr_pkcs11_get_trust_store_uri (void)
{
	initialize_uris ();
	return trust_store_uri;
}

/**
 * gcr_pkcs11_set_trust_store_uri:
 * @pkcs11_uri: (nullable): the uri which identifies trust storage slot
 *
 * Set the PKCS#11 URI that is used to identify which slot to use for
 * storing trust assertions.
 *
 * It is not normally necessary to call this function. The relevant
 * PKCS#11 slot is automatically configured by the GCR library.
 */
void
gcr_pkcs11_set_trust_store_uri (const gchar *pkcs11_uri)
{
	G_LOCK (uris);

	g_free (trust_store_uri);
	trust_store_uri = g_strdup (pkcs11_uri);
	initialized_uris = TRUE;

	G_UNLOCK (uris);
}


/**
 * gcr_pkcs11_get_trust_lookup_uris:
 *
 * Get the PKCS#11 URIs that are used to identify which slots to use for
 * lookup trust assertions.
 *
 * Returns: (nullable) (transfer none): the uri which identifies trust storage slot
 */
const gchar **
gcr_pkcs11_get_trust_lookup_uris (void)
{
	initialize_uris ();
	return (const gchar **)trust_lookup_uris;
}

/**
 * gcr_pkcs11_set_trust_lookup_uris:
 * @pkcs11_uris: (nullable): the uris which identifies trust lookup slots
 *
 * Set the PKCS#11 URIs that are used to identify which slots to use for
 * lookup of trust assertions.
 *
 * It is not normally necessary to call this function. The relevant
 * PKCS#11 slots are automatically configured by the GCR library.
 */
void
gcr_pkcs11_set_trust_lookup_uris (const gchar **pkcs11_uris)
{
	G_LOCK (uris);

	g_strfreev (trust_lookup_uris);
	trust_lookup_uris = g_strdupv ((gchar**)pkcs11_uris);
	initialized_uris = TRUE;

	G_UNLOCK (uris);
}
