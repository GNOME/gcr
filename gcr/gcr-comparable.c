/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Stefan Walter
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
 */

#include "config.h"

#include "gcr-comparable.h"

#include <string.h>

/**
 * SECTION:gcr-comparable
 * @title: GcrComparable
 * @short_description: Interface for comparing objects
 *
 * The #GcrComparable interface is implemented by objects when they should be
 * comparable against one another.
 */

/**
 * GcrComparable:
 *
 * The #GcrComparable interface is implemented by comparable objects.
 */

/**
 * GcrComparableIface:
 * @parent: type interface
 * @compare: Compare whether tow objects represent the same thing.
 *
 * The interface to implement for #GcrComparable
 */

typedef GcrComparableIface GcrComparableInterface;
G_DEFINE_INTERFACE (GcrComparable, gcr_comparable, G_TYPE_OBJECT);

static void
gcr_comparable_default_init (GcrComparableIface *iface)
{

}


/**
 * gcr_comparable_compare:
 * @self: The comparable object
 * @other: (nullable): Another comparable object
 *
 * Compare whether two objects represent the same thing. The return value can
 * also be used to sort the objects.
 *
 * Returns: Zero if the two objects represent the same thing, non-zero if not.
 */
gint
gcr_comparable_compare (GcrComparable *self, GcrComparable *other)
{
	g_return_val_if_fail (GCR_IS_COMPARABLE (self), -1);
	g_return_val_if_fail (GCR_COMPARABLE_GET_INTERFACE (self)->compare, -1);
	g_return_val_if_fail (G_IS_OBJECT (self), -1);
	return GCR_COMPARABLE_GET_INTERFACE (self)->compare (self, other);
}

/**
 * gcr_comparable_memcmp: (skip)
 * @mem1: (array length=size1) (element-type guint8): First block of memory
 * @size1: Length of first block
 * @mem2: (array length=size2) (element-type guint8): Second block of memory
 * @size2: Length of second block
 *
 * Compare two blocks of memory. The return value can be used to sort
 * the blocks of memory.
 *
 * Returns: Zero if the blocks are identical, negative if first
 *          less than secend, possitive otherwise.
 */
gint
gcr_comparable_memcmp (gconstpointer mem1,
                       gsize size1,
                       gconstpointer mem2,
                       gsize size2)
{
	gint result;

	if (mem1 == mem2 && size1 == size2)
		return 0;

	if (!mem1)
		return 1;
	if (!mem2)
		return -1;

	result = memcmp (mem1, mem2, MIN (size1, size2));
	if (result != 0)
		return result;

	if (size1 == size2)
		return 0;
	if (size1 < size2)
		return -1;
	return 1;
}
