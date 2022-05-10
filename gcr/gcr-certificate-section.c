/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gcr-certificate-field.h"
#include "gcr-certificate-section.h"
#include "gcr-enum-types.h"

struct _GcrCertificateSection {
	GObject parent_instance;

	char *label;
	GcrCertificateSectionFlags flags;
	GListStore *fields;
};

G_DEFINE_FINAL_TYPE (GcrCertificateSection, gcr_certificate_section, G_TYPE_OBJECT)

enum {
	PROP_LABEL = 1,
	PROP_FIELDS,
	PROP_FLAGS,
	N_PROPERTIES
};

static GParamSpec *obj_properties [N_PROPERTIES];


/**
 * _gcr_certificate_section_new:
 * @label: the user-displayable label of the section
 * @important: whether this section should be visible by default
 *
 * Create a new certificate section usable in user interfaces.
 *
 * Returns: (transfer full): a new #GcrCertificateSection
 */
GcrCertificateSection *
_gcr_certificate_section_new (const char *label,
                              gboolean    important)
{
	return g_object_new (GCR_TYPE_CERTIFICATE_SECTION,
	                     "label", label,
	                     "flags", important ? GCR_CERTIFICATE_SECTION_IMPORTANT : GCR_CERTIFICATE_SECTION_NONE,
	                     NULL);
}

static void
gcr_certificate_section_finalize (GObject *object)
{
	GcrCertificateSection *self = (GcrCertificateSection *)object;
	g_clear_pointer (&self->label, g_free);

	G_OBJECT_CLASS (gcr_certificate_section_parent_class)->finalize (object);
}

static void
gcr_certificate_section_dispose (GObject *object)
{
	GcrCertificateSection *self = (GcrCertificateSection *)object;
	g_clear_object (&self->fields);

	G_OBJECT_CLASS (gcr_certificate_section_parent_class)->dispose (object);
}

static void
gcr_certificate_section_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	GcrCertificateSection *self = GCR_CERTIFICATE_SECTION (object);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_set_string (value, self->label);
		break;
	case PROP_FIELDS:
		g_value_set_object (value, &self->fields);
		break;
	case PROP_FLAGS:
		g_value_set_flags (value, self->flags);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_section_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	GcrCertificateSection *self = GCR_CERTIFICATE_SECTION (object);

	switch (prop_id) {
	case PROP_LABEL:
		self->label = g_value_dup_string (value);
		break;
	case PROP_FLAGS:
		self->flags = g_value_get_flags (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_section_class_init (GcrCertificateSectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gcr_certificate_section_finalize;
	object_class->dispose = gcr_certificate_section_dispose;
	object_class->get_property = gcr_certificate_section_get_property;
	object_class->set_property = gcr_certificate_section_set_property;

	obj_properties[PROP_LABEL] =
		g_param_spec_string ("label",
		                     "Label",
		                     "Display name of the field.",
		                     NULL,
		                     G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	obj_properties[PROP_FIELDS] =
		g_param_spec_object ("fields",
		                     "Fields",
		                     "The list of fields.",
		                     G_TYPE_LIST_MODEL,
		                     G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

	obj_properties[PROP_FLAGS] =
		g_param_spec_flags ("flags",
		                    "Flags",
		                    "Flags defined for the section.",
		                    GCR_TYPE_CERTIFICATE_SECTION_FLAGS,
		                    GCR_CERTIFICATE_SECTION_NONE,
		                    G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

static void
gcr_certificate_section_init (GcrCertificateSection *self)
{
	self->fields = g_list_store_new (GCR_TYPE_CERTIFICATE_FIELD);
}

/**
 * gcr_certificate_section_get_label:
 * @self: the #GcrCertificateSection
 *
 * Get the displayable label of the section.
 *
 * Returns: the displayable label of the section
 */
const char *
gcr_certificate_section_get_label (GcrCertificateSection *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->label;
}

/**
 * gcr_certificate_section_get_flags:
 * @self: the #GcrCertificateSection
 *
 * Get the flags.
 *
 * Returns: the `GcrCertificateSectionFlags`
 */
GcrCertificateSectionFlags
gcr_certificate_section_get_flags (GcrCertificateSection *self)
{
	g_return_val_if_fail (self != NULL, FALSE);

	return self->flags;
}

/**
 * gcr_certificate_section_get_fields:
 * @self: the #GcrCertificateSection
 *
 * Get the list of all the fields in this section.
 *
 * Returns: (transfer none): a #GListModel of #GcrCertificateField
 */
GListModel *
gcr_certificate_section_get_fields (GcrCertificateSection *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return G_LIST_MODEL (self->fields);
}

void
_gcr_certificate_section_append_field (GcrCertificateSection *section,
                                       GcrCertificateField   *field)
{
	g_return_if_fail (GCR_IS_CERTIFICATE_SECTION (section));
	g_return_if_fail (GCR_IS_CERTIFICATE_FIELD (field));

	g_list_store_append (section->fields, field);
}

