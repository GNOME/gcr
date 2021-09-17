/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_URI_H__
#define __GCK_URI_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>

#include <gck/gck-slot.h>
#include <gck/gck-module.h>
#include <gck/gck-attributes.h>

G_BEGIN_DECLS

typedef enum {
	GCK_URI_BAD_SCHEME = 1,
	GCK_URI_BAD_ENCODING,
	GCK_URI_BAD_SYNTAX,
	GCK_URI_BAD_VERSION,
	GCK_URI_NOT_FOUND
} GckUriError;

/* WARNING: Don't modify these without syncing with p11-kit */
typedef enum {
	GCK_URI_FOR_OBJECT =  (1 << 1),
	GCK_URI_FOR_TOKEN =   (1 << 2),
	GCK_URI_FOR_MODULE =  (1 << 3),
	GCK_URI_WITH_VERSION = (1 << 4),
	GCK_URI_FOR_ANY =     0x0000FFFF,
} GckUriFlags;

#define             GCK_URI_FOR_MODULE_WITH_VERSION         (GCK_URI_WITH_VERSION | GCK_URI_FOR_MODULE)

#define             GCK_URI_FOR_OBJECT_ON_TOKEN             (GCK_URI_FOR_OBJECT | GCK_URI_FOR_TOKEN)

#define             GCK_URI_FOR_OBJECT_ON_TOKEN_AND_MODULE  (GCK_URI_FOR_OBJECT_ON_TOKEN | GCK_URI_FOR_MODULE)

struct _GckUriData {
	gboolean any_unrecognized;
	GckModuleInfo *module_info;
	GckTokenInfo *token_info;
	GckAttributes *attributes;

	/*< private >*/
	gpointer dummy[4];
};

#define             GCK_URI_ERROR                           (gck_uri_error_get_quark ())

GQuark              gck_uri_error_get_quark                 (void) G_GNUC_CONST;

GckUriData*         gck_uri_data_new                        (void);

gchar*              gck_uri_build                           (GckUriData *uri_data,
                                                             GckUriFlags flags);

GckUriData*         gck_uri_parse                           (const gchar *string,
                                                             GckUriFlags flags,
                                                             GError **error);

#define             GCK_URI_DATA_TYPE                       (gck_uri_data_get_type ())

GType               gck_uri_data_get_type                   (void) G_GNUC_CONST;

GckUriData *        gck_uri_data_copy                       (GckUriData *uri_data);

void                gck_uri_data_free                       (GckUriData *uri_data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckUriData, gck_uri_data_free);

G_END_DECLS

#endif /* __GCK_URI_H__ */
