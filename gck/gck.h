/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck.h - the GObject PKCS#11 wrapper library

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

#ifndef GCK_H
#define GCK_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <p11-kit/pkcs11.h>

#define __GCK_INSIDE_HEADER__

#include <gck/gck-enum-types.h>
#include <gck/gck-version.h>

G_BEGIN_DECLS

/*
 * To use this API, you need to be prepared for changes to the API,
 * and add the C flag: -DGCK_API_SUBJECT_TO_CHANGE
 */

#ifndef __GI_SCANNER__
#ifndef GCK_API_SUBJECT_TO_CHANGE
#error "This API has not yet reached stability."
#endif
#endif

#define             GCK_VENDOR_CODE                         0x47434B00 /* GCK */

/* An error code which results from a failure to load the PKCS11 module */
typedef enum {
	GCK_ERROR_MODULE_PROBLEM = (CKR_VENDOR_DEFINED | (GCK_VENDOR_CODE + 1)),
} GckError;

#define             GCK_ERROR                               (gck_error_quark ())

GQuark              gck_error_quark                         (void) G_GNUC_CONST;

const gchar*        gck_message_from_rv                     (gulong rv);

gboolean            gck_string_to_chars                     (guchar *data,
                                                             gsize max,
                                                             const gchar *string);

gchar*              gck_string_from_chars                   (const guchar *data,
                                                             gsize max);

typedef gpointer    (*GckAllocator)                         (gpointer data, gsize length);

typedef enum {
	GCK_SESSION_READ_ONLY = 0,
	GCK_SESSION_READ_WRITE = 1 << 1,
	GCK_SESSION_LOGIN_USER =  1 << 2,
	GCK_SESSION_AUTHENTICATE = 1 << 3,
} GckSessionOptions;

typedef struct _GckMechanism GckMechanism;

struct _GckMechanism {
	gulong type;
	gconstpointer parameter;
	gulong n_parameter;
};

typedef struct _GckAttribute GckAttribute;

struct _GckAttribute {
	gulong type;
	guchar *value;
	gulong length;
};

#define GCK_INVALID ((gulong)-1)

gboolean            gck_value_to_ulong                      (const guchar *value,
                                                             gsize length,
                                                             gulong *result);

gboolean            gck_value_to_boolean                    (const guchar *value,
                                                             gsize length,
                                                             gboolean *result);

void                gck_attribute_init                      (GckAttribute *attr,
                                                             gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

void                gck_attribute_init_invalid              (GckAttribute *attr,
                                                             gulong attr_type);

void                gck_attribute_init_empty                (GckAttribute *attr,
                                                             gulong attr_type);

void                gck_attribute_init_boolean              (GckAttribute *attr,
                                                             gulong attr_type,
                                                             gboolean value);

void                gck_attribute_init_date                 (GckAttribute *attr,
                                                             gulong attr_type,
                                                             const GDate *value);

void                gck_attribute_init_ulong                (GckAttribute *attr,
                                                             gulong attr_type,
                                                             gulong value);

void                gck_attribute_init_string               (GckAttribute *attr,
                                                             gulong attr_type,
                                                             const gchar *value);

void                gck_attribute_init_copy                 (GckAttribute *dest,
                                                             const GckAttribute *src);

#define             GCK_TYPE_ATTRIBUTE                      (gck_attribute_get_type ())

GType               gck_attribute_get_type                  (void) G_GNUC_CONST;

GckAttribute*       gck_attribute_new                       (gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

GckAttribute*       gck_attribute_new_invalid               (gulong attr_type);

GckAttribute*       gck_attribute_new_empty                 (gulong attr_type);

GckAttribute*       gck_attribute_new_boolean               (gulong attr_type,
                                                             gboolean value);

GckAttribute*       gck_attribute_new_date                  (gulong attr_type,
                                                             const GDate *value);

GckAttribute*       gck_attribute_new_ulong                 (gulong attr_type,
                                                             gulong value);

GckAttribute*       gck_attribute_new_string                (gulong attr_type,
                                                             const gchar *value);

gboolean            gck_attribute_is_invalid                (const GckAttribute *attr);

gboolean            gck_attribute_get_boolean               (const GckAttribute *attr);

gulong              gck_attribute_get_ulong                 (const GckAttribute *attr);

gchar*              gck_attribute_get_string                (const GckAttribute *attr);

void                gck_attribute_get_date                  (const GckAttribute *attr,
                                                             GDate* value);

const guchar *      gck_attribute_get_data                  (const GckAttribute *attr,
                                                             gsize *length);

gboolean            gck_attribute_equal                     (gconstpointer attr1,
                                                             gconstpointer attr2);

guint               gck_attribute_hash                      (gconstpointer attr);

GckAttribute*       gck_attribute_dup                       (const GckAttribute *attr);

void                gck_attribute_clear                     (GckAttribute *attr);

void                gck_attribute_free                      (gpointer attr);

void                gck_attribute_dump                      (const GckAttribute *attr);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckAttribute, gck_attribute_free);

typedef struct _GckBuilder GckBuilder;

struct _GckBuilder {
	/*< private >*/
	gsize x[16];
};

typedef enum {
	GCK_BUILDER_NONE,
	GCK_BUILDER_SECURE_MEMORY = 1,
} GckBuilderFlags;

typedef struct _GckAttributes GckAttributes;

GckBuilder *         gck_builder_new                        (GckBuilderFlags flags);

#define              GCK_BUILDER_INIT                       { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } }

GckBuilder *         gck_builder_ref                        (GckBuilder *builder);

void                 gck_builder_unref                      (GckBuilder *builder);

void                 gck_builder_init                       (GckBuilder *builder);

void                 gck_builder_init_full                  (GckBuilder *builder,
                                                             GckBuilderFlags flags);

#define              GCK_TYPE_BUILDER                       (gck_builder_get_type ())

GType                gck_builder_get_type                   (void) G_GNUC_CONST;

void                 gck_builder_take_data                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             guchar *value,
                                                             gsize length);

