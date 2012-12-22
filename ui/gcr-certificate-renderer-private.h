/*
 * Copyright (C) 2010 Stefan Walter
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_CERTIFICATE_RENDERER_PRIVATE_H__
#define __GCR_CERTIFICATE_RENDERER_PRIVATE_H__

#include "gcr-display-view.h"
#include "gcr-renderer.h"

G_BEGIN_DECLS

void      _gcr_certificate_renderer_append_distinguished_name        (GcrRenderer *renderer,
                                                                      GcrDisplayView *view,
                                                                      GNode *dn);

void      _gcr_certificate_renderer_append_subject_public_key        (GcrRenderer *renderer,
                                                                      GcrDisplayView *view,
                                                                      guint key_size,
                                                                      GNode *subject_public_key);

void      _gcr_certificate_renderer_append_signature                 (GcrRenderer *renderer,
                                                                      GcrDisplayView *view,
                                                                      GNode *asn);

void      _gcr_certificate_renderer_append_extension                 (GcrRenderer *renderer,
                                                                      GcrDisplayView *view,
                                                                      GNode *asn);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_RENDERER_PRIVATE_H__ */
