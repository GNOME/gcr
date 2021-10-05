/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_MODULE_H__
#define __GCK_MODULE_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include <gck/gck-object.h>
#include <gck/gck-slot.h>
#include <gck/gck-attributes.h>
#include <gck/gck-enumerator.h>

G_BEGIN_DECLS

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
#define GCK_MODULE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GCK_TYPE_MODULE, GckModule))
#define GCK_MODULE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GCK_TYPE_MODULE, GckModule))
#define GCK_IS_MODULE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GCK_TYPE_MODULE))
#define GCK_IS_MODULE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GCK_TYPE_MODULE))
#define GCK_MODULE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GCK_TYPE_MODULE, GckModuleClass))

typedef struct _GckModuleClass GckModuleClass;
typedef struct _GckModulePrivate GckModulePrivate;

struct _GckModule {
	GObject parent;

	/*< private >*/
	GckModulePrivate *pv;
	gpointer reserved[4];
};

struct _GckModuleClass {
	GObjectClass parent;

	/*< private >*/
	gpointer reserved[8];
};

GType                 gck_module_get_type                     (void) G_GNUC_CONST;

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

gboolean              gck_module_equal                        (gconstpointer module1,
                                                               gconstpointer module2);

guint                 gck_module_hash                         (gconstpointer module);

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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckModule, g_object_unref);

G_END_DECLS

#endif /* __GCK_MODULE_H__ */
