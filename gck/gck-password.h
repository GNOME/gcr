/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_PASSWORD_H__
#define __GCK_PASSWORD_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>

#include <gck/gck-module.h>
#include <gck/gck-slot.h>
#include <gck/gck-object.h>

G_BEGIN_DECLS

#define GCK_TYPE_PASSWORD             (gck_password_get_type ())
#define GCK_PASSWORD(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCK_TYPE_PASSWORD, GckPassword))
#define GCK_PASSWORD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GCK_TYPE_PASSWORD, GckPassword))
#define GCK_IS_PASSWORD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCK_TYPE_PASSWORD))
#define GCK_IS_PASSWORD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GCK_TYPE_PASSWORD))
#define GCK_PASSWORD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GCK_TYPE_PASSWORD, GckPasswordClass))

typedef struct _GckPassword GckPassword;
typedef struct _GckPasswordClass GckPasswordClass;
typedef struct _GckPasswordPrivate GckPasswordPrivate;

struct _GckPassword {
	GTlsPassword parent;

	/*< private >*/
	GckPasswordPrivate *pv;
	gpointer reserved[4];
};

struct _GckPasswordClass {
	GTlsPasswordClass parent;

	/*< private >*/
	gpointer reserved[4];
};

GType               gck_password_get_type                   (void) G_GNUC_CONST;

GckModule *         gck_password_get_module                 (GckPassword *self);

GckSlot *           gck_password_get_token                  (GckPassword *self);

GckObject *         gck_password_get_key                    (GckPassword *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckPassword, g_object_unref);

G_END_DECLS

#endif /* __GCK_PASSWORD_H__ */
