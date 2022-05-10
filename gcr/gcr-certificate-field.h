/*
 * Copyright 2022 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GCR_CERTIFICATE_FIELD_H__
#define __GCR_CERTIFICATE_FIELD_H__

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif

#include <gcr/gcr-certificate-section.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_FIELD (gcr_certificate_field_get_type ())

G_DECLARE_FINAL_TYPE (GcrCertificateField, gcr_certificate_field, GCR, CERTIFICATE_FIELD, GObject)

const char               *gcr_certificate_field_get_label      (GcrCertificateField *self);
gboolean                  gcr_certificate_field_get_value      (GcrCertificateField *self,
                                                                GValue              *value);
GType                     gcr_certificate_field_get_value_type (GcrCertificateField *self);
GcrCertificateSection    *gcr_certificate_field_get_section    (GcrCertificateField *self);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_FIELD_H__ */
