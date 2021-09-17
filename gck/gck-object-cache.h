/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_OBJECT_CACHE_H__
#define __GCK_OBJECT_CACHE_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCK_TYPE_OBJECT_CACHE                    (gck_object_cache_get_type ())
#define GCK_OBJECT_CACHE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCK_TYPE_OBJECT_CACHE, GckObjectCache))
#define GCK_IS_OBJECT_CACHE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCK_TYPE_OBJECT_CACHE))
#define GCK_OBJECT_CACHE_GET_INTERFACE(inst)     (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GCK_TYPE_OBJECT_CACHE, GckObjectCacheIface))

typedef struct _GckObjectCache GckObjectCache;
typedef struct _GckObjectCacheIface GckObjectCacheIface;

struct _GckObjectCacheIface {
	GTypeInterface interface;

	const gulong *  default_types;
	gint            n_default_types;

	void         (* fill)                              (GckObjectCache *object,
	                                                    GckAttributes *attrs);

	/*< private >*/
	gpointer reserved[6];
};

GType               gck_object_cache_get_type              (void) G_GNUC_CONST;

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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckObjectCache, g_object_unref);

G_END_DECLS

#endif /* __GCK_OBJECT_CACHE_H__ */
