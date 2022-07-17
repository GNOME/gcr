/* gck-misc.c - the GObject PKCS#11 wrapper library

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

#include "egg/egg-secure-memory.h"

#include <p11-kit/p11-kit.h>

#include <glib/gi18n-lib.h>

#include <string.h>

EGG_SECURE_DEFINE_GLIB_GLOBALS ();

/**
 * GCK_CHECK_VERSION:
 * @major: the major version to check for
 * @minor: the minor version to check for
 * @micro: the micro version to check for
 *
 * Checks the version of the Gck library that is being compiled
 * against.
 *
 * ```
 * #if !GCK_CHECK_VERSION (3, 0, 0)
 * #warning Old Gck version, disabling functionality
 * #endif
 * ```
 *
 * Returns: %TRUE if the version of the GLib header files
 * is the same as or newer than the passed-in version.
 */

/**
 * GCK_MAJOR_VERSION:
 *
 * The major version number of the Gck library.
 */

/**
 * GCK_MINOR_VERSION:
 *
 * The minor version number of the Gck library.
 */

/**
 * GCK_MICRO_VERSION:
 *
 * The micro version number of the Gck library.
 */

/**
 * GCK_INVALID:
 *
 * Used as a terminator at the end of variable argument lists.
 */

/**
 * GCK_VENDOR_CODE:
 *
 * Custom PKCS#11 errors that originate from the gck library, are
 * based at this error code.
 */

/**
 * GckError:
 * @GCK_ERROR_MODULE_PROBLEM: a result code that signifies there was a problem
 *                            loading a PKCS#11 module, usually a shared library
 *
 * Various error codes. All the `CKR_XXX` error codes from PKCS#11 are also
 * relevant error codes.
 *
 * Note that errors are returned as [struct@GLib.Error] structures. The `code`
 * member of the error then contains the raw PKCS#11 `CK_RV` result value.
 */

/**
 * GCK_ERROR:
 *
 * The error domain for gck library errors.
 */
G_DEFINE_QUARK(GckError, gck_error)

/**
 * gck_message_from_rv:
 * @rv: The PKCS#11 return value to get a message for.
 *
 * Get a message for a PKCS#11 return value or error code. Do not
 * pass `CKR_OK` or other non-errors to this function.
 *
 * Return value: The user readable message.
 */
const gchar*
gck_message_from_rv (gulong rv)
{
	switch (rv) {

	/* These are not really errors, or not current */
	case CKR_OK:
	case CKR_NO_EVENT:
	case CKR_FUNCTION_NOT_PARALLEL:
	case CKR_SESSION_PARALLEL_NOT_SUPPORTED:
		g_return_val_if_reached ("");

	default:
		return p11_kit_strerror (rv);
	}
}

