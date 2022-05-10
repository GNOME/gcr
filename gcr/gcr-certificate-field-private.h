/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GCR_CERTIFICATE_FIELD_PRIVATE_H__
#define __GCR_CERTIFICATE_FIELD_PRIVATE_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include "gcr-types.h"
#include "gcr-certificate-field.h"

#include <glib-object.h>

G_BEGIN_DECLS

GcrCertificateSection *_gcr_certificate_section_new (const char *label,
                                                     gboolean    important);
void                   _gcr_certificate_section_append_field (GcrCertificateSection *section,
                                                              GcrCertificateField   *field);
GcrCertificateField   *_gcr_certificate_field_new_take_value (GcrCertificateSection *section,
                                                              const char            *label,
                                                              char                  *value);
GcrCertificateField   *_gcr_certificate_field_new_take_values (GcrCertificateSection *section,
                                                               const char            *label,
                                                               GStrv                  value);
GcrCertificateField   *_gcr_certificate_field_new_take_bytes (GcrCertificateSection *section,
                                                              const char            *label,
                                                              GBytes                *bytes);

static inline void
_gcr_certificate_section_new_field_take_value (GcrCertificateSection *section,
                                               const char            *label,
                                               char                  *value)
{
	GcrCertificateField *field = _gcr_certificate_field_new_take_value (section, label, value);
	_gcr_certificate_section_append_field (section, field);
	g_object_unref (field);
}

static inline void
_gcr_certificate_section_new_field_take_values (GcrCertificateSection *section,
                                                const char            *label,
                                                GStrv                  values)
{
	GcrCertificateField *field = _gcr_certificate_field_new_take_values (section, label, values);
	_gcr_certificate_section_append_field (section, field);
	g_object_unref (field);
}

static inline void
_gcr_certificate_section_new_field_take_bytes (GcrCertificateSection *section,
                                               const char            *label,
                                               GBytes                *bytes)
{
	GcrCertificateField *field = _gcr_certificate_field_new_take_bytes (section, label, bytes);
	_gcr_certificate_section_append_field (section, field);
	g_object_unref (field);
}

static inline void
_gcr_certificate_section_new_field (GcrCertificateSection *section,
                                    const char            *label,
                                    const char            *value)
{
	_gcr_certificate_section_new_field_take_value (section, label, g_strdup (value));
}

G_END_DECLS

#endif /* __GCR_CERTIFICATE_FIELD_PRIVATE_H__ */
