/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_OBJECT_H__
#define __GCK_OBJECT_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include <gck/gck-attributes.h>

G_BEGIN_DECLS

/* FORWARD DECLARATIONS */
typedef struct _GckModule GckModule;
typedef struct _GckSession GckSession;

#define GCK_TYPE_OBJECT             (gck_object_get_type())
#define GCK_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GCK_TYPE_OBJECT, GckObject))
#define GCK_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GCK_TYPE_OBJECT, GckObjectClass))
#define GCK_IS_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCK_TYPE_OBJECT))
#define GCK_IS_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GCK_TYPE_OBJECT))
#define GCK_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GCK_TYPE_OBJECT, GckObjectClass))

typedef struct _GckObject GckObject;
typedef struct _GckObjectClass GckObjectClass;
typedef struct _GckObjectPrivate GckObjectPrivate;

struct _GckObject {
	GObject parent;

	/*< private >*/
	GckObjectPrivate *pv;
	gpointer reserved[4];
};

struct _GckObjectClass {
	GObjectClass parent;

	/*< private >*/
	gpointer reserved[8];
};

GType               gck_object_get_type                     (void) G_GNUC_CONST;

GckObject *         gck_object_from_handle                  (GckSession *session,
                                                             gulong object_handle);

GList*              gck_objects_from_handle_array           (GckSession *session,
                                                             gulong *object_handles,
                                                             gulong n_object_handles);

gboolean            gck_object_equal                        (gconstpointer object1,
                                                             gconstpointer object2);

guint               gck_object_hash                         (gconstpointer object);

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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckObject, g_object_unref);

G_END_DECLS

#endif /* __GCK_OBJECT_H__ */
