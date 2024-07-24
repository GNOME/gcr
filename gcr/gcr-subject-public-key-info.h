/*
 * gcr
 *
 * Copyright (C) Niels De Graef
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
 * Author: Niels De Graef <nielsdegraef@gmail.com>
 */

#ifndef GCR_SUBJECT_PUBLIC_KEY_INFO_H
#define GCR_SUBJECT_PUBLIC_KEY_INFO_H

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GcrSubjectPublicKeyInfo GcrSubjectPublicKeyInfo;

#define GCR_TYPE_SUBJECT_PUBLIC_KEY_INFO (gcr_subject_public_key_info_get_type ())
GType gcr_subject_public_key_info_get_type (void) G_GNUC_CONST;


GBytes *        gcr_subject_public_key_info_get_key                       (GcrSubjectPublicKeyInfo *self);

const char *    gcr_subject_public_key_info_get_algorithm_oid             (GcrSubjectPublicKeyInfo *self);

const char *    gcr_subject_public_key_info_get_algorithm_description     (GcrSubjectPublicKeyInfo *self);

GBytes *        gcr_subject_public_key_info_get_algorithm_parameters_raw  (GcrSubjectPublicKeyInfo *self);

unsigned int    gcr_subject_public_key_info_get_key_size                  (GcrSubjectPublicKeyInfo *self);

GcrSubjectPublicKeyInfo *
                gcr_subject_public_key_info_copy                          (GcrSubjectPublicKeyInfo *self);

void            gcr_subject_public_key_info_free                          (GcrSubjectPublicKeyInfo *self);

G_END_DECLS

#endif /* GCR_SUBJECT_PUBLIC_KEY_INFO_H */
