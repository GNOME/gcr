/* GTK - The GIMP Toolkit
 * Copyright (C) Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * GtkImageMenuItem was was deprecated from GTK+ and reasoning explained
 * here: https://docs.google.com/document/d/1KCVPoYQBqMbDP11tHPpjW6uaEHrvLUmcDPqKAppCY8o/pub
 * Gcr wants to use image menu items in button drop downs using the device icons
 * (for the import button). So just copy it here with the stock and activatable
 * sutff removed
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __EGG_IMAGE_MENUITEM_H__
#define __EGG_IMAGE_MENUITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_IMAGE_MENU_ITEM            (egg_image_menu_item_get_type ())
#define EGG_IMAGE_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_IMAGE_MENU_ITEM, EggImageMenuItem))
#define EGG_IMAGE_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_IMAGE_MENU_ITEM, EggImageMenuItemClass))
#define EGG_IS_IMAGE_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_IMAGE_MENU_ITEM))
#define EGG_IS_IMAGE_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_IMAGE_MENU_ITEM))
#define EGG_IMAGE_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_IMAGE_MENU_ITEM, EggImageMenuItemClass))


typedef struct _EggImageMenuItem              EggImageMenuItem;
typedef struct _EggImageMenuItemPrivate       EggImageMenuItemPrivate;
typedef struct _EggImageMenuItemClass         EggImageMenuItemClass;

struct _EggImageMenuItem
{
  GtkMenuItem menu_item;

  /*< private >*/
  EggImageMenuItemPrivate *priv;
};

struct _EggImageMenuItemClass
{
  GtkMenuItemClass parent_class;
};

GType	   egg_image_menu_item_get_type          (void) G_GNUC_CONST;
GtkWidget* egg_image_menu_item_new               (void);
GtkWidget* egg_image_menu_item_new_with_label    (const gchar      *label);
GtkWidget* egg_image_menu_item_new_with_mnemonic (const gchar      *label);
void       egg_image_menu_item_set_always_show_image (EggImageMenuItem *image_menu_item,
                                                      gboolean          always_show);
gboolean   egg_image_menu_item_get_always_show_image (EggImageMenuItem *image_menu_item);
void       egg_image_menu_item_set_image         (EggImageMenuItem *image_menu_item,
                                                  GtkWidget        *image);
GtkWidget* egg_image_menu_item_get_image         (EggImageMenuItem *image_menu_item);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EggImageMenuItem, g_object_unref)

G_END_DECLS

#endif /* __EGG_IMAGE_MENUITEM_H__ */
