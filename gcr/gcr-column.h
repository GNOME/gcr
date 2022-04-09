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

#ifndef __GCR_COLUMN_H__
#define __GCR_COLUMN_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GCR_COLUMN_NONE = 0,
	GCR_COLUMN_HIDDEN = 1 << 1,
	GCR_COLUMN_SORTABLE = 1 << 2,
} GcrColumnFlags;

typedef struct _GcrColumn {
	const gchar *property_name;     /* The property to retrieve */
	GType property_type;            /* The property type */
	GType column_type;              /* The resulting property type for this column */

	const gchar *label;             /* The label for this column, or NULL */
	GcrColumnFlags flags;           /* Column flags */

	GValueTransform transformer;    /* The way to transform to this type or NULL */

	gpointer user_data;

	/*< private >*/
	gpointer reserved;
} GcrColumn;

G_END_DECLS

#endif /* __GCR_COLUMN_H__ */