void                 gck_builder_add_data                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

void                 gck_builder_add_empty                  (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_add_invalid                (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_add_ulong                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gulong value);

void                 gck_builder_add_boolean                (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gboolean value);

void                 gck_builder_add_date                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const GDate *value);

void                 gck_builder_add_string                 (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const gchar *value);

void                 gck_builder_add_attribute              (GckBuilder *builder,
                                                             const GckAttribute *attr);

void                 gck_builder_add_all                    (GckBuilder *builder,
                                                             GckAttributes *attrs);

void                 gck_builder_add_only                   (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             gulong only_type,
                                                             ...);

void                 gck_builder_add_onlyv                  (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             const gulong *only_types,
                                                             guint n_only_types);

void                 gck_builder_add_except                 (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             gulong except_type,
                                                             ...);

void                 gck_builder_add_exceptv                (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             const gulong *except_types,
                                                             guint n_except_types);

void                 gck_builder_set_data                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

void                 gck_builder_set_empty                  (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_set_invalid                (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_set_ulong                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gulong value);

void                 gck_builder_set_boolean                (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gboolean value);

void                 gck_builder_set_date                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const GDate *value);

void                 gck_builder_set_string                 (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const gchar *value);

void                 gck_builder_set_all                    (GckBuilder *builder,
                                                             GckAttributes *attrs);

const GckAttribute * gck_builder_find                       (GckBuilder *builder,
                                                             gulong attr_type);

gboolean             gck_builder_find_boolean               (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gboolean *value);

gboolean             gck_builder_find_ulong                 (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gulong *value);

gboolean             gck_builder_find_string                (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gchar **value);

gboolean             gck_builder_find_date                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             GDate *value);

GckAttributes *      gck_builder_end                        (GckBuilder *builder);

GckBuilder *         gck_builder_copy                       (GckBuilder *builder);

void                 gck_builder_clear                      (GckBuilder *builder);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckBuilder, gck_builder_unref);

#define              GCK_TYPE_ATTRIBUTES                    (gck_attributes_get_type ())

GType                gck_attributes_get_type                (void) G_GNUC_CONST;

GckAttributes *      gck_attributes_new                     (void);

GckAttributes *      gck_attributes_new_empty               (gulong first_type,
                                                             ...);

const GckAttribute * gck_attributes_at                      (GckAttributes *attrs,
                                                             guint index);

const GckAttribute * gck_attributes_find                    (GckAttributes *attrs,
                                                             gulong attr_type);

gboolean             gck_attributes_find_boolean            (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gboolean *value);

gboolean             gck_attributes_find_ulong              (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gulong *value);

gboolean             gck_attributes_find_string             (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gchar **value);

gboolean             gck_attributes_find_date               (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             GDate *value);

gulong               gck_attributes_count                   (GckAttributes *attrs);

GckAttributes *      gck_attributes_ref                     (GckAttributes *attrs);

void                 gck_attributes_unref                   (gpointer attrs);

gboolean             gck_attributes_contains                (GckAttributes *attrs,
                                                             const GckAttribute *match);

void                 gck_attributes_dump                    (GckAttributes *attrs);

gchar *              gck_attributes_to_string               (GckAttributes *attrs);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckAttributes, gck_attributes_unref);

/* -------------------------------------------------------------------------
 * FORWARDS
 */
typedef struct _GckSlot GckSlot;
typedef struct _GckModule GckModule;
typedef struct _GckSession GckSession;
typedef struct _GckObject GckObject;
typedef struct _GckObjectCache GckObjectCache;
typedef struct _GckEnumerator GckEnumerator;
typedef struct _GckUriData GckUriData;

