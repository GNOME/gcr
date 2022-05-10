/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gcr-certificate-widget.h"
#include "gcr-section.h"

struct _GcrCertificateWidget
{
	GtkBox parent_instance;

	GcrCertificate *certificate;
	GtkWidget *reveal_button;
	GtkWidget *revealer;
	GtkWidget *primary_info;
	GtkWidget *secondary_info;
	GtkSizeGroup *size_group;
};

G_DEFINE_TYPE (GcrCertificateWidget, gcr_certificate_widget, GTK_TYPE_BOX)

enum {
	PROP_CERTIFICATE = 1,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
gcr_certificate_widget_finalize (GObject *object)
{
	GcrCertificateWidget *self = (GcrCertificateWidget *)object;

	g_clear_object (&self->size_group);

	G_OBJECT_CLASS (gcr_certificate_widget_parent_class)->finalize (object);
}

static void
gcr_certificate_widget_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	GcrCertificateWidget *self = GCR_CERTIFICATE_WIDGET (object);

	switch (prop_id)
	{
	case PROP_CERTIFICATE:
		g_value_set_object (value, gcr_certificate_widget_get_certificate (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gcr_certificate_widget_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	GcrCertificateWidget *self = GCR_CERTIFICATE_WIDGET (object);

	switch (prop_id)
	{
	case PROP_CERTIFICATE:
		gcr_certificate_widget_set_certificate (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_certificate_widget_class_init (GcrCertificateWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gcr_certificate_widget_finalize;
	object_class->get_property = gcr_certificate_widget_get_property;
	object_class->set_property = gcr_certificate_widget_set_property;

	obj_properties[PROP_CERTIFICATE] =
		g_param_spec_object ("certificate",
		                     "Certificate",
		                     "Certificate to display.",
		                     GCR_TYPE_CERTIFICATE,
		                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

static void
on_reveal_button_clicked (GtkWidget *button,
			  GcrCertificateWidget *self)
{
	g_assert (GCR_IS_CERTIFICATE_WIDGET (self));
	gtk_widget_hide (button);
	gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
}

static void
gcr_certificate_widget_init (GcrCertificateWidget *self)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
	self->reveal_button = gtk_button_new_with_label ("…");
	gtk_widget_set_halign (self->reveal_button, GTK_ALIGN_CENTER);
	gtk_box_pack_end (GTK_BOX (self), self->reveal_button, FALSE, TRUE, 0);
	self->primary_info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	self->secondary_info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (self), self->primary_info, FALSE, TRUE, 0);
	self->revealer = g_object_new (GTK_TYPE_REVEALER,
	                               "child", self->secondary_info,
	                               "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
	                               NULL);
	gtk_box_pack_end (GTK_BOX (self), self->revealer, FALSE, TRUE, 0);
	g_signal_connect (self->reveal_button, "clicked", G_CALLBACK (on_reveal_button_clicked), self);
	self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
}

/**
 * gcr_certificate_widget_new:
 * @certificate: (nullable): certificate to display, or %NULL
 *
 * Create a new certificate widget which displays a given certificate.
 *
 * Returns: (transfer full): a newly allocated #GcrCertificateWidget, which
 *          should be freed with g_object_unref()
 */
GtkWidget *
gcr_certificate_widget_new (GcrCertificate *certificate)
{
	return g_object_new (GCR_TYPE_CERTIFICATE_WIDGET, "certificate", certificate, NULL);
}

/**
 * gcr_certificate_widget_get_certificate:
 * @self: The certificate widget
 *
 * Get the certificate displayed in the widget.
 *
 * Returns: (nullable) (transfer none): the certificate
 */
GcrCertificate *
gcr_certificate_widget_get_certificate (GcrCertificateWidget *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_WIDGET (self), NULL);
	return self->certificate;
}

/**
 * gcr_certificate_widget_set_certificate:
 * @self: The certificate widget
 * @certificate: (nullable): the certificate to display
 *
 * Set the certificate displayed in the widget
 */
void
gcr_certificate_widget_set_certificate (GcrCertificateWidget *self, GcrCertificate *certificate)
{
	GList* elements, *l, *children;

	g_return_if_fail (GCR_IS_CERTIFICATE_WIDGET (self));

	children = gtk_container_get_children (GTK_CONTAINER (self->secondary_info));
	for (l = children; l != NULL; l = l->next) {
		gtk_widget_destroy (GTK_WIDGET (l->data));
	}

	g_list_free (children);

	g_set_object (&self->certificate, certificate);
	if (!certificate) {
		return;
	}

	elements = gcr_certificate_get_interface_elements (certificate);
	for (l = elements; l != NULL; l = l->next) {
		GcrCertificateSection *section = l->data;
		GtkWidget *widget;

		widget = gcr_section_new (section);

		gtk_size_group_add_widget (self->size_group, widget);
		if (gcr_certificate_section_get_flags (section) & GCR_CERTIFICATE_SECTION_IMPORTANT)
			gtk_box_pack_start (GTK_BOX (self->primary_info), widget, FALSE, FALSE, 0);
		else
			gtk_box_pack_start (GTK_BOX (self->secondary_info), widget, FALSE, FALSE, 0);
	}

	g_list_free_full (elements, (GDestroyNotify) g_object_unref);
}
