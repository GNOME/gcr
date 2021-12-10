/*
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

#ifndef __GCR_KEY_MECHANISMS_H__
#define __GCR_KEY_MECHANISMS_H__

#include <glib-object.h>

#include "gck/gck.h"

G_BEGIN_DECLS

gulong                 _gcr_key_mechanisms_check              (GckObject *key,
                                                               const gulong *mechanisms,
                                                               gsize n_mechanisms,
                                                               gulong action_attr_type,
                                                               GCancellable *cancellable,
                                                               GError **error);

void                   _gcr_key_mechanisms_check_async        (GckObject *key,
                                                               const gulong *mechanisms,
                                                               gsize n_mechanisms,
                                                               gulong action_attr_type,
                                                               GCancellable *cancellable,
                                                               GAsyncReadyCallback callback,
                                                               gpointer user_data);

gulong                 _gcr_key_mechanisms_check_finish       (GckObject *key,
                                                               GAsyncResult *result,
                                                               GError **error);

G_END_DECLS

#endif /* __GCR_KEY_MECHANISMS_H__ */