const gchar *
_gck_stringize_rv (CK_RV rv)
{
	switch(rv) {
	#define X(x) case x: return #x;
	X (CKR_OK)
	X (CKR_CANCEL)
	X (CKR_HOST_MEMORY)
	X (CKR_SLOT_ID_INVALID)
	X (CKR_GENERAL_ERROR)
	X (CKR_FUNCTION_FAILED)
	X (CKR_ARGUMENTS_BAD)
	X (CKR_NO_EVENT)
	X (CKR_NEED_TO_CREATE_THREADS)
	X (CKR_CANT_LOCK)
	X (CKR_ATTRIBUTE_READ_ONLY)
	X (CKR_ATTRIBUTE_SENSITIVE)
	X (CKR_ATTRIBUTE_TYPE_INVALID)
	X (CKR_ATTRIBUTE_VALUE_INVALID)
	X (CKR_DATA_INVALID)
	X (CKR_DATA_LEN_RANGE)
	X (CKR_DEVICE_ERROR)
	X (CKR_DEVICE_MEMORY)
	X (CKR_DEVICE_REMOVED)
	X (CKR_ENCRYPTED_DATA_INVALID)
	X (CKR_ENCRYPTED_DATA_LEN_RANGE)
	X (CKR_FUNCTION_CANCELED)
	X (CKR_FUNCTION_NOT_PARALLEL)
	X (CKR_FUNCTION_NOT_SUPPORTED)
	X (CKR_KEY_HANDLE_INVALID)
	X (CKR_KEY_SIZE_RANGE)
	X (CKR_KEY_TYPE_INCONSISTENT)
	X (CKR_KEY_NOT_NEEDED)
	X (CKR_KEY_CHANGED)
	X (CKR_KEY_NEEDED)
	X (CKR_KEY_INDIGESTIBLE)
	X (CKR_KEY_FUNCTION_NOT_PERMITTED)
	X (CKR_KEY_NOT_WRAPPABLE)
	X (CKR_KEY_UNEXTRACTABLE)
	X (CKR_MECHANISM_INVALID)
	X (CKR_MECHANISM_PARAM_INVALID)
	X (CKR_OBJECT_HANDLE_INVALID)
	X (CKR_OPERATION_ACTIVE)
	X (CKR_OPERATION_NOT_INITIALIZED)
	X (CKR_PIN_INCORRECT)
	X (CKR_PIN_INVALID)
	X (CKR_PIN_LEN_RANGE)
	X (CKR_PIN_EXPIRED)
	X (CKR_PIN_LOCKED)
	X (CKR_SESSION_CLOSED)
	X (CKR_SESSION_COUNT)
	X (CKR_SESSION_HANDLE_INVALID)
	X (CKR_SESSION_PARALLEL_NOT_SUPPORTED)
	X (CKR_SESSION_READ_ONLY)
	X (CKR_SESSION_EXISTS)
	X (CKR_SESSION_READ_ONLY_EXISTS)
	X (CKR_SESSION_READ_WRITE_SO_EXISTS)
	X (CKR_SIGNATURE_INVALID)
	X (CKR_SIGNATURE_LEN_RANGE)
	X (CKR_TEMPLATE_INCOMPLETE)
	X (CKR_TEMPLATE_INCONSISTENT)
	X (CKR_TOKEN_NOT_PRESENT)
	X (CKR_TOKEN_NOT_RECOGNIZED)
	X (CKR_TOKEN_WRITE_PROTECTED)
	X (CKR_UNWRAPPING_KEY_HANDLE_INVALID)
	X (CKR_UNWRAPPING_KEY_SIZE_RANGE)
	X (CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT)
	X (CKR_USER_ALREADY_LOGGED_IN)
	X (CKR_USER_NOT_LOGGED_IN)
	X (CKR_USER_PIN_NOT_INITIALIZED)
	X (CKR_USER_TYPE_INVALID)
	X (CKR_USER_ANOTHER_ALREADY_LOGGED_IN)
	X (CKR_USER_TOO_MANY_TYPES)
	X (CKR_WRAPPED_KEY_INVALID)
	X (CKR_WRAPPED_KEY_LEN_RANGE)
	X (CKR_WRAPPING_KEY_HANDLE_INVALID)
	X (CKR_WRAPPING_KEY_SIZE_RANGE)
	X (CKR_WRAPPING_KEY_TYPE_INCONSISTENT)
	X (CKR_RANDOM_SEED_NOT_SUPPORTED)
	X (CKR_RANDOM_NO_RNG)
	X (CKR_DOMAIN_PARAMS_INVALID)
	X (CKR_BUFFER_TOO_SMALL)
	X (CKR_SAVED_STATE_INVALID)
	X (CKR_INFORMATION_SENSITIVE)
	X (CKR_STATE_UNSAVEABLE)
	X (CKR_CRYPTOKI_NOT_INITIALIZED)
	X (CKR_CRYPTOKI_ALREADY_INITIALIZED)
	X (CKR_MUTEX_BAD)
	X (CKR_MUTEX_NOT_LOCKED)
	X (CKR_FUNCTION_REJECTED)
	X (CKR_VENDOR_DEFINED)
	#undef X
	default:
		return "CKR_??????";
	}
}

CK_RV
_gck_rv_from_error (GError *error,
                    CK_RV catch_all_code)
{
	g_return_val_if_fail (error != NULL, CKR_GENERAL_ERROR);

	if (error->domain == GCK_ERROR)
		return error->code;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return CKR_FUNCTION_CANCELED;

	return catch_all_code;
}

/**
 * gck_string_from_chars: (skip)
 * @data: The character data to turn into a null terminated string.
 * @max: The maximum length of the charater data.
 *
 * Create a string from a set of PKCS#11 characters. This is
 * similar to [func@GLib.strndup], except for that it also strips trailing
 * spaces. These space padded strings are often used in PKCS#11
 * structures.
 *
 * If the space padded string is filled with null characters then
 * this function will return %NULL.
 *
 * Return value: The null terminated string.
 */
