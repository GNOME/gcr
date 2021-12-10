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

#ifndef __GCR_OPENPGP_H__
#define __GCR_OPENPGP_H__

#include <glib.h>

#include <gck/gck.h>

typedef enum {
	GCR_OPENPGP_ALGO_RSA = 1,
	GCR_OPENPGP_ALGO_RSA_E = 2,
	GCR_OPENPGP_ALGO_RSA_S = 3,
	GCR_OPENPGP_ALGO_ELG_E = 16,
	GCR_OPENPGP_ALGO_DSA = 17
} GcrOpenpgpAlgo;

typedef enum {
	GCR_OPENPGP_PARSE_NONE = 0,
	GCR_OPENPGP_PARSE_KEYS = 1 << 1,
	GCR_OPENPGP_PARSE_NO_RECORDS = 1 << 2,
	GCR_OPENPGP_PARSE_SIGNATURES = 1 << 3,
	GCR_OPENPGP_PARSE_ATTRIBUTES = 1 << 4,
} GcrOpenpgpParseFlags;

G_BEGIN_DECLS

typedef void             (*GcrOpenpgpCallback)             (GPtrArray *records,
                                                            GBytes *outer,
                                                            gpointer user_data);

guint                    _gcr_openpgp_parse                (GBytes *data,
                                                            GcrOpenpgpParseFlags flags,
                                                            GcrOpenpgpCallback callback,
                                                            gpointer user_data);

G_END_DECLS

#endif /* __GCR_OPENPGP_H__ */
