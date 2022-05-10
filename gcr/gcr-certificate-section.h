/*
 * Copyright 2022 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GCR_CERTIFICATE_SECTION_H__
#define __GCR_CERTIFICATE_SECTION_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <gcr/gcr-enum-types.h>

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_SECTION (gcr_certificate_section_get_type())

G_DECLARE_FINAL_TYPE (GcrCertificateSection, gcr_certificate_section, GCR, CERTIFICATE_SECTION, GObject)

typedef enum {
	GCR_CERTIFICATE_SECTION_NONE = 0,
	GCR_CERTIFICATE_SECTION_IMPORTANT = 1 << 0,
} GcrCertificateSectionFlags;

const char                 *gcr_certificate_section_get_label  (GcrCertificateSection *self);
GListModel                 *gcr_certificate_section_get_fields (GcrCertificateSection *self);
GcrCertificateSectionFlags  gcr_certificate_section_get_flags  (GcrCertificateSection *self);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_SECTION_H__ */
