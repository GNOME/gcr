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

#include "config.h"

#include "gcr-column.h"

/**
 * GcrColumnFlags:
 * @GCR_COLUMN_NONE: No column flags
 * @GCR_COLUMN_HIDDEN: Don't display this column.
 * @GCR_COLUMN_SORTABLE: This column is sortable.
 *
 * Flags to be used with a [struct@Column].
 */

/**
 * GcrColumn:
 * @property_name: The name of the property this column will display
 * @property_type: The type of the property
 * @column_type: The eventual type of the column
 * @label: The display label for the column
 * @flags: Flags from #GcrColumnFlags
 * @transformer: A transformer function used to convert the value from
 *     the property type to the column type. Can be %NULL if the types
 *     are the same.
 * @user_data: User data associated with the column
 *
 * Represents a column to display in a `GcrCollectionModel` or
 * `GcrTreeSelector`.
 *
 * The label should be set as a translatable string with a context of
 * `"column"`. This should be done with with this macro:
 *
 * ```
 * NC_("column", "My Column Name")
 * ```
 */
