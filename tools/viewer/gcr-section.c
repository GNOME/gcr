/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gcr-section.h"

struct _GcrSection
{
	GtkWidget parent_instance;

	GcrCertificateSection *section;
	GtkWidget *label;
	GtkWidget *listbox;
	GtkSizeGroup *size_group;
};

G_DEFINE_TYPE (GcrSection, gcr_section, GTK_TYPE_WIDGET)

enum {
	PROP_SECTION = 1,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static char*
bytes_to_display (GBytes *bytes)
{
	const char *hexc = "0123456789ABCDEF";
	GString *result;
	const char *input;
	gsize read_bytes;
	gsize remaining_bytes;
	guchar j;

	g_return_val_if_fail (bytes != NULL, NULL);

	input = g_bytes_get_data (bytes, &remaining_bytes);
	result = g_string_sized_new (remaining_bytes * 2 + 1);

	for (read_bytes = 0; read_bytes < remaining_bytes; read_bytes++) {
		if (read_bytes && (read_bytes % 1) == 0)
			g_string_append_c (result, ' ');

		j = *(input) >> 4 & 0xf;
		g_string_append_c (result, hexc[j]);

		j = *(input++) & 0xf;
		g_string_append_c (result, hexc[j]);
	}

	return g_string_free (result, FALSE);
}

GtkWidget *
gcr_section_create_row (GObject *item,
                        gpointer user_data)
{
	GcrCertificateField *field = (GcrCertificateField *) item;
	GtkSizeGroup *size_group = user_data;
	GtkWidget *row, *box, *label, *value;
	char *value_a11y_desc;
	GValue val = G_VALUE_INIT;
	GType value_type;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_FIELD (field), NULL);

	row = gtk_list_box_row_new ();
	gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
	gtk_widget_set_focusable (row, FALSE);
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
	label = gtk_label_new (gcr_certificate_field_get_label (field));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	g_object_set (label,
	              "margin-start", 12,
	              "margin-end", 12,
	              "margin-top", 8,
	              "margin-bottom", 8,
	              "xalign", 0.0,
	              "halign", GTK_ALIGN_START,
	              NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (label), "caption");
	value = g_object_new (GTK_TYPE_LABEL,
	                      "xalign", 1.0,
	                      "halign", GTK_ALIGN_END,
	                      "hexpand", TRUE,
	                      "selectable", TRUE,
	                      "wrap", TRUE,
	                      NULL);
	value_type = gcr_certificate_field_get_value_type (field);
	g_value_init (&val, value_type);
	gcr_certificate_field_get_value (field, &val);
	if (g_type_is_a (value_type, G_TYPE_STRING)) {
		g_object_set (G_OBJECT (value), "label", g_value_get_string (&val), NULL);
	} else if (g_type_is_a (value_type, G_TYPE_STRV)) {
		char *lbl = g_strjoinv ("\n", (GStrv) g_value_get_boxed (&val));
		g_object_set (G_OBJECT (value), "label", lbl, NULL);
		g_free (lbl);
	} else if (g_type_is_a (value_type, G_TYPE_BYTES)) {
		PangoAttrList *attributes;
		GBytes *bytes = g_value_get_boxed (&val);
		char *lbl = bytes_to_display (bytes);
		g_object_set (G_OBJECT (value), "label", lbl, NULL);
		g_free (lbl);
		attributes = pango_attr_list_new ();
		pango_attr_list_insert (attributes, pango_attr_family_new ("Monospace"));
		gtk_label_set_attributes (GTK_LABEL (value), attributes);
		pango_attr_list_unref (attributes);
	}
	g_value_unset (&val);

	g_object_set (value,
	              "margin-start", 12,
	              "margin-end", 12,
	              "margin-top", 8,
	              "margin-bottom", 8,
	              "halign", GTK_ALIGN_END,
	              NULL);

	value_a11y_desc = g_strdup_printf ("%s: %s",
	                                   gtk_label_get_text (GTK_LABEL (label)),
	                                   gtk_label_get_text (GTK_LABEL (value)));
	gtk_accessible_update_property (GTK_ACCESSIBLE (value),
	                                GTK_ACCESSIBLE_PROPERTY_LABEL,value_a11y_desc,
	                                -1);
	g_free (value_a11y_desc);

	gtk_size_group_add_widget (size_group, label);
	gtk_box_append (GTK_BOX (box), label);
	gtk_box_append (GTK_BOX (box), value);
	return row;
}

static void
gcr_section_dispose (GObject *object)
{
	GcrSection *self = (GcrSection *)object;

	g_clear_pointer (&self->label, gtk_widget_unparent);
	g_clear_pointer (&self->listbox, gtk_widget_unparent);
	g_clear_object (&self->size_group);
	g_clear_object (&self->section);

	G_OBJECT_CLASS (gcr_section_parent_class)->dispose (object);
}

static void
gcr_section_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
	GcrSection *self = GCR_SECTION (object);

	switch (prop_id)
	{
	case PROP_SECTION:
		g_value_set_object (value, self->section);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_section_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
	GcrSection *self = GCR_SECTION (object);

	switch (prop_id)
	{
	case PROP_SECTION:
		g_assert (!self->section);
		self->section = g_value_dup_object (value);
		gtk_label_set_label (GTK_LABEL (self->label), gcr_certificate_section_get_label (self->section));
		gtk_list_box_bind_model (GTK_LIST_BOX (self->listbox),
		                         gcr_certificate_section_get_fields (self->section),
		                         (GtkListBoxCreateWidgetFunc) gcr_section_create_row,
		                         self->size_group, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_section_class_init (GcrSectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gcr_section_dispose;
	object_class->get_property = gcr_section_get_property;
	object_class->set_property = gcr_section_set_property;
	obj_properties[PROP_SECTION] =
		g_param_spec_object ("section",
		                     "Section",
		                     "The section object",
		                     GCR_TYPE_CERTIFICATE_SECTION,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   obj_properties);

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_GRID_LAYOUT);
}

static void
gcr_section_init (GcrSection *self)
{
	GtkLayoutManager *layout_manager;
	GtkLayoutChild *child;

	self->label = gtk_label_new (NULL);
	g_object_set (G_OBJECT (self->label),
	              "margin-start", 12,
	              "margin-end", 12,
	              "margin-top", 8,
	              "margin-bottom", 8,
	              "xalign", 0.0,
	              "halign", GTK_ALIGN_START,
	              "hexpand", TRUE,
	              NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (self->label), "title");
	gtk_style_context_add_class (gtk_widget_get_style_context (self->label), "caption-heading");
	self->listbox = gtk_list_box_new ();
	gtk_style_context_add_class (gtk_widget_get_style_context (self->listbox), "frame");
	gtk_style_context_add_class (gtk_widget_get_style_context (self->listbox), "rich-list");
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->listbox), GTK_SELECTION_NONE);
	self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_widget_insert_after (self->label, GTK_WIDGET (self), NULL);
	gtk_widget_insert_after (self->listbox, GTK_WIDGET (self), self->label);
	layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));

	child = gtk_layout_manager_get_layout_child (layout_manager, self->listbox);
	g_object_set (G_OBJECT (child),
	              "row", 1,
	              NULL);
	g_object_set (self,
	              "margin-start", 12,
	              "margin-end", 12,
	              "margin-top", 8,
	              "margin-bottom", 8,
	              NULL);
}

GtkWidget *
gcr_section_new (GcrCertificateSection *section)
{
	return g_object_new (GCR_TYPE_SECTION, "section", section, NULL);
}
