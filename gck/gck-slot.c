/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-slot.c - the GObject PKCS#11 wrapper library

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

#include "egg/egg-timegm.h"

#include <string.h>

/**
 * SECTION:gck-slot
 * @title: GckSlot
 * @short_description: Represents a PKCS\#11 slot that can contain a token.
 *
 * A PKCS11 slot can contain a token. As an example, a slot might be a card reader, and the token
 * the card. If the PKCS\#11 module is not a hardware driver, often the slot and token are equivalent.
 */

/**
 * GckSlot:
 *
 * Represents a PKCS11 slot.
 */

enum {
	PROP_0,
	PROP_MODULE,
	PROP_HANDLE
};

struct _GckSlotPrivate {
	GckModule *module;
	CK_SLOT_ID handle;
};

G_DEFINE_TYPE_WITH_PRIVATE (GckSlot, gck_slot, G_TYPE_OBJECT);

/* ----------------------------------------------------------------------------
 * OBJECT
 */

static void
gck_slot_init (GckSlot *self)
{
	self->pv = gck_slot_get_instance_private (self);
}

static void
gck_slot_get_property (GObject *obj, guint prop_id, GValue *value,
                       GParamSpec *pspec)
{
	GckSlot *self = GCK_SLOT (obj);

	switch (prop_id) {
	case PROP_MODULE:
		g_value_take_object (value, gck_slot_get_module (self));
		break;
	case PROP_HANDLE:
		g_value_set_ulong (value, gck_slot_get_handle (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gck_slot_set_property (GObject *obj, guint prop_id, const GValue *value,
                        GParamSpec *pspec)
{
	GckSlot *self = GCK_SLOT (obj);

	/* All writes to data members below, happen only during construct phase */

	switch (prop_id) {
	case PROP_MODULE:
		g_assert (!self->pv->module);
		self->pv->module = g_value_get_object (value);
		g_assert (self->pv->module);
		g_object_ref (self->pv->module);
		break;
	case PROP_HANDLE:
		g_assert (!self->pv->handle);
		self->pv->handle = g_value_get_ulong (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gck_slot_finalize (GObject *obj)
{
	GckSlot *self = GCK_SLOT (obj);

	g_clear_object (&self->pv->module);

	G_OBJECT_CLASS (gck_slot_parent_class)->finalize (obj);
}

static void
gck_slot_class_init (GckSlotClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	gck_slot_parent_class = g_type_class_peek_parent (klass);

	gobject_class->get_property = gck_slot_get_property;
	gobject_class->set_property = gck_slot_set_property;
	gobject_class->finalize = gck_slot_finalize;

	/**
	 * GckSlot:module:
	 *
	 * The PKCS11 object that this slot is a part of.
	 */
	g_object_class_install_property (gobject_class, PROP_MODULE,
		g_param_spec_object ("module", "Module", "PKCS11 Module",
		                     GCK_TYPE_MODULE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GckSlot:handle:
	 *
	 * The raw CK_SLOT_ID handle of this slot.
	 */
	g_object_class_install_property (gobject_class, PROP_HANDLE,
		g_param_spec_ulong ("handle", "Handle", "PKCS11 Slot ID",
		                   0, G_MAXULONG, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/* ----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * GckSlotInfo:
 * @slot_description: Description of the slot.
 * @manufacturer_id: The manufacturer of this slot.
 * @flags: Various PKCS11 flags that apply to this slot.
 * @hardware_version_major: The major version of the hardware.
 * @hardware_version_minor: The minor version of the hardware.
 * @firmware_version_major: The major version of the firmware.
 * @firmware_version_minor: The minor version of the firmware.
 *
 * Represents information about a PKCS11 slot.
 *
 * This is analogous to a CK_SLOT_INFO structure, but the
 * strings are far more usable.
 *
 * When you're done with this structure it should be released with
 * gck_slot_info_free().
 */

GType
gck_slot_info_get_type (void)
{
	static volatile gsize initialized = 0;
	static GType type = 0;
	if (g_once_init_enter (&initialized)) {
		type = g_boxed_type_register_static ("GckSlotInfo",
		                                     (GBoxedCopyFunc)gck_slot_info_copy,
		                                     (GBoxedFreeFunc)gck_slot_info_free);
		g_once_init_leave (&initialized, 1);
	}
	return type;
}

/**
 * gck_slot_info_copy:
 * @slot_info: a slot info
 *
 * Make a copy of the slot info.
 *
 * Returns: (transfer full): a newly allocated copy slot info
 */
GckSlotInfo *
gck_slot_info_copy (GckSlotInfo *slot_info)
{
	if (slot_info == NULL)
		return NULL;

	slot_info = g_memdup (slot_info, sizeof (GckSlotInfo));
	slot_info->manufacturer_id = g_strdup (slot_info->manufacturer_id);
	slot_info->slot_description = g_strdup (slot_info->slot_description);

	return slot_info;
}


/**
 * gck_slot_info_free:
 * @slot_info: The slot info to free, or NULL.
 *
 * Free the GckSlotInfo and associated resources.
 **/
void
gck_slot_info_free (GckSlotInfo *slot_info)
{
	if (!slot_info)
		return;
	g_free (slot_info->slot_description);
	g_free (slot_info->manufacturer_id);
	g_free (slot_info);
}

/**
 * GckTokenInfo:
 * @label: The displayable token label.
 * @manufacturer_id: The manufacturer of this slot.
 * @model: The token model number as a string.
 * @serial_number: The token serial number as a string.
 * @flags: Various PKCS11 flags that apply to this token.
 * @max_session_count: The maximum number of sessions allowed on this token.
 * @session_count: The number of sessions open on this token.
 * @max_rw_session_count: The maximum number of read/write sessions allowed on this token.
 * @rw_session_count: The number of sessions open on this token.
 * @max_pin_len: The maximum length of a PIN for locking this token.
 * @min_pin_len: The minimum length of a PIN for locking this token.
 * @total_public_memory: The total amount of memory on this token for storing public objects.
 * @free_public_memory: The available amount of memory on this token for storing public objects.
 * @total_private_memory: The total amount of memory on this token for storing private objects.
 * @free_private_memory: The available amount of memory on this token for storing private objects.
 * @hardware_version_major: The major version of the hardware.
 * @hardware_version_minor: The minor version of the hardware.
 * @firmware_version_major: The major version of the firmware.
 * @firmware_version_minor: The minor version of the firmware.
 * @utc_time: If the token has a hardware clock, this is set to the number of seconds since the epoch.
 *
 * Represents information about a PKCS11 token.
 *
 * This is analogous to a CK_TOKEN_INFO structure, but the
 * strings are far more usable.
 *
 * When you're done with this structure it should be released with
 * gck_token_info_free().
 */

GType
gck_token_info_get_type (void)
{
	static volatile gsize initialized = 0;
	static GType type = 0;
	if (g_once_init_enter (&initialized)) {
		type = g_boxed_type_register_static ("GckTokenInfo",
		                                     (GBoxedCopyFunc)gck_token_info_copy,
		                                     (GBoxedFreeFunc)gck_token_info_free);
		g_once_init_leave (&initialized, 1);
	}
	return type;
}

/**
 * gck_token_info_copy:
 * @token_info: a token info
 *
 * Make a copy of the token info.
 *
 * Returns: (transfer full): a newly allocated copy token info
 */
GckTokenInfo *
gck_token_info_copy (GckTokenInfo *token_info)
{
	if (token_info == NULL)
		return NULL;

	token_info = g_memdup (token_info, sizeof (GckTokenInfo));
	token_info->label = g_strdup (token_info->label);
	token_info->manufacturer_id = g_strdup (token_info->manufacturer_id);
	token_info->model = g_strdup (token_info->model);
	token_info->serial_number = g_strdup (token_info->serial_number);
	return token_info;
}

/**
 * gck_token_info_free:
 * @token_info: The token info to free, or NULL.
 *
 * Free the GckTokenInfo and associated resources.
 **/
void
gck_token_info_free (GckTokenInfo *token_info)
{
	if (!token_info)
		return;
	g_free (token_info->label);
	g_free (token_info->manufacturer_id);
	g_free (token_info->model);
	g_free (token_info->serial_number);
	g_free (token_info);
}

/**
 * GckMechanismInfo:
 * @min_key_size: The minimum key size that can be used with this mechanism.
 * @max_key_size: The maximum key size that can be used with this mechanism.
 * @flags: Various PKCS11 flags that apply to this mechanism.
 *
 * Represents information about a PKCS11 mechanism.
 *
 * This is analogous to a CK_MECHANISM_INFO structure.
 *
 * When you're done with this structure it should be released with
 * gck_mechanism_info_free().
 */

GType
gck_mechanism_info_get_type (void)
{
	static volatile gsize initialized = 0;
	static GType type = 0;
	if (g_once_init_enter (&initialized)) {
		type = g_boxed_type_register_static ("GckMechanismInfo",
		                                     (GBoxedCopyFunc)gck_mechanism_info_copy,
		                                     (GBoxedFreeFunc)gck_mechanism_info_free);
		g_once_init_leave (&initialized, 1);
	}
	return type;
}

/**
 * gck_mechanism_info_copy:
 * @mech_info: a mechanism info
 *
 * Make a copy of the mechanism info.
 *
 * Returns: (transfer full): a newly allocated copy mechanism info
 */
GckMechanismInfo *
gck_mechanism_info_copy (GckMechanismInfo *mech_info)
{
	return g_memdup (mech_info, sizeof (GckMechanismInfo));
}

/**
 * gck_mechanism_info_free:
 * @mech_info: The mechanism info to free, or NULL.
 *
 * Free the GckMechanismInfo and associated resources.
 **/
void
gck_mechanism_info_free (GckMechanismInfo *mech_info)
{
	if (!mech_info)
		return;
	g_free (mech_info);
}

/**
 * GckMechanisms:
 *
 * A set of GckMechanismInfo structures.
 */

/**
 * gck_mechanisms_length:
 * @a: A GckMechanisms set.
 *
 * Get the number of GckMechanismInfo in the set.
 *
 * Returns: The number in the set.
 */

/**
 * gck_mechanisms_at:
 * @a: A GckMechanisms set.
 * @i: The index of a mechanism
 *
 * Get a specific mechanism in a the set.
 *
 * Returns: the mechanism
 */

/**
 * gck_mechanisms_free:
 * @a: A GckMechanism set.
 *
 * Free a GckMechanisms set.
 */

/**
 * gck_mechanisms_check:
 * @mechanisms: (element-type ulong): A list of mechanisms, perhaps
 *              retrieved from gck_slot_get_mechanisms().
 * @...: A list of mechanism types followed by GCK_INVALID.
 *
 * Check whether all the mechanism types are in the list.
 *
 * The arguments should be a list of CKM_XXX mechanism types. The last argument
 * should be GCK_INVALID.
 *
 * Return value: Whether the mechanism is in the list or not.
 **/
gboolean
gck_mechanisms_check (GArray *mechanisms, ...)
{
	gboolean found = TRUE;
	va_list va;
	gulong mech;
	gsize i;

	g_return_val_if_fail (mechanisms != NULL, FALSE);

	va_start (va, mechanisms);
	for (;;) {
		mech = va_arg (va, gulong);
		if (mech == GCK_INVALID)
			break;

		found = FALSE;
		for (i = 0; i < gck_mechanisms_length (mechanisms); ++i) {
			if (gck_mechanisms_at (mechanisms, i) == mech) {
				found = TRUE;
				break;
			}
		}

		if (found == FALSE)
			break;

	}
	va_end (va);

	return found;
}

/**
 * gck_slot_equal:
 * @slot1: (type Gck.Slot): a pointer to the first #GckSlot
 * @slot2: (type Gck.Slot): a pointer to the second #GckSlot
 *
 * Checks equality of two slots. Two GckSlot objects can point to the same
 * underlying PKCS\#11 slot.
 *
 * Return value: TRUE if slot1 and slot2 are equal. FALSE if either is not a GckSlot.
 **/
gboolean
gck_slot_equal (gconstpointer slot1, gconstpointer slot2)
{
	GckSlot *s1, *s2;

	if (slot1 == slot2)
		return TRUE;
	if (!GCK_IS_SLOT (slot1) || !GCK_IS_SLOT (slot2))
		return FALSE;

	s1 = GCK_SLOT (slot1);
	s2 = GCK_SLOT (slot2);

	return s1->pv->handle == s2->pv->handle &&
	       gck_module_equal (s1->pv->module, s2->pv->module);
}

/**
 * gck_slot_hash:
 * @slot: (type Gck.Slot): a pointer to a #GckSlot
 *
 * Create a hash value for the GckSlot.
 *
 * This function is intended for easily hashing a GckSlot to add to
 * a GHashTable or similar data structure.
 *
 * Return value: An integer that can be used as a hash value, or 0 if invalid.
 **/
guint
gck_slot_hash (gconstpointer slot)
{
	GckSlot *self;

	g_return_val_if_fail (GCK_IS_SLOT (slot), 0);

	self = GCK_SLOT (slot);

	return _gck_ulong_hash (&self->pv->handle) ^
	       gck_module_hash (self->pv->module);
}

/**
 * gck_slot_from_handle:
 * @module: The module that this slot is on.
 * @slot_id: The raw PKCS\#11 handle or slot id of this slot.
 *
 * Create a new GckSlot object for a raw PKCS\#11 handle.
 *
 * Returns: (transfer full): The new GckSlot object.
 **/
GckSlot *
gck_slot_from_handle (GckModule *module,
                      gulong slot_id)
{
	return g_object_new (GCK_TYPE_SLOT,
	                     "module", module,
	                     "handle", slot_id,
	                     NULL);
}

/**
 * gck_slot_get_handle:
 * @self: The slot to get the handle of.
 *
 * Get the raw PKCS\#11 handle of a slot.
 *
 * Return value: the raw CK_SLOT_ID handle
 **/
gulong
gck_slot_get_handle (GckSlot *self)
{
	g_return_val_if_fail (GCK_IS_SLOT (self), (CK_SLOT_ID)-1);
	return self->pv->handle;
}

/**
 * gck_slot_get_module:
 * @self: The slot to get the module for.
 *
 * Get the module that this slot is on.
 *
 * Returns: (transfer full): The module, you must unreference this after
 *          you're done with it.
 */
GckModule *
gck_slot_get_module (GckSlot *self)
{
	g_return_val_if_fail (GCK_IS_SLOT (self), NULL);
	g_return_val_if_fail (GCK_IS_MODULE (self->pv->module), NULL);
	return g_object_ref (self->pv->module);
}

/**
 * gck_slot_get_info:
 * @self: The slot to get info for.
 *
 * Get the information for this slot.
 *
 * Returns: (transfer full): the slot information, when done, use gck_slot_info_free()
 *          to release it.
 **/
GckSlotInfo *
gck_slot_get_info (GckSlot *self)
{
	CK_SLOT_ID handle = (CK_SLOT_ID)-1;
	GckModule *module = NULL;
	CK_FUNCTION_LIST_PTR funcs;
	GckSlotInfo *slotinfo;
	CK_SLOT_INFO info;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_SLOT (self), NULL);

	g_object_get (self, "module", &module, "handle", &handle, NULL);
	g_return_val_if_fail (GCK_IS_MODULE (module), NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, NULL);

	memset (&info, 0, sizeof (info));
	rv = (funcs->C_GetSlotInfo) (handle, &info);

	g_object_unref (module);

	if (rv != CKR_OK) {
		g_warning ("couldn't get slot info: %s", gck_message_from_rv (rv));
		return NULL;
	}

	slotinfo = g_new0 (GckSlotInfo, 1);
	slotinfo->slot_description = gck_string_from_chars (info.slotDescription,
	                                                     sizeof (info.slotDescription));
	slotinfo->manufacturer_id = gck_string_from_chars (info.manufacturerID,
	                                                    sizeof (info.manufacturerID));
	slotinfo->flags = info.flags;
	slotinfo->hardware_version_major = info.hardwareVersion.major;
	slotinfo->hardware_version_minor = info.hardwareVersion.minor;
	slotinfo->firmware_version_major = info.firmwareVersion.major;
	slotinfo->firmware_version_minor = info.firmwareVersion.minor;

	return slotinfo;
}

GckTokenInfo*
_gck_token_info_from_pkcs11 (CK_TOKEN_INFO_PTR info)
{
	GckTokenInfo *token_info;
	gchar *string;
	struct tm tm;

	token_info = g_new0 (GckTokenInfo, 1);
	token_info->label = gck_string_from_chars (info->label, sizeof (info->label));
	token_info->model = gck_string_from_chars (info->model, sizeof (info->model));
	token_info->manufacturer_id = gck_string_from_chars (info->manufacturerID,
	                                                     sizeof (info->manufacturerID));
	token_info->serial_number = gck_string_from_chars (info->serialNumber,
	                                                   sizeof (info->serialNumber));
	token_info->flags = info->flags;
	token_info->max_session_count = info->ulMaxSessionCount;
	token_info->session_count = info->ulSessionCount;
	token_info->max_rw_session_count = info->ulMaxRwSessionCount;
	token_info->rw_session_count = info->ulRwSessionCount;
	token_info->max_pin_len = info->ulMaxPinLen;
	token_info->min_pin_len = info->ulMinPinLen;
	token_info->total_public_memory = info->ulTotalPublicMemory;
	token_info->total_private_memory = info->ulTotalPrivateMemory;
	token_info->free_private_memory = info->ulFreePrivateMemory;
	token_info->free_public_memory = info->ulFreePublicMemory;
	token_info->hardware_version_major = info->hardwareVersion.major;
	token_info->hardware_version_minor = info->hardwareVersion.minor;
	token_info->firmware_version_major = info->firmwareVersion.major;
	token_info->firmware_version_minor = info->firmwareVersion.minor;

	/* Parse the time into seconds since epoch */
	if (info->flags & CKF_CLOCK_ON_TOKEN) {
		string = g_strndup ((gchar*)info->utcTime, MIN (14, sizeof (info->utcTime)));
		if (!strptime (string, "%Y%m%d%H%M%S", &tm))
			token_info->utc_time = -1;
		else
			token_info->utc_time = timegm (&tm);
		g_free (string);
	} else {
		token_info->utc_time = -1;
	}

	return token_info;
}

void
_gck_token_info_to_pkcs11 (GckTokenInfo *token_info, CK_TOKEN_INFO_PTR info)
{
	gchar buffer[64];
	struct tm tm;
	time_t tim;
	gsize len;

	if (!gck_string_to_chars (info->label,
	                          sizeof (info->label),
	                          token_info->label))
		g_return_if_reached ();
	if (!gck_string_to_chars (info->model,
	                          sizeof (info->model),
	                          token_info->model))
		g_return_if_reached ();
	if (!gck_string_to_chars (info->manufacturerID,
	                          sizeof (info->manufacturerID),
	                          token_info->manufacturer_id))
		g_return_if_reached ();
	if (!gck_string_to_chars (info->serialNumber,
	                          sizeof (info->serialNumber),
	                          token_info->serial_number))
		g_return_if_reached ();

	info->flags = token_info->flags;
	info->ulMaxSessionCount = token_info->max_session_count;
	info->ulSessionCount = token_info->session_count;
	info->ulMaxRwSessionCount = token_info->max_rw_session_count;
	info->ulRwSessionCount = token_info->rw_session_count;
	info->ulMaxPinLen = token_info->max_pin_len;
	info->ulMinPinLen = token_info->min_pin_len;
	info->ulTotalPublicMemory = token_info->total_public_memory;
	info->ulTotalPrivateMemory = token_info->total_private_memory;
	info->ulFreePrivateMemory = token_info->free_private_memory;
	info->ulFreePublicMemory = token_info->free_public_memory;
	info->hardwareVersion.major = token_info->hardware_version_major;
	info->hardwareVersion.minor = token_info->hardware_version_minor;
	info->firmwareVersion.major = token_info->firmware_version_major;
	info->firmwareVersion.minor = token_info->firmware_version_minor;

	/* Parse the time into seconds since epoch */
	if (token_info->flags & CKF_CLOCK_ON_TOKEN) {
		tim = token_info->utc_time;
		if (!gmtime_r (&tim, &tm))
			g_return_if_reached ();
		len = strftime (buffer, sizeof (buffer), "%Y%m%d%H%M%S", &tm);
		g_return_if_fail (len == sizeof (info->utcTime));
		memcpy (info->utcTime, buffer, sizeof (info->utcTime));
	} else {
		memset (info->utcTime, 0, sizeof (info->utcTime));
	}
}

/**
 * gck_slot_get_token_info:
 * @self: The slot to get info for.
 *
 * Get the token information for this slot.
 *
 * Returns: (transfer full): the token information; when done, use gck_token_info_free()
 *          to release it
 **/
GckTokenInfo *
gck_slot_get_token_info (GckSlot *self)
{
	CK_SLOT_ID handle = (CK_SLOT_ID)-1;
	CK_FUNCTION_LIST_PTR funcs;
	GckModule *module = NULL;
	CK_TOKEN_INFO info;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_SLOT (self), NULL);

	g_object_get (self, "module", &module, "handle", &handle, NULL);
	g_return_val_if_fail (GCK_IS_MODULE (module), NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, NULL);

	memset (&info, 0, sizeof (info));
	rv = (funcs->C_GetTokenInfo) (handle, &info);

	g_object_unref (module);

	if (rv != CKR_OK) {
		g_warning ("couldn't get slot info: %s", gck_message_from_rv (rv));
		return NULL;
	}

	return _gck_token_info_from_pkcs11 (&info);
}

/**
 * gck_slot_get_mechanisms:
 * @self: The slot to get mechanisms for.
 *
 * Get the available mechanisms for this slot.
 *
 * Returns: (transfer full) (element-type ulong): a list of the mechanisms
 *          for this slot, which should be freed with g_array_free ()
 **/
GArray *
gck_slot_get_mechanisms (GckSlot *self)
{
	CK_SLOT_ID handle = (CK_SLOT_ID)-1;
	CK_FUNCTION_LIST_PTR funcs;
	GckModule *module = NULL;
	CK_MECHANISM_TYPE_PTR mech_list = NULL;
	CK_ULONG count, i;
	GArray *result;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_SLOT (self), NULL);

	g_object_get (self, "module", &module, "handle", &handle, NULL);
	g_return_val_if_fail (GCK_IS_MODULE (module), NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, NULL);

	rv = (funcs->C_GetMechanismList) (handle, NULL, &count);
	if (rv != CKR_OK) {
		g_warning ("couldn't get mechanism count: %s", gck_message_from_rv (rv));
		count = 0;
	} else {
		mech_list = g_new (CK_MECHANISM_TYPE, count);
		rv = (funcs->C_GetMechanismList) (handle, mech_list, &count);
		if (rv != CKR_OK) {
			g_warning ("couldn't get mechanism list: %s", gck_message_from_rv (rv));
			g_free (mech_list);
			count = 0;
		}
	}

	g_object_unref (module);

	if (!count)
		return NULL;

	result = g_array_new (FALSE, TRUE, sizeof (CK_MECHANISM_TYPE));
	for (i = 0; i < count; ++i)
		g_array_append_val (result, mech_list[i]);

	g_free (mech_list);
	return result;

}

/**
 * gck_slot_get_mechanism_info:
 * @self: The slot to get mechanism info from.
 * @mech_type: The mechanisms type to get info for.
 *
 * Get information for the specified mechanism.
 *
 * Returns: (transfer full): the mechanism information, or NULL if failed; use
 *          gck_mechanism_info_free() when done with it
 **/
GckMechanismInfo*
gck_slot_get_mechanism_info (GckSlot *self, gulong mech_type)
{
	CK_SLOT_ID handle = (CK_SLOT_ID)-1;
	CK_FUNCTION_LIST_PTR funcs;
	GckMechanismInfo *mechinfo;
	GckModule *module = NULL;
	CK_MECHANISM_INFO info;
	struct tm;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_SLOT (self), NULL);

	g_object_get (self, "module", &module, "handle", &handle, NULL);
	g_return_val_if_fail (GCK_IS_MODULE (module), NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, NULL);

	memset (&info, 0, sizeof (info));
	rv = (funcs->C_GetMechanismInfo) (handle, mech_type, &info);

	g_object_unref (module);

	if (rv != CKR_OK) {
		g_warning ("couldn't get mechanism info: %s", gck_message_from_rv (rv));
		return NULL;
	}

	mechinfo = g_new0 (GckMechanismInfo, 1);
	mechinfo->flags = info.flags;
	mechinfo->max_key_size = info.ulMaxKeySize;
	mechinfo->min_key_size = info.ulMinKeySize;

	return mechinfo;
}

/**
 * gck_slot_has_flags:
 * @self: The GckSlot object.
 * @flags: The flags to check.
 *
 * Check if the PKCS11 slot has the given flags.
 *
 * Returns: Whether one or more flags exist.
 */
gboolean
gck_slot_has_flags (GckSlot *self, gulong flags)
{
	CK_FUNCTION_LIST_PTR funcs;
	GckModule *module = NULL;
	CK_TOKEN_INFO info;
	CK_SLOT_ID handle;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_SLOT (self), FALSE);

	g_object_get (self, "module", &module, "handle", &handle, NULL);
	g_return_val_if_fail (GCK_IS_MODULE (module), FALSE);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, FALSE);

	memset (&info, 0, sizeof (info));
	rv = (funcs->C_GetTokenInfo) (handle, &info);

	g_object_unref (module);

	if (rv != CKR_OK) {
		g_warning ("couldn't get slot info: %s", gck_message_from_rv (rv));
		return FALSE;
	}

	return (info.flags & flags) != 0;
}

/**
 * gck_slot_enumerate_objects:
 * @self: a #GckSlot to enumerate objects on
 * @match: attributes that the objects must match, or empty for all objects
 * @options: options for opening a session
 *
 * Setup an enumerator for listing matching objects on the slot.
 *
 * If the @match #GckAttributes is floating, it is consumed.
 *
 * This call will not block but will return an enumerator immediately.
 *
 * Returns: (transfer full): a new enumerator
 **/
GckEnumerator *
gck_slot_enumerate_objects (GckSlot *self,
                            GckAttributes *match,
                            GckSessionOptions options)
{
	GList *slots = NULL;
	GckEnumerator *enumerator;

	g_return_val_if_fail (GCK_IS_SLOT (self), NULL);
	g_return_val_if_fail (match != NULL, NULL);

	slots = g_list_append (slots, self);
	enumerator = gck_slots_enumerate_objects (slots, match, options);
	g_list_free (slots);

	return enumerator;
}

/**
 * gck_slots_enumerate_objects:
 * @slots: (element-type Gck.Slot): a list of #GckSlot to enumerate objects on.
 * @match: attributes that the objects must match, or empty for all objects
 * @options: options for opening a session
 *
 * Setup an enumerator for listing matching objects on the slots.
 *
 * If the @match #GckAttributes is floating, it is consumed.
 *
 * This call will not block but will return an enumerator immediately.
 *
 * Returns: (transfer full): a new enumerator
 **/
GckEnumerator*
gck_slots_enumerate_objects (GList *slots,
                             GckAttributes *match,
                             GckSessionOptions options)
{
	GckUriData *uri_data;

	g_return_val_if_fail (match != NULL, NULL);

	uri_data = gck_uri_data_new ();
	uri_data->attributes = gck_attributes_ref_sink (match);

	return _gck_enumerator_new_for_slots (slots, options, uri_data);
}


/**
 * gck_slot_open_session:
 * @self: The slot ot open a session on.
 * @options: The #GckSessionOptions to open a session with.
 * @cancellable: An optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Open a session on the slot. If the 'auto reuse' setting is set,
 * then this may be a recycled session with the same flags.
 *
 * This call may block for an indefinite period.
 *
 * Returns: (transfer full): a new session or %NULL if an error occurs
 **/
GckSession *
gck_slot_open_session (GckSlot *self,
                       GckSessionOptions options,
                       GCancellable *cancellable,
                       GError **error)
{
	return gck_slot_open_session_full (self, options, 0, NULL, NULL, cancellable, error);
}

/**
 * gck_slot_open_session_full: (skip)
 * @self: The slot to open a session on.
 * @options: The options to open the new session with.
 * @pkcs11_flags: Additional raw PKCS\#11 flags.
 * @app_data: Application data for notification callback.
 * @notify: PKCS\#11 notification callback.
 * @cancellable: Optional cancellation object, or NULL.
 * @error: A location to return an error, or NULL.
 *
 * Open a session on the slot. If the 'auto reuse' setting is set,
 * then this may be a recycled session with the same flags.
 *
 * This call may block for an indefinite period.
 *
 * Returns: (transfer full): a new session or %NULL if an error occurs
 **/
GckSession *
gck_slot_open_session_full (GckSlot *self,
                            GckSessionOptions options,
                            gulong pkcs11_flags,
                            gpointer app_data,
                            CK_NOTIFY notify,
                            GCancellable *cancellable,
                            GError **error)
{
	return g_initable_new (GCK_TYPE_SESSION, cancellable, error,
	                       "options", options,
	                       "slot", self,
	                       "opening-flags", pkcs11_flags,
	                       "app-data", app_data,
	                       NULL);
}

/**
 * gck_slot_open_session_async:
 * @self: The slot to open a session on.
 * @options: The options to open the new session with.
 * @cancellable: Optional cancellation object, or NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Open a session on the slot. If the 'auto reuse' setting is set,
 * then this may be a recycled session with the same flags.
 *
 * This call will return immediately and complete asynchronously.
 **/
void
gck_slot_open_session_async (GckSlot *self,
                             GckSessionOptions options,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	gck_slot_open_session_full_async (self, options, 0UL, NULL, NULL, cancellable, callback, user_data);
}

static void
on_open_session_complete (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;
	GObject *session;

	session = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, &error);
	if (session != NULL)
		g_simple_async_result_set_op_res_gpointer (res, session, g_object_unref);
	else
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

/**
 * gck_slot_open_session_full_async: (skip)
 * @self: The slot to open a session on.
 * @options: Options to open the new session with.
 * @pkcs11_flags: Additional raw PKCS\#11 flags.
 * @app_data: Application data for notification callback.
 * @notify: PKCS\#11 notification callback.
 * @cancellable: Optional cancellation object, or NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Open a session on the slot. If the 'auto reuse' setting is set,
 * then this may be a recycled session with the same flags.
 *
 * This call will return immediately and complete asynchronously.
 **/
void
gck_slot_open_session_full_async (GckSlot *self,
                                  GckSessionOptions options,
                                  gulong pkcs11_flags,
                                  gpointer app_data,
                                  CK_NOTIFY notify,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (GCK_IS_SLOT (self));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 gck_slot_open_session_full_async);

	g_async_initable_new_async (GCK_TYPE_SESSION, G_PRIORITY_DEFAULT,
	                            cancellable, on_open_session_complete,
	                            g_object_ref (res),
	                            "options", options,
	                            "slot", self,
	                            "opening-flags", pkcs11_flags,
	                            "app-data", app_data,
	                            NULL);

	g_object_unref (res);
}

/**
 * gck_slot_open_session_finish:
 * @self: The slot to open a session on.
 * @result: The result passed to the callback.
 * @error: A location to return an error or NULL.
 *
 * Get the result of an open session operation. If the 'auto reuse' setting is set,
 * then this may be a recycled session with the same flags.
 *
 * Returns: (transfer full): the new session or %NULL if an error occurs
 */
GckSession *
gck_slot_open_session_finish (GckSlot *self, GAsyncResult *result, GError **error)
{
	GSimpleAsyncResult *res;

	g_return_val_if_fail (GCK_IS_SLOT (self), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      gck_slot_open_session_full_async), NULL);

	res = G_SIMPLE_ASYNC_RESULT (result);
	if (g_simple_async_result_propagate_error (res, error))
		return NULL;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (res));
}

/**
 * gck_slot_match:
 * @self: the slot to match
 * @uri: the uri to match against the slot
 *
 * Check whether the PKCS\#11 URI matches the slot
 *
 * Returns: whether the URI matches or not
 */
gboolean
gck_slot_match (GckSlot *self,
                GckUriData *uri)
{
	GckModule *module;
	GckTokenInfo *info;
	gboolean match = TRUE;

	g_return_val_if_fail (GCK_IS_SLOT (self), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (uri->any_unrecognized)
		match = FALSE;

	if (match && uri->module_info) {
		module = gck_slot_get_module (self);
		match = gck_module_match (module, uri);
		g_object_unref (module);
	}

	if (match && uri->token_info) {
		info = gck_slot_get_token_info (self);
		match = _gck_token_info_match (uri->token_info, info);
		gck_token_info_free (info);
	}

	return match;
}
