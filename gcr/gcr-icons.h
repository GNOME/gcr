/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd
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

#ifndef __GCR_ICONS_H__
#define __GCR_ICONS_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <gck/gck.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define GCR_ICON_CERTIFICATE    "application-certificate"
#define GCR_ICON_PASSWORD       "gcr-password"
#define GCR_ICON_GNUPG          "gcr-gnupg"
#define GCR_ICON_KEY            "gcr-key"
#define GCR_ICON_KEY_PAIR       "gcr-key-pair"
#define GCR_ICON_SMART_CARD     "gcr-smart-card"
#define GCR_ICON_HOME_DIRECTORY "user-home"

GIcon *          gcr_icon_for_token                (GckTokenInfo *token_info);

G_END_DECLS

#endif /* __GCR_SMART_CARD_H__ */
