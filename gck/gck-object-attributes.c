/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-object-attributes.c - the GObject PKCS#11 wrapper library

   Copyright (C) 2011 Collabora Ltd.

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gck.h"
#include "gck-private.h"

#include <string.h>

/**
 * SECTION:gck-object-attributes
 * @title: GckObjectAttributes
 * @short_description: An interface which holds attributes for a PKCS\#11 object
 *
 * #GckObjectAttributes is an interface implemented by derived classes of
 * #GckObject to indicate which attributes they'd like an enumerator to retrieve.
 * These attributes are then cached on the object and can be retrieved through
 * the #GckObjectAttributes:attributes property.
 */

/**
 * GckObjectAttributesIface:
 * @interface: parent interface
 * @attribute_types: (array length=n_attribute_types): attribute types that an
 *                   enumerator should retrieve
 * @n_attribute_types: number of attribute types to be retrieved
 *
 * Interface for #GckObjectAttributes. If the @attribute_types field is set by
 * a implementing class, then the a #GckEnumerator which has been setup using
 * gck_enumerator_set_object_type()
 */

typedef GckObjectAttributesIface GckObjectAttributesInterface;
G_DEFINE_INTERFACE (GckObjectAttributes, gck_object_attributes, GCK_TYPE_OBJECT);

static void
gck_object_attributes_default_init (GckObjectAttributesIface *iface)
{
	static volatile gsize initialized = 0;
	if (g_once_init_enter (&initialized)) {

		/**
		 * GckObjectAttributes:attributes:
		 *
		 * The attributes cached on this object.
		 */
		g_object_interface_install_property (iface,
		         g_param_spec_boxed ("attributes", "Attributes", "PKCS#11 Attributes",
		                             GCK_TYPE_ATTRIBUTES, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

		g_once_init_leave (&initialized, 1);
	}
}

/**
 * gck_object_attributes_get_attributes:
 * @object: an object with attributes
 *
 * Gets the attributes cached on this object.
 *
 * Returns: (transfer full) (allow-none): the attributes
 */
GckAttributes *
gck_object_attributes_get_attributes (GckObjectAttributes *object)
{
	GckAttributes *attributes = NULL;
	g_return_val_if_fail (GCK_IS_OBJECT_ATTRIBUTES (object), NULL);
	g_object_get (object, "attributes", &attributes, NULL);
	return attributes;
}

/**
 * gck_object_attributes_set_attributes:
 * @object: an object with attributes
 * @attributes: (allow-none): the attributes to cache
 *
 * Sets the attributes cached on this object.
 */
void
gck_object_attributes_set_attributes (GckObjectAttributes *object,
                                      GckAttributes *attributes)
{
	g_return_if_fail (GCK_IS_OBJECT_ATTRIBUTES (object));
	g_object_set (object, "attributes", attributes, NULL);
}
