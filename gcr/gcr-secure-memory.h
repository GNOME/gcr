/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-secure-memory.h - library for allocating memory that is non-pageable

   Copyright (C) 2007 Stefan Walter
   Copyright (C) 2012 Red Hat Inc.

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

   Author: Stef Walter <stefw@gnome.org>
*/

#ifndef GCR_SECURE_MEMORY_H
#define GCR_SECURE_MEMORY_H

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

#define   gcr_secure_memory_new(type, n_objects) \
          ((type *)(gcr_secure_memory_alloc (sizeof (type) * (n_objects))))

gpointer  gcr_secure_memory_alloc          (gsize size);

gpointer  gcr_secure_memory_try_alloc      (gsize size);

gpointer  gcr_secure_memory_realloc        (gpointer memory,
                                            gsize size);

gpointer  gcr_secure_memory_try_realloc    (gpointer memory,
                                            gsize size);

void      gcr_secure_memory_free           (gpointer memory);

gboolean  gcr_secure_memory_is_secure      (gpointer memory);

gchar *   gcr_secure_memory_strdup         (const gchar *string);

void      gcr_secure_memory_strfree        (gchar *string);

G_END_DECLS

#endif /* GCR_SECURE_MEMORY_H */
