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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef GCR_BASE_DEPRECATED_H_
#define GCR_BASE_DEPRECATED_H_
#ifndef GCR_DISABLE_DEPRECATED
#ifndef __GI_SCANNER__

#include <glib.h>

#include "gcr-importer.h"
#include "gcr-parser.h"
#include "gcr-simple-collection.h"

G_BEGIN_DECLS

#define           GCR_ERROR                                   (gcr_error_get_domain ())

G_DEPRECATED
GQuark            gcr_error_get_domain                        (void) G_GNUC_CONST;

G_DEPRECATED_FOR(gcr_collection_contains)
gboolean          gcr_simple_collection_contains              (GcrSimpleCollection *self,
                                                               GObject *object);

G_DEPRECATED_FOR(gcr_importer_listen)
GcrParser *       gcr_importer_get_parser                     (GcrImporter *self);

G_DEPRECATED_FOR(gcr_importer_listen)
void              gcr_importer_set_parser                     (GcrImporter *self,
                                                               GcrParser *parser);

G_DEPRECATED
GckSlot *         gcr_importer_get_slot                       (GcrImporter *self);

G_DEPRECATED
void              gcr_importer_set_slot                       (GcrImporter *self,
                                                               GckSlot *slot);

typedef enum {
	GCR_IMPORTER_PROMPT_NEEDED,
	GCR_IMPORTER_PROMPT_ALWAYS,
	GCR_IMPORTER_PROMPT_NEVER
} GcrImporterPromptBehavior;

G_DEPRECATED
GcrImporterPromptBehavior  gcr_importer_get_prompt_behavior   (GcrImporter *self);

G_DEPRECATED
void                       gcr_importer_set_prompt_behavior   (GcrImporter *self,
                                                               GcrImporterPromptBehavior behavior);

G_END_DECLS

#endif /* __GI_SCANNER__ */
#endif /* GCR_DISABLE_DEPRECATED */
#endif /* GCRTYPES_H_ */
