/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-viewer-widget.h: Widget for viewer

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
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#ifndef GCR_VIEWER_WIDGET_H
#define GCR_VIEWER_WIDGET_H

#include <gtk/gtk.h>

#define GCR_TYPE_VIEWER_WIDGET               (gcr_viewer_widget_get_type ())
#define GCR_VIEWER_WIDGET(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_VIEWER_WIDGET, GcrViewerWidget))
#define GCR_IS_VIEWER_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_VIEWER_WIDGET))

typedef struct _GcrViewerWidget GcrViewerWidget;


GType              gcr_viewer_widget_get_type         (void);

GcrViewerWidget *  gcr_viewer_widget_new              (void);

void               gcr_viewer_widget_load_file        (GcrViewerWidget *self,
                                                       GFile *file);

void               gcr_viewer_widget_load_bytes       (GcrViewerWidget *self,
                                                       const gchar *display_name,
                                                       GBytes *data);

void               gcr_viewer_widget_load_data        (GcrViewerWidget *self,
                                                       const gchar *display_name,
                                                       const guchar *data,
                                                       gsize n_data);

GcrViewer *        gcr_viewer_widget_get_viewer       (GcrViewerWidget *self);

GcrParser *        gcr_viewer_widget_get_parser       (GcrViewerWidget *self);

void               gcr_viewer_widget_show_error       (GcrViewerWidget *self,
                                                       const gchar *message,
                                                       GError *error);

void               gcr_viewer_widget_clear_error      (GcrViewerWidget *self);

const gchar *      gcr_viewer_widget_get_display_name (GcrViewerWidget *self);

void               gcr_viewer_widget_set_display_name (GcrViewerWidget *self,
                                                       const gchar *display_name);

#endif /* GCR_VIEWER_WIDGET_H */
