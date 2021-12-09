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

#ifndef GCR_CALLBACK_OUTPUT_STREAM_H
#define GCR_CALLBACK_OUTPUT_STREAM_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCR_TYPE_CALLBACK_OUTPUT_STREAM (_gcr_callback_output_stream_get_type ())
G_DECLARE_FINAL_TYPE (GcrCallbackOutputStream, _gcr_callback_output_stream,
                      GCR, CALLBACK_OUTPUT_STREAM,
                      GOutputStream)

typedef gssize      (*GcrCallbackOutputFunc)               (gconstpointer buffer,
                                                            gsize count,
                                                            GCancellable *cancellable,
                                                            gpointer user_data,
                                                            GError **error);

GOutputStream *     _gcr_callback_output_stream_new        (GcrCallbackOutputFunc callback,
                                                            gpointer user_data,
                                                            GDestroyNotify destroy_func);

G_END_DECLS

#endif /* GCR_CALLBACK_OUTPUT_STREAM_H */
