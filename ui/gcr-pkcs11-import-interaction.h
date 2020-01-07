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

#ifndef __GCR_PKCS11_IMPORT_INTERACTION_H__
#define __GCR_PKCS11_IMPORT_INTERACTION_H__

#include "gcr-pkcs11-import-dialog.h"

G_BEGIN_DECLS

#define GCR_TYPE_PKCS11_IMPORT_INTERACTION (_gcr_pkcs11_import_interaction_get_type ())
G_DECLARE_FINAL_TYPE (GcrPkcs11ImportInteraction, _gcr_pkcs11_import_interaction,
                      GCR, PKCS11_IMPORT_INTERACTION,
                      GTlsInteraction)

GTlsInteraction *   _gcr_pkcs11_import_interaction_new          (GtkWindow *parent_window);

G_END_DECLS

#endif /* __GCR_PKCS11_IMPORT_INTERACTION_H__ */
