/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_SLOT_H__
#define __GCK_SLOT_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <p11-kit/p11-kit.h>

#include <gck/gck-enumerator.h>

G_BEGIN_DECLS

/* FORWARD DECLARATION */
typedef struct _GckUriData GckUriData;

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
	gint64 utc_time;
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
#define GCK_SLOT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GCK_TYPE_SLOT, GckSlot))
#define GCK_SLOT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GCK_TYPE_SLOT, GckSlot))
#define GCK_IS_SLOT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCK_TYPE_SLOT))
#define GCK_IS_SLOT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GCK_TYPE_SLOT))
#define GCK_SLOT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GCK_TYPE_SLOT, GckSlotClass))

typedef struct _GckSlot GckSlot;
typedef struct _GckSlotClass GckSlotClass;
typedef struct _GckSlotPrivate GckSlotPrivate;

struct _GckSlot {
	GObject parent;

	/*< private >*/
	GckSlotPrivate *pv;
	gpointer reserved[4];
};

struct _GckSlotClass {
	GObjectClass parent;

	/*< private >*/
	gpointer reserved[9];
};

GType               gck_slot_get_type                       (void) G_GNUC_CONST;

gboolean            gck_slot_equal                          (gconstpointer slot1,
                                                             gconstpointer slot2);

guint               gck_slot_hash                           (gconstpointer slot);

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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckSlot, g_object_unref);

G_END_DECLS

#endif /* __GCK_SLOT_H__ */
