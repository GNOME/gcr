/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-private.h - the GObject PKCS#11 wrapper library

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

#ifndef GCK_PRIVATE_H_
#define GCK_PRIVATE_H_

#include "gck.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCK_IS_GET_ATTRIBUTE_RV_OK(rv) \
	((rv) == CKR_OK || (rv) == CKR_ATTRIBUTE_SENSITIVE || (rv) == CKR_ATTRIBUTE_TYPE_INVALID)

/* ---------------------------------------------------------------------------
 * ATTRIBUTE INTERNALS
 */

CK_ATTRIBUTE_PTR    _gck_attributes_commit_out             (GckAttributes *attrs,
                                                             CK_ULONG_PTR n_attrs);

CK_ATTRIBUTE_PTR    _gck_builder_prepare_in                (GckBuilder *attrs,
                                                            CK_ULONG_PTR n_attrs);

CK_ATTRIBUTE_PTR    _gck_builder_commit_in                 (GckBuilder *attrs,
                                                            CK_ULONG_PTR n_attrs);

/* ----------------------------------------------------------------------------
 * MISC
 */

guint               _gck_ulong_hash                        (gconstpointer v);

gboolean            _gck_ulong_equal                       (gconstpointer v1,
                                                             gconstpointer v2);

const gchar *       _gck_stringize_rv                      (CK_RV rv);

CK_RV               _gck_rv_from_error                     (GError *error,
                                                            CK_RV catch_all_code);

/* ----------------------------------------------------------------------------
 * MODULE
 */

GckModule*          _gck_module_new_initialized            (CK_FUNCTION_LIST_PTR funcs);

GckModuleInfo*      _gck_module_info_from_pkcs11            (CK_INFO_PTR info);

void                _gck_module_info_to_pkcs11              (GckModuleInfo* module_info,
                                                             CK_INFO_PTR info);

gboolean            _gck_module_info_match                  (GckModuleInfo *match,
                                                             GckModuleInfo *module_info);

/* -----------------------------------------------------------------------------
 * ENUMERATOR
 */

GckEnumerator *     _gck_enumerator_new_for_modules         (GList *modules,
                                                             GckSessionOptions session_options,
                                                             GckUriData *uri_data);

GckEnumerator *     _gck_enumerator_new_for_slots           (GList *slots,
                                                             GckSessionOptions session_options,
                                                             GckUriData *uri_data);

GckEnumerator *     _gck_enumerator_new_for_session         (GckSession *session,
                                                             GckUriData *uri_data);

/* ----------------------------------------------------------------------------
 * SLOT
 */

GckTokenInfo*      _gck_token_info_from_pkcs11              (CK_TOKEN_INFO_PTR info);

void               _gck_token_info_to_pkcs11                (GckTokenInfo *token_info,
                                                             CK_TOKEN_INFO_PTR info);

gboolean           _gck_token_info_match                    (GckTokenInfo *match,
                                                             GckTokenInfo *info);

CK_RV              _gck_session_authenticate_token          (CK_FUNCTION_LIST_PTR funcs,
                                                             CK_SESSION_HANDLE session,
                                                             GckSlot *token,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable);

CK_RV              _gck_session_authenticate_key            (CK_FUNCTION_LIST_PTR funcs,
                                                             CK_SESSION_HANDLE session,
                                                             GckObject *key,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable);

/* ----------------------------------------------------------------------------
 * PASSWORD
 */

void               _gck_password_update                     (GckPassword *self,
                                                             gboolean request_retry);

/* ----------------------------------------------------------------------------
 * CALL
 */

typedef CK_RV (*GckPerformFunc) (gpointer call_data);
typedef gboolean (*GckCompleteFunc) (gpointer call_data, CK_RV result);

typedef struct _GckCall GckCall;

typedef struct _GckArguments {
	/* For the call function to use */
	CK_FUNCTION_LIST_PTR pkcs11;
	CK_ULONG handle;

} GckArguments;

#define GCK_MECHANISM_EMPTY        { 0UL, NULL, 0 }

#define GCK_ARGUMENTS_INIT 	   { NULL, 0 }

G_DECLARE_FINAL_TYPE (GckCall, _gck_call, GCK, CALL, GObject)

#define GCK_TYPE_CALL             (_gck_call_get_type())

#define            _gck_call_arguments(call, type) (type*) \
   (_gck_call_get_arguments (GCK_CALL (call)))

#define            _gck_call_async_result_arguments(task, type) \
   (type*)(_gck_call_get_arguments (g_task_get_task_data (G_TASK (task))))

gpointer           _gck_call_get_arguments               (GckCall *call);

void               _gck_call_uninitialize                (void);

gboolean           _gck_call_sync                        (gpointer object,
                                                           gpointer perform,
                                                           gpointer complete,
                                                           gpointer args,
                                                           GCancellable *cancellable,
                                                           GError **err);

GckCall*           _gck_call_async_prep                  (gpointer object,
                                                           gpointer perform,
                                                           gpointer complete,
                                                           gsize args_size,
                                                           gpointer destroy_func);

GckCall*           _gck_call_async_ready                 (GckCall* call,
                                                           gpointer cb_object,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

void               _gck_call_async_go                    (GckCall *call);

void               _gck_call_async_ready_go              (GckCall *call,
                                                           gpointer cb_object,
                                                           GCancellable *cancellable,
                                                           GAsyncReadyCallback callback,
                                                           gpointer user_data);

void               _gck_call_async_short                 (GckCall *call,
                                                           CK_RV rv);

gboolean           _gck_call_basic_finish                (GAsyncResult *result,
                                                           GError **err);

void               _gck_call_async_object                (GckCall *call,
                                                           gpointer object);

#endif /* GCK_PRIVATE_H_ */
