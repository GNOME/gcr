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
#include <gio/gio.h>

#include <gck/gck-module.h>
#include <gck/gck-slot.h>
#include <gck/gck-object.h>

G_BEGIN_DECLS

#define GCK_TYPE_PASSWORD gck_password_get_type ()
G_DECLARE_FINAL_TYPE (GckPassword, gck_password, GCK, PASSWORD, GTlsPassword)

GckModule *         gck_password_get_module                 (GckPassword *self);

GckSlot *           gck_password_get_token                  (GckPassword *self);

GckObject *         gck_password_get_key                    (GckPassword *self);

G_END_DECLS

#endif /* __GCK_PASSWORD_H__ */
