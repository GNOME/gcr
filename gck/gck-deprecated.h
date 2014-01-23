/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-deprecated.h - the GObject PKCS#11 wrapper library

   Copyright (C) 2011 Stefan Walter

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

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#ifndef GCK_DEPRECATED_H
#define GCK_DEPRECATED_H

#include "gck.h"

G_BEGIN_DECLS

#ifndef GCK_DISABLE_DEPRECATED

typedef             GArray                                  GckMechanisms;

#define             gck_mechanisms_free(a)                  (g_array_free (a, TRUE))

#define             CKR_GCK_MODULE_PROBLEM                  GCK_ERROR_MODULE_PROBLEM

GQuark              gck_get_error_quark                     (void);

GQuark              gck_uri_get_error_quark                 (void);

#define             GCK_URI_BAD_PREFIX                      GCK_URI_BAD_SCHEME

GType               gck_attributes_get_boxed_type           (void) G_GNUC_CONST;

GckAttributes *     gck_attributes_new_full                 (GckAllocator allocator);

GckAttribute *      gck_attributes_add                      (GckAttributes *attrs,
                                                             GckAttribute *attr);

void                gck_attributes_add_all                  (GckAttributes *attrs,
                                                             GckAttributes *from);

GckAttribute *      gck_attributes_add_data                 (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

GckAttribute *      gck_attributes_add_invalid              (GckAttributes *attrs,
                                                             gulong attr_type);

GckAttribute *      gck_attributes_add_empty                (GckAttributes *attrs,
                                                             gulong attr_type);

GckAttribute*       gck_attributes_add_boolean              (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gboolean value);

GckAttribute*       gck_attributes_add_string               (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             const gchar *value);

GckAttribute*       gck_attributes_add_date                 (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             const GDate *value);

GckAttribute*       gck_attributes_add_ulong                (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gulong value);

void                gck_attributes_set                      (GckAttributes *attrs,
                                                             GckAttribute *attr);

void                gck_attributes_set_boolean              (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gboolean value);

void                gck_attributes_set_ulong                (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gulong value);

void                gck_attributes_set_string               (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             const gchar *value);

void                gck_attributes_set_date                 (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             const GDate *value);

void                gck_attributes_set_all                  (GckAttributes *attrs,
                                                             GckAttributes *from);

GckAttributes *     gck_attributes_dup                      (GckAttributes *attrs);

#endif /* GCK_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* GCK_H */
