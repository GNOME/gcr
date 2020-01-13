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

#ifndef __GCR_SINGLE_COLLECTION_H__
#define __GCR_SINGLE_COLLECTION_H__

#include "gcr-collection.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_SINGLE_COLLECTION (_gcr_single_collection_get_type ())
G_DECLARE_FINAL_TYPE (GcrSingleCollection, _gcr_single_collection,
                      GCR, SINGLE_COLLECTION,
                      GObject)

GcrCollection *     _gcr_single_collection_new                     (GObject *object);

GObject *           _gcr_single_collection_get_object              (GcrSingleCollection *self);

void                _gcr_single_collection_set_object              (GcrSingleCollection *self,
                                                                   GObject *object);

G_END_DECLS

#endif /* __GCR_SINGLE_COLLECTION_H__ */