/* -------------------------------------------------------------------------
 * MODULE
 */

typedef struct _GckModuleInfo GckModuleInfo;

struct _GckModuleInfo {
	guint8 pkcs11_version_major;
	guint8 pkcs11_version_minor;

	gchar *manufacturer_id;
	gulong flags;

	gchar *library_description;
	guint8 library_version_major;
	guint8 library_version_minor;
};

#define             GCK_TYPE_MODULE_INFO                   (gck_module_info_get_type ())

GType               gck_module_info_get_type               (void) G_GNUC_CONST;

GckModuleInfo *     gck_module_info_copy                   (GckModuleInfo *module_info);

void                gck_module_info_free                   (GckModuleInfo *module_info);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckModuleInfo, gck_module_info_free);

#define GCK_TYPE_MODULE             (gck_module_get_type())
G_DECLARE_DERIVABLE_TYPE (GckModule, gck_module, GCK, MODULE, GObject)

struct _GckModuleClass {
	GObjectClass parent;

	gboolean (*authenticate_slot) (GckModule *self, GckSlot *slot, gchar *label, gchar **password);

	gboolean (*authenticate_object) (GckModule *self, GckObject *object, gchar *label, gchar **password);

	/*< private >*/
	gpointer reserved[8];
};

GckModule*            gck_module_new                          (CK_FUNCTION_LIST_PTR funcs);

GckModule*            gck_module_initialize                   (const gchar *path,
                                                               GCancellable *cancellable,
                                                               GError **error);

void                  gck_module_initialize_async             (const gchar *path,
                                                               GCancellable *cancellable,
                                                               GAsyncReadyCallback callback,
                                                               gpointer user_data);

GckModule *           gck_module_initialize_finish            (GAsyncResult *result,
                                                               GError **error);

gboolean              gck_module_equal                        (GckModule *module1,
                                                               GckModule *module2);

guint                 gck_module_hash                         (GckModule *module);

gboolean              gck_module_match                        (GckModule *self,
                                                               GckUriData *uri);

const gchar*          gck_module_get_path                     (GckModule *self);

CK_FUNCTION_LIST_PTR  gck_module_get_functions                (GckModule *self);

GckModuleInfo*        gck_module_get_info                     (GckModule *self);

GList*                gck_module_get_slots                    (GckModule *self,
                                                               gboolean token_present);

GList*                gck_modules_initialize_registered        (GCancellable *cancellable,
                                                                GError **error);

void                  gck_modules_initialize_registered_async  (GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

GList *               gck_modules_initialize_registered_finish (GAsyncResult *result,
                                                                GError **error);

GList*                gck_modules_get_slots                   (GList *modules,
                                                               gboolean token_present);

GckEnumerator*        gck_modules_enumerate_objects           (GList *modules,
                                                               GckAttributes *attrs,
                                                               GckSessionOptions session_options);

GckSlot*              gck_modules_token_for_uri               (GList *modules,
                                                               const gchar *uri,
                                                               GError **error);

GList *               gck_modules_tokens_for_uri              (GList *modules,
                                                               const gchar *uri,
                                                               GError **error);

GckObject*            gck_modules_object_for_uri              (GList *modules,
                                                               const gchar *uri,
                                                               GckSessionOptions session_options,
                                                               GError **error);

GList*                gck_modules_objects_for_uri             (GList *modules,
                                                               const gchar *uri,
                                                               GckSessionOptions session_options,
                                                               GError **error);

GckEnumerator*        gck_modules_enumerate_uri               (GList *modules,
                                                               const gchar *uri,
                                                               GckSessionOptions session_options,
                                                               GError **error);


/* ------------------------------------------------------------------------
 * ENUMERATOR
 */

#define GCK_TYPE_ENUMERATOR             (gck_enumerator_get_type())
G_DECLARE_FINAL_TYPE (GckEnumerator, gck_enumerator, GCK, ENUMERATOR, GObject)

GTlsInteraction *     gck_enumerator_get_interaction          (GckEnumerator *self);

void                  gck_enumerator_set_interaction          (GckEnumerator *self,
                                                               GTlsInteraction *interaction);

GType                 gck_enumerator_get_object_type          (GckEnumerator *self);

void                  gck_enumerator_set_object_type          (GckEnumerator *self,
                                                               GType object_type);

void                  gck_enumerator_set_object_type_full     (GckEnumerator *self,
                                                               GType object_type,
                                                               const gulong *attr_types,
                                                               gint attr_count);

GckEnumerator *       gck_enumerator_get_chained              (GckEnumerator *self);

void                  gck_enumerator_set_chained              (GckEnumerator *self,
                                                               GckEnumerator *chained);

GckObject *           gck_enumerator_next                     (GckEnumerator *self,
                                                               GCancellable *cancellable,
                                                               GError **error);

GList*                gck_enumerator_next_n                   (GckEnumerator *self,
                                                               gint max_objects,
                                                               GCancellable *cancellable,
                                                               GError **error);

void                  gck_enumerator_next_async               (GckEnumerator *self,
                                                               gint max_objects,
                                                               GCancellable *cancellable,
                                                               GAsyncReadyCallback callback,
                                                               gpointer user_data);

GList*                gck_enumerator_next_finish              (GckEnumerator *self,
                                                               GAsyncResult *result,
                                                               GError **error);

/* ------------------------------------------------------------------------
 * SLOT
 */

typedef struct _GckSlotInfo GckSlotInfo;

struct _GckSlotInfo {
	gchar *slot_description;
	gchar *manufacturer_id;
	gulong flags;
	guint8 hardware_version_major;
	guint8 hardware_version_minor;
	guint8 firmware_version_major;
	guint8 firmware_version_minor;
};

#define             GCK_TYPE_SLOT_INFO                      (gck_slot_info_get_type ())

GType               gck_slot_info_get_type                  (void) G_GNUC_CONST;

GckSlotInfo *       gck_slot_info_copy                      (GckSlotInfo *slot_info);

void                gck_slot_info_free                      (GckSlotInfo *slot_info);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckSlotInfo, gck_slot_info_free);

