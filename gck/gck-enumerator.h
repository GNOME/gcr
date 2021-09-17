/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_ENUMERATOR_H__
#define __GCK_ENUMERATOR_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include <gck/gck-object.h>

G_BEGIN_DECLS

#define GCK_TYPE_ENUMERATOR             (gck_enumerator_get_type())
#define GCK_ENUMERATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GCK_TYPE_ENUMERATOR, GckEnumerator))
#define GCK_ENUMERATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GCK_TYPE_ENUMERATOR, GckEnumerator))
#define GCK_IS_ENUMERATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCK_TYPE_ENUMERATOR))
#define GCK_IS_ENUMERATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GCK_TYPE_ENUMERATOR))
#define GCK_ENUMERATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GCK_TYPE_ENUMERATOR, GckEnumeratorClass))

typedef struct _GckEnumerator GckEnumerator;
typedef struct _GckEnumeratorClass GckEnumeratorClass;
typedef struct _GckEnumeratorPrivate GckEnumeratorPrivate;

struct _GckEnumerator {
	GObject parent;

	/*< private >*/
	GckEnumeratorPrivate *pv;
	gpointer reserved[2];
};

struct _GckEnumeratorClass {
	GObjectClass parent;

	/*< private >*/
	gpointer reserved[2];
};

GType                 gck_enumerator_get_type                 (void) G_GNUC_CONST;

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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckEnumerator, g_object_unref);

G_END_DECLS

#endif /* __GCK_ENUMERATOR_H__ */
