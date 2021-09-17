/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_SESSION_H__
#define __GCK_SESSION_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>
#include <p11-kit/p11-kit.h>

#include <gck/gck-object.h>
#include <gck/gck-slot.h>
#include <gck/gck-session.h>

G_BEGIN_DECLS

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
#define GCK_SESSION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GCK_TYPE_SESSION, GckSession))
#define GCK_SESSION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GCK_TYPE_SESSION, GckSession))
#define GCK_IS_SESSION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCK_TYPE_SESSION))
#define GCK_IS_SESSION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GCK_TYPE_SESSION))
#define GCK_SESSION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GCK_TYPE_SESSION, GckSessionClass))

typedef struct _GckSessionClass GckSessionClass;
typedef struct _GckSessionPrivate GckSessionPrivate;

struct _GckSession {
	GObject parent;

	/*< private >*/
	GckSessionPrivate *pv;
	gpointer reserved[4];
};

struct _GckSessionClass {
	GObjectClass parent;

	gboolean (*discard_handle) (GckSession *session, CK_SESSION_HANDLE handle);

	/*< private >*/
	gpointer reserved[8];
};

GType               gck_session_get_type                    (void) G_GNUC_CONST;

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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckSession, g_object_unref);

G_END_DECLS

#endif /* __GCK_SESSION_H__ */
