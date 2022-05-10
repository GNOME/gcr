/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gcr-certificate-field.h"
#include "gcr-certificate-field-private.h"
#include "gcr-enum-types.h"

struct _GcrCertificateField
{
	GObject parent_instance;

	char *label;
	GValue value;
	GcrCertificateSection *section;
};

G_DEFINE_FINAL_TYPE (GcrCertificateField, gcr_certificate_field, G_TYPE_OBJECT)

enum {
	PROP_LABEL = 1,
	PROP_VALUE,
	PROP_SECTION,
	N_PROPERTIES
};

static GParamSpec *obj_properties [N_PROPERTIES];

static void
gcr_certificate_field_finalize (GObject *object)
{
	GcrCertificateField *self = (GcrCertificateField *)object;

	g_clear_pointer (&self->label, g_free);
	g_value_unset (&self->value);

	G_OBJECT_CLASS (gcr_certificate_field_parent_class)->finalize (object);
}

static void
gcr_certificate_field_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GcrCertificateField *self = GCR_CERTIFICATE_FIELD (object);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_set_string (value, self->label);
		break;
	case PROP_VALUE:
		g_value_set_boxed (value, &self->value);
		break;
	case PROP_SECTION:
		g_value_set_object (value, self->section);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_field_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	GcrCertificateField *self = GCR_CERTIFICATE_FIELD (object);

	switch (prop_id) {
	case PROP_LABEL:
		self->label = g_value_dup_string (value);
		break;
	case PROP_SECTION:
		self->section = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_field_class_init (GcrCertificateFieldClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gcr_certificate_field_finalize;
	object_class->get_property = gcr_certificate_field_get_property;
	object_class->set_property = gcr_certificate_field_set_property;

	obj_properties[PROP_LABEL] =
		g_param_spec_string ("label",
		                     "Label",
		                     "Display name of the field.",
		                     NULL,
		                     G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	obj_properties[PROP_VALUE] =
		g_param_spec_boxed ("value",
		                    "Value",
		                    "Display name of the value.",
		                    G_TYPE_VALUE,
		                    G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

	obj_properties[PROP_SECTION] =
		g_param_spec_object ("section",
		                     "Section",
		                     "The section it is included.",
		                     GCR_TYPE_CERTIFICATE_SECTION,
		                     G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

static void
gcr_certificate_field_init (GcrCertificateField *self)
{
}

/**
 * gcr_certificate_field_get_label:
 * @self: the #GcrCertificateField
 *
 * Get the display label of the field.
 *
 * Returns: the display label of the field
 */
const char *
gcr_certificate_field_get_label (GcrCertificateField *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_FIELD (self), NULL);

	return self->label;
}

/**
 * gcr_certificate_field_get_value:
 * @self: the #GcrCertificateField
 * @value: (out caller-allocates): the `GValue` to fill
 *
 * Get the value of the field.
 *
 * The @value will have been initialized to the `GType` the value should be
 * provided in.
 *
 * Returns: %TRUE if the value was set successfully.
 */
gboolean
gcr_certificate_field_get_value (GcrCertificateField *self,
                                 GValue              *value)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_FIELD (self), FALSE);
	g_return_val_if_fail (G_IS_VALUE (value), FALSE);

	if (G_VALUE_HOLDS (&self->value, G_VALUE_TYPE (value))) {
		g_value_copy (&self->value, value);
		return TRUE;
	}

	return FALSE;
}

/**
 * gcr_certificate_field_get_value_type:
 * @self: the #GcrCertificateField
 *
 * Get the type associated with the value.
 *
 * Returns: The `GType` of the value
 */
GType
gcr_certificate_field_get_value_type (GcrCertificateField *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_FIELD (self), FALSE);

	return G_VALUE_TYPE (&self->value);
}

/**
 * gcr_certificate_field_get_section:
 * @self: the #GcrCertificateField
 *
 * Get the parent #GcrCertificateSection.
 *
 * Returns: (transfer none): the parent #GcrCertificateSection
 */
GcrCertificateSection *
gcr_certificate_field_get_section (GcrCertificateField *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_FIELD (self), NULL);

	return self->section;
}

GcrCertificateField *
_gcr_certificate_field_new_take_value (GcrCertificateSection *section,
                                       const char            *label,
                                       char                  *value)
{
	GcrCertificateField *self;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_SECTION (section), NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	self = g_object_new (GCR_TYPE_CERTIFICATE_FIELD,
	                     "section", section,
	                     "label", label,
	                     NULL);
	g_value_init (&self->value, G_TYPE_STRING);
	g_value_take_string (&self->value, value);

	return self;
}

GcrCertificateField *
_gcr_certificate_field_new_take_values (GcrCertificateSection *section,
                                        const char            *label,
                                        GStrv                  values)
{
	GcrCertificateField *self;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_SECTION (section), NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (values != NULL, NULL);

	self = g_object_new (GCR_TYPE_CERTIFICATE_FIELD,
	                     "section", section,
	                     "label", label,
	                     NULL);
	g_value_init (&self->value, G_TYPE_STRV);
	g_value_take_boxed (&self->value, values);

	return self;
}

GcrCertificateField *
_gcr_certificate_field_new_take_bytes (GcrCertificateSection *section,
                                       const char            *label,
                                       GBytes                *bytes)
{
	GcrCertificateField *self;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_SECTION (section), NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (bytes != NULL, NULL);

	self = g_object_new (GCR_TYPE_CERTIFICATE_FIELD,
	                     "section", section,
	                     "label", label,
	                     NULL);
	g_value_init (&self->value, G_TYPE_BYTES);
	g_value_take_boxed (&self->value, bytes);

	return self;
}