typedef struct _GckTokenInfo GckTokenInfo;

struct _GckTokenInfo {
	gchar *label;
	gchar *manufacturer_id;
	gchar *model;
	gchar *serial_number;
	gulong flags;
	glong max_session_count;
	glong session_count;
	glong max_rw_session_count;
	glong rw_session_count;
	glong max_pin_len;
	glong min_pin_len;
	glong total_public_memory;
	glong free_public_memory;
	glong total_private_memory;
	glong free_private_memory;
	guint8 hardware_version_major;
	guint8 hardware_version_minor;
	guint8 firmware_version_major;
	guint8 firmware_version_minor;
	GDateTime *utc_time;
};

#define             GCK_TYPE_TOKEN_INFO                     (gck_token_info_get_type ())

GType               gck_token_info_get_type                 (void) G_GNUC_CONST;

GckTokenInfo *      gck_token_info_copy                     (GckTokenInfo *token_info);

void                gck_token_info_free                     (GckTokenInfo *token_info);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckTokenInfo, gck_token_info_free);

typedef struct _GckMechanismInfo GckMechanismInfo;

struct _GckMechanismInfo {
	gulong min_key_size;
	gulong max_key_size;
	gulong flags;
};

#define             GCK_TYPE_MECHANISM_INFO                 (gck_mechanism_info_get_type ())

GType               gck_mechanism_info_get_type             (void) G_GNUC_CONST;

GckMechanismInfo *  gck_mechanism_info_copy                 (GckMechanismInfo *mech_info);

void                gck_mechanism_info_free                 (GckMechanismInfo *mech_info);

#define             gck_mechanisms_length(a)                ((a)->len)

#define             gck_mechanisms_at(a, i)                 (g_array_index (a, CK_MECHANISM_TYPE, i))

gboolean            gck_mechanisms_check                    (GArray *mechanisms,
                                                             ...);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckMechanismInfo, gck_mechanism_info_free);

#define GCK_TYPE_SLOT             (gck_slot_get_type())
G_DECLARE_FINAL_TYPE (GckSlot, gck_slot, GCK, SLOT, GObject)

gboolean            gck_slot_equal                          (GckSlot *slot1,
                                                             GckSlot *slot2);

guint               gck_slot_hash                           (GckSlot *slot);

gboolean            gck_slot_match                          (GckSlot *self,
                                                             GckUriData *uri);

GckSlot*            gck_slot_from_handle                    (GckModule *module,
                                                             gulong slot_id);

GckModule*          gck_slot_get_module                     (GckSlot *self);

gulong              gck_slot_get_handle                     (GckSlot *self);

GckSlotInfo*        gck_slot_get_info                       (GckSlot *self);

GckTokenInfo*       gck_slot_get_token_info                 (GckSlot *self);

GArray *            gck_slot_get_mechanisms                 (GckSlot *self);

GckMechanismInfo*   gck_slot_get_mechanism_info             (GckSlot *self,
                                                             gulong mech_type);

gboolean            gck_slot_has_flags                      (GckSlot *self,
                                                             gulong flags);

GckSession*         gck_slot_open_session                   (GckSlot *self,
                                                             GckSessionOptions options,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GError **error);

