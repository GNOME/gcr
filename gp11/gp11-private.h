/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gp11-private.h - the GObject PKCS#11 wrapper library

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Stef Walter <nielsen@memberwebs.com>
*/

#ifndef GP11_PRIVATE_H_
#define GP11_PRIVATE_H_

#include "gp11.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* ---------------------------------------------------------------------------
 * ATTRIBUTE INTERNALS
 */

void                _gp11_attribute_init_take               (GP11Attribute *attr,
                                                             gulong attr_type,
                                                             gpointer value,
                                                             gsize length);

void                _gp11_attributes_add_take               (GP11Attributes *attr,
                                                             gulong attr_type,
                                                             gpointer value,
                                                             gsize length);

CK_ATTRIBUTE_PTR    _gp11_attributes_raw                    (GP11Attributes *attrs);

/* ----------------------------------------------------------------------------
 * SLOT
 */

gboolean            _gp11_slot_token_authentication       (GP11Slot *slot,
                                                           gchar **password);

/* ----------------------------------------------------------------------------
 * CALL
 */

typedef CK_RV (*GP11CallFunc) (gpointer call_data);

typedef struct _GP11Call GP11Call;

typedef struct _GP11Arguments {
	GP11Call *call;

	/* For the call function to use */
	CK_FUNCTION_LIST_PTR pkcs11;
	CK_ULONG handle;

} GP11Arguments;

#define GP11_ARGUMENTS_INIT 	   { NULL, NULL, 0 }

#define GP11_TYPE_CALL             (_gp11_call_get_type())
#define GP11_CALL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GP11_TYPE_CALL, GP11Call))
#define GP11_CALL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GP11_TYPE_CALL, GP11Call))
#define GP11_IS_CALL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GP11_TYPE_CALL))
#define GP11_IS_CALL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GP11_TYPE_CALL))
#define GP11_CALL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GP11_TYPE_CALL, GP11CallClass))

typedef struct _GP11CallClass GP11CallClass;

struct _GP11Call {
	GObject parent;

	/* For making the call */
	GP11CallFunc func;
	GP11Arguments *args;
	GCancellable *cancellable;
	GDestroyNotify destroy;
	CK_RV rv;

	/* For result callback only */
	gpointer object;
	GAsyncReadyCallback callback;
	gpointer user_data;

	/* For authenticating */
	gboolean do_login;
	gchar *password;
};

struct _GP11CallClass {
	GObjectClass parent;
};

GType              _gp11_call_get_type                    (void) G_GNUC_CONST;

#define            _gp11_call_arguments(call, type) \
			(type*)(GP11_CALL (call)->args)

void               _gp11_call_uninitialize                (void);

gboolean           _gp11_call_sync                        (gpointer object,
                                                           gpointer func,
                                                           gpointer args,
                                                           GCancellable *cancellable,
                                                           GError **err);

gpointer           _gp11_call_async_prep                  (gpointer object,
                                                           gpointer cb_object,
                                                           gpointer func,
                                                           gsize args_size,
                                                           gpointer destroy_func);

void               _gp11_call_async_go                    (gpointer args,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

void                _gp11_call_async_short                (gpointer data,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

gboolean           _gp11_call_basic_finish                (gpointer object,
                                                           GAsyncResult *result,
                                                           GError **err);

#endif /* GP11_PRIVATE_H_ */