/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GCR_SECTION_H__
#define __GCR_SECTION_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCR_TYPE_SECTION gcr_section_get_type()
G_DECLARE_FINAL_TYPE (GcrSection, gcr_section, GCR, SECTION, GtkGrid)

GtkWidget *gcr_section_new (const gchar *title);
GtkWidget *gcr_section_new_with_icon (const gchar *title,
                                      GIcon       *icon);

void gcr_section_add_child (GcrSection  *self,
                            const gchar *description,
                            GtkWidget   *child);

G_END_DECLS

#endif /* __GCR_SECTION_H__ */