GckSession*         gck_slot_open_session_full              (GckSlot *self,
                                                             GckSessionOptions options,
                                                             GTlsInteraction *interaction,
                                                             gulong pkcs11_flags,
                                                             gpointer app_data,
                                                             CK_NOTIFY notify,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_slot_open_session_async             (GckSlot *self,
                                                             GckSessionOptions options,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

void                gck_slot_open_session_full_async        (GckSlot *self,
                                                             GckSessionOptions options,
                                                             GTlsInteraction *interaction,
                                                             gulong pkcs11_flags,
                                                             gpointer app_data,
                                                             CK_NOTIFY notify,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

GckSession*         gck_slot_open_session_finish            (GckSlot *self,
                                                             GAsyncResult *result,
                                                             GError **error);

GckEnumerator *     gck_slot_enumerate_objects              (GckSlot *self,
                                                             GckAttributes *match,
                                                             GckSessionOptions options);

GckEnumerator*      gck_slots_enumerate_objects             (GList *slots,
                                                             GckAttributes *match,
                                                             GckSessionOptions options);

/* ------------------------------------------------------------------------
 * SESSION
 */

typedef struct _GckSessionInfo GckSessionInfo;

struct _GckSessionInfo {
	gulong slot_id;
	gulong state;
	gulong flags;
	gulong device_error;
};

#define             GCK_TYPE_SESSION_INFO                  (gck_session_info_get_type ())

GType               gck_session_info_get_type              (void) G_GNUC_CONST;

GckSessionInfo *    gck_session_info_copy                  (GckSessionInfo *session_info);

void                gck_session_info_free                  (GckSessionInfo *session_info);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckSessionInfo, gck_session_info_free);

#define GCK_TYPE_SESSION             (gck_session_get_type())
G_DECLARE_DERIVABLE_TYPE (GckSession, gck_session, GCK, SESSION, GObject)

struct _GckSessionClass {
	GObjectClass parent;

	gboolean (*discard_handle) (GckSession *session, CK_SESSION_HANDLE handle);

	/*< private >*/
	gpointer reserved[8];
};

GckSession *        gck_session_from_handle                 (GckSlot *slot,
                                                             gulong session_handle,
                                                             GckSessionOptions options);

GckModule*          gck_session_get_module                  (GckSession *self);

GckSlot*            gck_session_get_slot                    (GckSession *self);

gulong              gck_session_get_handle                  (GckSession *self);

GckSessionInfo*     gck_session_get_info                    (GckSession *self);

gulong              gck_session_get_state                   (GckSession *self);

GckSessionOptions   gck_session_get_options                 (GckSession *self);

GTlsInteraction *   gck_session_get_interaction             (GckSession *self);

void                gck_session_set_interaction             (GckSession *self,
                                                             GTlsInteraction *interaction);

GckSession *        gck_session_open                        (GckSlot *slot,
                                                             GckSessionOptions options,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_open_async                  (GckSlot *slot,
                                                             GckSessionOptions options,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

GckSession *        gck_session_open_finish                 (GAsyncResult *result,
                                                             GError **error);

gboolean            gck_session_init_pin                    (GckSession *self,
                                                             const guchar *pin,
                                                             gsize n_pin,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_init_pin_async              (GckSession *self,
                                                             const guchar *pin,
                                                             gsize n_pin,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_session_init_pin_finish             (GckSession *self,
                                                             GAsyncResult *result,
                                                             GError **error);

gboolean            gck_session_set_pin                     (GckSession *self,
                                                             const guchar *old_pin,
                                                             gsize n_old_pin,
                                                             const guchar *new_pin,
                                                             gsize n_new_pin,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_set_pin_async               (GckSession *self,
                                                             const guchar *old_pin,
                                                             gsize n_old_pin,
                                                             const guchar *new_pin,
                                                             gsize n_new_pin,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_session_set_pin_finish              (GckSession *self,
                                                             GAsyncResult *result,
                                                             GError **error);

gboolean            gck_session_login                       (GckSession *self,
                                                             gulong user_type,
                                                             const guchar *pin,
                                                             gsize n_pin,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_login_async                 (GckSession *self,
                                                             gulong user_type,
                                                             const guchar *pin,
                                                             gsize n_pin,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_session_login_finish                (GckSession *self,
                                                             GAsyncResult *result,
                                                             GError **error);

gboolean            gck_session_login_interactive           (GckSession *self,
                                                             gulong user_type,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_login_interactive_async     (GckSession *self,
                                                             gulong user_type,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_session_login_interactive_finish    (GckSession *self,
                                                             GAsyncResult *result,
                                                             GError **error);

gboolean            gck_session_logout                      (GckSession *self,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_logout_async                (GckSession *self,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_session_logout_finish               (GckSession *self,
                                                             GAsyncResult *result,
                                                             GError **error);

GckObject*          gck_session_create_object               (GckSession *self,
                                                             GckAttributes *attrs,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_create_object_async         (GckSession *self,
                                                             GckAttributes *attrs,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

GckObject*          gck_session_create_object_finish        (GckSession *self,
                                                             GAsyncResult *result,
                                                             GError **error);

GList*              gck_session_find_objects                (GckSession *self,
                                                             GckAttributes *match,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_find_objects_async          (GckSession *self,
                                                             GckAttributes *match,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

GList*              gck_session_find_objects_finish         (GckSession *self,
                                                             GAsyncResult *result,
                                                             GError **error);

gulong *            gck_session_find_handles                (GckSession *self,
                                                             GckAttributes *match,
                                                             GCancellable *cancellable,
                                                             gulong *n_handles,
                                                             GError **error);

void                gck_session_find_handles_async          (GckSession *self,
                                                             GckAttributes *match,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gulong *            gck_session_find_handles_finish         (GckSession *self,
                                                             GAsyncResult *result,
                                                             gulong *n_handles,
                                                             GError **error);

GckEnumerator *     gck_session_enumerate_objects           (GckSession *self,
                                                             GckAttributes *match);

gboolean            gck_session_generate_key_pair           (GckSession *self,
                                                             gulong mech_type,
                                                             GckAttributes *public_attrs,
                                                             GckAttributes *private_attrs,
                                                             GckObject **public_key,
                                                             GckObject **private_key,
                                                             GCancellable *cancellable,
                                                             GError **error);

gboolean            gck_session_generate_key_pair_full      (GckSession *self,
                                                             GckMechanism *mechanism,
                                                             GckAttributes *public_attrs,
                                                             GckAttributes *private_attrs,
                                                             GckObject **public_key,
                                                             GckObject **private_key,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_session_generate_key_pair_async     (GckSession *self,
                                                             GckMechanism *mechanism,
                                                             GckAttributes *public_attrs,
                                                             GckAttributes *private_attrs,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_session_generate_key_pair_finish    (GckSession *self,
                                                             GAsyncResult *result,
                                                             GckObject **public_key,
                                                             GckObject **private_key,
                                                             GError **error);

guchar*             gck_session_encrypt                      (GckSession *self,
                                                              GckObject *key,
                                                              gulong mech_type,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

guchar*             gck_session_encrypt_full                 (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                gck_session_encrypt_async                (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

guchar*             gck_session_encrypt_finish               (GckSession *self,
                                                              GAsyncResult *result,
                                                              gsize *n_result,
                                                              GError **error);

guchar*             gck_session_decrypt                      (GckSession *self,
                                                              GckObject *key,
                                                              gulong mech_type,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

guchar*             gck_session_decrypt_full                 (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                gck_session_decrypt_async                (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

guchar*             gck_session_decrypt_finish               (GckSession *self,
                                                              GAsyncResult *result,
                                                              gsize *n_result,
                                                              GError **error);

guchar*             gck_session_sign                         (GckSession *self,
                                                              GckObject *key,
                                                              gulong mech_type,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

guchar*             gck_session_sign_full                    (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                gck_session_sign_async                   (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

guchar*             gck_session_sign_finish                  (GckSession *self,
                                                              GAsyncResult *result,
                                                              gsize *n_result,
                                                              GError **error);

gboolean            gck_session_verify                       (GckSession *self,
                                                              GckObject *key,
                                                              gulong mech_type,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              const guchar *signature,
                                                              gsize n_signature,
                                                              GCancellable *cancellable,
                                                              GError **error);

gboolean            gck_session_verify_full                  (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              const guchar *signature,
                                                              gsize n_signature,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                gck_session_verify_async                 (GckSession *self,
                                                              GckObject *key,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              const guchar *signature,
                                                              gsize n_signature,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            gck_session_verify_finish                (GckSession *self,
                                                              GAsyncResult *result,
                                                              GError **error);

guchar *            gck_session_wrap_key                     (GckSession *self,
                                                              GckObject *wrapper,
                                                              gulong mech_type,
                                                              GckObject *wrapped,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

guchar *            gck_session_wrap_key_full                (GckSession *self,
                                                              GckObject *wrapper,
                                                              GckMechanism *mechanism,
                                                              GckObject *wrapped,
                                                              gsize *n_result,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                gck_session_wrap_key_async               (GckSession *self,
                                                              GckObject *wrapper,
                                                              GckMechanism *mechanism,
                                                              GckObject *wrapped,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

guchar *            gck_session_wrap_key_finish              (GckSession *self,
                                                              GAsyncResult *result,
                                                              gsize *n_result,
                                                              GError **error);

GckObject*          gck_session_unwrap_key                   (GckSession *self,
                                                              GckObject *wrapper,
                                                              gulong mech_type,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              GckAttributes *attrs,
                                                              GCancellable *cancellable,
                                                              GError **error);

GckObject*          gck_session_unwrap_key_full              (GckSession *self,
                                                              GckObject *wrapper,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              GckAttributes *attrs,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                gck_session_unwrap_key_async             (GckSession *self,
                                                              GckObject *wrapper,
                                                              GckMechanism *mechanism,
                                                              const guchar *input,
                                                              gsize n_input,
                                                              GckAttributes *attrs,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GckObject*          gck_session_unwrap_key_finish            (GckSession *self,
                                                              GAsyncResult *result,
                                                              GError **error);

GckObject*          gck_session_derive_key                   (GckSession *self,
                                                              GckObject *base,
                                                              gulong mech_type,
                                                              GckAttributes *attrs,
                                                              GCancellable *cancellable,
                                                              GError **error);

GckObject*          gck_session_derive_key_full              (GckSession *self,
                                                              GckObject *base,
                                                              GckMechanism *mechanism,
                                                              GckAttributes *attrs,
                                                              GCancellable *cancellable,
                                                              GError **error);

void                gck_session_derive_key_async             (GckSession *self,
                                                              GckObject *base,
                                                              GckMechanism *mechanism,
                                                              GckAttributes *attrs,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GckObject*          gck_session_derive_key_finish            (GckSession *self,
                                                              GAsyncResult *result,
                                                              GError **error);

/* ------------------------------------------------------------------------
 * OBJECT
 */

#define GCK_TYPE_OBJECT             (gck_object_get_type())
G_DECLARE_DERIVABLE_TYPE (GckObject, gck_object, GCK, OBJECT, GObject)

struct _GckObjectClass {
	GObjectClass parent;

	/*< private >*/
	gpointer reserved[8];
};

GckObject *         gck_object_from_handle                  (GckSession *session,
                                                             gulong object_handle);

GList*              gck_objects_from_handle_array           (GckSession *session,
                                                             gulong *object_handles,
                                                             gulong n_object_handles);

gboolean            gck_object_equal                        (GckObject *object1,
                                                             GckObject *object2);

guint               gck_object_hash                         (GckObject *object);

GckModule*          gck_object_get_module                   (GckObject *self);

gulong              gck_object_get_handle                   (GckObject *self);

GckSession*         gck_object_get_session                  (GckObject *self);

gboolean            gck_object_destroy                      (GckObject *self,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_object_destroy_async                (GckObject *self,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_object_destroy_finish               (GckObject *self,
                                                             GAsyncResult *result,
                                                             GError **error);

gboolean            gck_object_set                          (GckObject *self,
                                                             GckAttributes *attrs,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_object_set_async                    (GckObject *self,
                                                             GckAttributes *attrs,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_object_set_finish                   (GckObject *self,
                                                             GAsyncResult *result,
                                                             GError **error);

GckAttributes*      gck_object_get                          (GckObject *self,
                                                             GCancellable *cancellable,
                                                             GError **error,
                                                             ...);

GckAttributes*      gck_object_get_full                     (GckObject *self,
                                                             const gulong *attr_types,
                                                             guint n_attr_types,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_object_get_async                    (GckObject *self,
                                                             const gulong *attr_types,
                                                             guint n_attr_types,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

GckAttributes*      gck_object_get_finish                   (GckObject *self,
                                                             GAsyncResult *result,
                                                             GError **error);

guchar *            gck_object_get_data                     (GckObject *self,
                                                             gulong attr_type,
                                                             GCancellable *cancellable,
                                                             gsize *n_data,
                                                             GError **error);

guchar *            gck_object_get_data_full                (GckObject *self,
                                                             gulong attr_type,
                                                             GckAllocator allocator,
                                                             GCancellable *cancellable,
                                                             gsize *n_data,
                                                             GError **error);

void                gck_object_get_data_async               (GckObject *self,
                                                             gulong attr_type,
                                                             GckAllocator allocator,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

guchar *            gck_object_get_data_finish              (GckObject *self,
                                                             GAsyncResult *result,
                                                             gsize *n_data,
                                                             GError **error);

gboolean            gck_object_set_template                 (GckObject *self,
                                                             gulong attr_type,
                                                             GckAttributes *attrs,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_object_set_template_async           (GckObject *self,
                                                             gulong attr_type,
                                                             GckAttributes *attrs,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean            gck_object_set_template_finish          (GckObject *self,
                                                             GAsyncResult *result,
                                                             GError **error);

GckAttributes*      gck_object_get_template                 (GckObject *self,
                                                             gulong attr_type,
                                                             GCancellable *cancellable,
                                                             GError **error);

void                gck_object_get_template_async           (GckObject *self,
                                                             gulong attr_type,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

GckAttributes*      gck_object_get_template_finish          (GckObject *self,
                                                             GAsyncResult *result,
                                                             GError **error);

/* ------------------------------------------------------------------------
 * OBJECT ATTRIBUTES
 */

#define GCK_TYPE_OBJECT_CACHE gck_object_cache_get_type ()
G_DECLARE_INTERFACE (GckObjectCache, gck_object_cache, GCK, OBJECT_CACHE, GckObject)

struct _GckObjectCacheInterface {
	GTypeInterface interface;

	const gulong *  default_types;
	gint            n_default_types;

	void         (* fill)                              (GckObjectCache *object,
	                                                    GckAttributes *attrs);

	/*< private >*/
	gpointer reserved[6];
};

GckAttributes *     gck_object_cache_get_attributes        (GckObjectCache *object);

void                gck_object_cache_set_attributes        (GckObjectCache *object,
                                                            GckAttributes *attrs);

void                gck_object_cache_fill                  (GckObjectCache *object,
                                                            GckAttributes *attrs);

gboolean            gck_object_cache_update                (GckObjectCache *object,
                                                            const gulong *attr_types,
                                                            gint n_attr_types,
                                                            GCancellable *cancellable,
                                                            GError **error);

void                gck_object_cache_update_async          (GckObjectCache *object,
                                                            const gulong *attr_types,
                                                            gint n_attr_types,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gboolean            gck_object_cache_update_finish         (GckObjectCache *object,
                                                            GAsyncResult *result,
                                                            GError **error);

GckAttributes *     gck_object_cache_lookup                (GckObject *object,
                                                            const gulong *attr_types,
                                                            gint n_attr_types,
                                                            GCancellable *cancellable,
                                                            GError **error);

void                gck_object_cache_lookup_async          (GckObject *object,
                                                            const gulong *attr_types,
                                                            gint n_attr_types,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

GckAttributes *     gck_object_cache_lookup_finish         (GckObject *object,
                                                            GAsyncResult *result,
                                                            GError **error);

/* ------------------------------------------------------------------------
 * PASSWORD
 */

#define GCK_TYPE_PASSWORD             (gck_password_get_type ())
G_DECLARE_FINAL_TYPE (GckPassword, gck_password, GCK, PASSWORD, GTlsPassword)

GckModule *         gck_password_get_module                 (GckPassword *self);

GckSlot *           gck_password_get_token                  (GckPassword *self);

GckObject *         gck_password_get_key                    (GckPassword *self);

/* ----------------------------------------------------------------------------
 * URI
 */

typedef enum {
	GCK_URI_BAD_SCHEME = 1,
	GCK_URI_BAD_ENCODING,
	GCK_URI_BAD_SYNTAX,
	GCK_URI_BAD_VERSION,
	GCK_URI_NOT_FOUND
} GckUriError;

/* WARNING: Don't modify these without syncing with p11-kit */
typedef enum {
	GCK_URI_FOR_OBJECT =  (1 << 1),
	GCK_URI_FOR_TOKEN =   (1 << 2),
	GCK_URI_FOR_MODULE =  (1 << 3),
	GCK_URI_WITH_VERSION = (1 << 4),
	GCK_URI_FOR_ANY =     0x0000FFFF,
} GckUriFlags;

#define             GCK_URI_FOR_MODULE_WITH_VERSION         (GCK_URI_WITH_VERSION | GCK_URI_FOR_MODULE)

#define             GCK_URI_FOR_OBJECT_ON_TOKEN             (GCK_URI_FOR_OBJECT | GCK_URI_FOR_TOKEN)

#define             GCK_URI_FOR_OBJECT_ON_TOKEN_AND_MODULE  (GCK_URI_FOR_OBJECT_ON_TOKEN | GCK_URI_FOR_MODULE)

struct _GckUriData {
	gboolean any_unrecognized;
	GckModuleInfo *module_info;
	GckTokenInfo *token_info;
	GckAttributes *attributes;

	/*< private >*/
	gpointer dummy[4];
};

#define             GCK_URI_ERROR                           (gck_uri_error_quark ())

GQuark              gck_uri_error_quark                     (void) G_GNUC_CONST;

GckUriData*         gck_uri_data_new                        (void);

gchar*              gck_uri_data_build                      (GckUriData *uri_data,
                                                             GckUriFlags flags);

GckUriData*         gck_uri_data_parse                      (const gchar *string,
                                                             GckUriFlags flags,
                                                             GError **error);

#define             GCK_URI_DATA_TYPE                       (gck_uri_data_get_type ())

GType               gck_uri_data_get_type                   (void) G_GNUC_CONST;

GckUriData *        gck_uri_data_copy                       (GckUriData *uri_data);

void                gck_uri_data_free                       (GckUriData *uri_data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckUriData, gck_uri_data_free);

G_END_DECLS

#undef __GCK_INSIDE_HEADER__

#endif /* GCK_H */
