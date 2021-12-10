/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
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

#ifndef __GCR_PKCS11_IMPORTER_H__
#define __GCR_PKCS11_IMPORTER_H__

#include "gcr-importer.h"

G_BEGIN_DECLS

#define GCR_TYPE_PKCS11_IMPORTER (_gcr_pkcs11_importer_get_type ())
G_DECLARE_FINAL_TYPE (GcrPkcs11Importer, _gcr_pkcs11_importer,
                      GCR, PKCS11_IMPORTER,
                      GObject);

GcrImporter *             _gcr_pkcs11_importer_new             (GckSlot *slot);

void                      _gcr_pkcs11_importer_queue           (GcrPkcs11Importer *self,
                                                                const gchar *label,
                                                                GckAttributes *attrs);

GckSlot *                 _gcr_pkcs11_importer_get_slot        (GcrPkcs11Importer *self);

GList *                   _gcr_pkcs11_importer_get_queued      (GcrPkcs11Importer *self);

GList *                   _gcr_pkcs11_importer_get_imported    (GcrPkcs11Importer *self);

G_END_DECLS

#endif /* __GCR_PKCS11_IMPORTER_H__ */
