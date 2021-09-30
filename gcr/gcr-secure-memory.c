/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-secure-memory.c - library for allocating memory that is non-pageable

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

#include "config.h"

#include "gcr-secure-memory.h"

#include "egg/egg-secure-memory.h"

#include <glib.h>

#include <string.h>

/**
 * gcr_secure_memory_new: (skip)
 * @type: C type of the objects to allocate
 * @n_objects: number of objects to allocate
 *
 * Allocate objects in non-pageable memory.
 *
 * Returns: (transfer full): the new block of memory
 **/

/**
 * gcr_secure_memory_alloc: (skip)
 * @size: The new desired size of the memory block.
 *
 * Allocate a block of non-pageable memory.
 *
 * If non-pageable memory cannot be allocated then normal memory will be
 * returned.
 *
 * Return value: (transfer full): new memory block which should be freed
 * with gcr_secure_memory_free()
 **/
gpointer
gcr_secure_memory_alloc (gsize size)
{
	gpointer memory;

	/* Try to allocate secure memory */
	memory = egg_secure_alloc_full ("gcr-secure-memory", size,
	                                EGG_SECURE_USE_FALLBACK);

	/* Our fallback will always allocate */
	g_assert (memory != NULL);

	return memory;
}

/**
 * gcr_secure_memory_try_alloc: (skip)
 * @size: new desired size of the memory block
 *
 * Allocate a block of non-pageable memory.
 *
 * If non-pageable memory cannot be allocated, then %NULL is returned.
 *
 * Return value: (transfer full): new block, or %NULL if memory cannot be
 * allocated; memory block should be freed with gcr_secure_memory_free()
 */
gpointer
gcr_secure_memory_try_alloc (gsize size)
{
	return egg_secure_alloc_full ("gcr-secure-memory", size, 0);
}

/**
 * gcr_secure_memory_realloc: (skip)
 * @memory: (nullable): pointer to reallocate or %NULL to allocate a new block
 * @size: new desired size of the memory block, or 0 to free the memory
 *
 * Reallocate a block of non-pageable memory.
 *
 * Glib memory is also reallocated correctly. If called with a null pointer,
 * then a new block of memory is allocated. If called with a zero size,
 * then the block of memory is freed.
 *
 * If non-pageable memory cannot be allocated then normal memory will be
 * returned.
 *
 * Return value: (transfer full): new block, or %NULL if the block was
 * freed; memory block should be freed with gcr_secure_memory_free()
 */
gpointer
gcr_secure_memory_realloc (gpointer memory,
                           gsize size)
{
	gpointer new_memory;

	if (!memory) {
		return gcr_secure_memory_alloc (size);
	} else if (!size) {
		 gcr_secure_memory_free (memory);
		 return NULL;
	} else if (!egg_secure_check (memory)) {
		return g_realloc (memory, size);
	}

	/* First try and ask secure memory to reallocate */
	new_memory = egg_secure_realloc_full ("gcr-secure-memory", memory,
	                                      size, EGG_SECURE_USE_FALLBACK);

	g_assert (new_memory != NULL);

	return new_memory;
}

/**
 * gcr_secure_memory_try_realloc: (skip)
 * @memory: (nullable): pointer to reallocate or %NULL to allocate a new block
 * @size: new desired size of the memory block
 *
 * Reallocate a block of non-pageable memory.
 *
 * Glib memory is also reallocated correctly when passed to this function.
 * If called with a null pointer, then a new block of memory is allocated.
 * If called with a zero size, then the block of memory is freed.
 *
 * If memory cannot be allocated, %NULL is returned and the original block
 * of memory remains intact.
 *
 * Return value: (transfer full): the new block, or %NULL if memory cannot be
 * allocated; the memory block should be freed with gcr_secure_memory_free()
 */
gpointer
gcr_secure_memory_try_realloc (gpointer memory,
                               gsize size)
{
	gpointer new_memory;

	if (!memory) {
		return gcr_secure_memory_try_alloc (size);
	} else if (!size) {
		 gcr_secure_memory_free (memory);
		 return NULL;
	} else if (!egg_secure_check (memory)) {
		return g_try_realloc (memory, size);
	}

	/* First try and ask secure memory to reallocate */
	new_memory = egg_secure_realloc_full ("gcr-secure-memory", memory,
	                                      size, 0);

	/* Might be NULL if reallocation failed. */
	return new_memory;
}

/**
 * gcr_secure_memory_free: (skip)
 * @memory: (nullable): pointer to the beginning of the block of memory to free
 *
 * Free a block of non-pageable memory.
 *
 * Glib memory is also freed correctly when passed to this function. If called
 * with a %NULL pointer then no action is taken.
 */
void
gcr_secure_memory_free (gpointer memory)
{
	if (!memory)
		return;
	egg_secure_free_full (memory, EGG_SECURE_USE_FALLBACK);
}

/**
 * gcr_secure_memory_is_secure: (skip)
 * @memory: pointer to check
 *
 * Check if a pointer is in non-pageable memory allocated by.
 *
 * Returns: whether the memory is secure non-pageable memory allocated by the
 *          Gcr library or not
 */
gboolean
gcr_secure_memory_is_secure (gpointer memory)
{
	return egg_secure_check (memory) ? TRUE : FALSE;
}

/**
 * gcr_secure_memory_strdup: (skip)
 * @string: (nullable): null terminated string to copy
 *
 * Copy a string into non-pageable memory. If the input string is %NULL, then
 * %NULL will be returned.
 *
 * Returns: copied string, should be freed with gcr_secure_memory_free()
 */
gchar *
gcr_secure_memory_strdup (const gchar* string)
{
	return egg_secure_strdup_full ("gcr-secure-memory", string,
	                               EGG_SECURE_USE_FALLBACK);
}

/**
 * gcr_secure_memory_strfree: (skip)
 * @string: (nullable): null terminated string to fere
 *
 * Free a string, whether securely allocated using these functions or not.
 * This will also clear out the contents of the string so they do not
 * remain in memory.
 */
void
gcr_secure_memory_strfree (gchar *string)
{
	egg_secure_strfree (string);
}
