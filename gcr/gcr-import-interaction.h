/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __GCR_IMPORT_INTERACTION_H__
#define __GCR_IMPORT_INTERACTION_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-importer.h"

#include <glib-object.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define GCR_TYPE_IMPORT_INTERACTION gcr_import_interaction_get_type ()
G_DECLARE_INTERFACE(GcrImportInteraction, gcr_import_interaction, GCR, IMPORT_INTERACTION, GTlsInteraction)

struct _GcrImportInteractionInterface {
	GTypeInterface parent;

	void                    (*supplement_prep)   (GcrImportInteraction *interaction,
	                                              GckBuilder *builder);

	GTlsInteractionResult   (*supplement)        (GcrImportInteraction *interaction,
	                                              GckBuilder *builder,
	                                              GCancellable *cancellable,
	                                              GError **error);

	void                    (*supplement_async)  (GcrImportInteraction *interaction,
	                                              GckBuilder *builder,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	GTlsInteractionResult   (*supplement_finish) (GcrImportInteraction *interaction,
	                                              GAsyncResult *result,
	                                              GError **error);

	/*< private >*/
	gpointer reserved[6];
};

void                   gcr_import_interaction_supplement_prep      (GcrImportInteraction *interaction,
                                                                    GckBuilder *builder);

GTlsInteractionResult  gcr_import_interaction_supplement           (GcrImportInteraction *interaction,
                                                                    GckBuilder *builder,
                                                                    GCancellable *cancellable,
                                                                    GError **error);

void                   gcr_import_interaction_supplement_async     (GcrImportInteraction *interaction,
                                                                    GckBuilder *builder,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer user_data);

GTlsInteractionResult  gcr_import_interaction_supplement_finish    (GcrImportInteraction *interaction,
                                                                    GAsyncResult *result,
                                                                    GError **error);

G_END_DECLS

#endif /* __GCR_IMPORT_INTERACTION_H__ */