gchar*
gck_string_from_chars (const guchar *data, gsize max)
{
	gchar *string;

	g_return_val_if_fail (data, NULL);
	g_return_val_if_fail (max, NULL);

	/* Means no value */
	if (!data[0])
		return NULL;

	string = g_strndup ((gchar*)data, max);
	g_strchomp (string);
	return string;
}

/**
 * gck_string_to_chars: (skip)
 * @data: The character buffer to place string into.
 * @max: The maximum length of the charater buffer.
 * @string: The string to place in the buffer.
 *
 * Create a space padded PKCS#11 string from a null terminated string.
 * The string must be shorter than the buffer or %FALSE will be
 * returned.
 *
 * If a %NULL string is passed, then the space padded string will be
 * set to zero characters.
 *
 * Return value: The null terminated string.
 */
gboolean
gck_string_to_chars (guchar *data, gsize max, const gchar *string)
{
	gsize len;

	g_return_val_if_fail (data, FALSE);
	g_return_val_if_fail (max, FALSE);

	if (!string) {
		memset (data, 0, max);
		return TRUE;
	}

	len = strlen (string);
	if (len > max)
		return FALSE;

	memset (data, ' ', max);
	memcpy (data, string, len);
	return TRUE;
}

guint
_gck_ulong_hash (gconstpointer v)
{
	const signed char *p = v;
	guint32 i, h = *p;

	for(i = 0; i < sizeof (gulong); ++i)
		h = (h << 5) - h + *(p++);

	return h;
}

gboolean
_gck_ulong_equal (gconstpointer v1, gconstpointer v2)
{
	return memcmp(v1, v2, sizeof (gulong)) == 0;
}

/**
 * gck_value_to_ulong:
 * @value: (array length=length): memory to convert
 * @length: length of memory
 * @result: (out): A location to store the result
 *
 * Convert `CK_ULONG` type memory to a boolean.
 *
 * Returns: Whether the conversion was successful.
 */
gboolean
gck_value_to_ulong (const guchar *value,
                    gsize length,
                    gulong *result)
{
	if (!value || length != sizeof (CK_ULONG))
		return FALSE;
	if (result)
		memcpy(result, value, sizeof(CK_ULONG));
	return TRUE;
}

/**
 * gck_value_to_boolean:
 * @value: (array length=length): memory to convert
 * @length: length of memory
 * @result: (out): A location to store the result
 *
 * Convert `CK_BBOOL` type memory to a boolean.
 *
 * Returns: Whether the conversion was successful.
 */
gboolean
gck_value_to_boolean (const guchar *value,
                      gsize length,
                      gboolean *result)
{
	CK_BBOOL tempval = CK_FALSE;
	if (!value || length != sizeof (CK_BBOOL))
		return FALSE;
	if (result) {
		memcpy(&tempval, value, sizeof(CK_BBOOL));
		*result = tempval ? TRUE : FALSE;
	}
	return TRUE;
}

static gboolean
match_info_string (const gchar *match, const gchar *string)
{
	/* NULL matches anything */
	if (match == NULL)
		return TRUE;

	if (string == NULL)
		return FALSE;

	return g_str_equal (match, string);
}

gboolean
_gck_module_info_match (GckModuleInfo *match, GckModuleInfo *info)
{
	/* Matches two GckModuleInfo for use in PKCS#11 URI's */

	g_return_val_if_fail (match, FALSE);
	g_return_val_if_fail (info, FALSE);

	return (match_info_string (match->library_description, info->library_description) &&
	        match_info_string (match->manufacturer_id, info->manufacturer_id));
}

gboolean
_gck_token_info_match (GckTokenInfo *match, GckTokenInfo *info)
{
	/* Matches two GckTokenInfo for use in PKCS#11 URI's */

	g_return_val_if_fail (match, FALSE);
	g_return_val_if_fail (info, FALSE);

	return (match_info_string (match->label, info->label) &&
	        match_info_string (match->manufacturer_id, info->manufacturer_id) &&
	        match_info_string (match->model, info->model) &&
	        match_info_string (match->serial_number, info->serial_number));
}
