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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef GCR_DEPRECATED_H_
#define GCR_DEPRECATED_H_
#ifndef GCR_DISABLE_DEPRECATED
#ifndef __GI_SCANNER__

#include <glib.h>

#include "gcr-certificate-basics-widget.h"
#include "gcr-certificate-details-widget.h"
#include "gcr-certificate-renderer.h"
#include "gcr-viewer.h"

G_BEGIN_DECLS

void              gcr_renderer_render                         (GcrRenderer *self,
                                                               GcrViewer *viewer);

GckAttributes *   gcr_certificate_renderer_get_attributes     (GcrCertificateRenderer *self);

void              gcr_certificate_renderer_set_attributes     (GcrCertificateRenderer *self,
                                                               GckAttributes *attrs);

GckAttributes *   gcr_certificate_widget_get_attributes       (GcrCertificateWidget *self);

void              gcr_certificate_widget_set_attributes       (GcrCertificateWidget *self,
                                                               GckAttributes *attrs);

G_END_DECLS

#endif /* __GI_SCANNER__ */
#endif /* GCR_DISABLE_DEPRECATED */
#endif /* GCRTYPES_H_ */
