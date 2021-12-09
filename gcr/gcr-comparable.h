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

#ifndef __GCR_COMPARABLE_H__
#define __GCR_COMPARABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_COMPARABLE gcr_comparable_get_type ()
G_DECLARE_INTERFACE (GcrComparable, gcr_comparable, GCR, COMPARABLE, GObject)

struct _GcrComparableInterface {
	GTypeInterface parent;
	gint (*compare) (GcrComparable *self, GcrComparable *other);
};

gint                gcr_comparable_compare                (GcrComparable *self,
                                                           GcrComparable *other);

gint                gcr_comparable_memcmp                 (gconstpointer mem1,
                                                           gsize size1,
                                                           gconstpointer mem2,
                                                           gsize size2);

G_END_DECLS

#endif /* __GCR_COMPARABLE_H__ */
