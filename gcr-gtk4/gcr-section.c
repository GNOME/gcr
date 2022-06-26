/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gcr-section.h"

struct _GcrSection
{
	GtkWidget parent_instance;

	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *image;
	GtkWidget *listbox;
	GtkSizeGroup *size_group;
};

G_DEFINE_TYPE (GcrSection, gcr_section, GTK_TYPE_WIDGET)

enum {
	PROP_TITLE = 1,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
gcr_section_dispose (GObject *object)
{
	GcrSection *self = (GcrSection *)object;

	g_clear_object (&self->size_group);
	g_clear_pointer (&self->label, gtk_widget_unparent);
	g_clear_pointer (&self->image, gtk_widget_unparent);
	g_clear_pointer (&self->frame, gtk_widget_unparent);

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
	case PROP_TITLE:
		g_value_set_string (value, gtk_label_get_label (GTK_LABEL (self->label)));
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
	case PROP_TITLE:
		gtk_label_set_label (GTK_LABEL (self->label), g_value_get_string (value));
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
	obj_properties[PROP_TITLE] =
		g_param_spec_string ("title",
		                     "Title",
		                     "The title of the section",
		                     NULL,
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
	gtk_style_context_add_class (gtk_widget_get_style_context (self->label), "caption-heading");
	self->listbox = gtk_list_box_new ();
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->listbox), GTK_SELECTION_NONE);
	self->frame = gtk_frame_new (NULL);
	g_object_set (G_OBJECT (self->frame),
		      "child", self->listbox,
		      "hexpand", TRUE,
		      NULL);
	self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_widget_insert_after (self->label, GTK_WIDGET (self), NULL);
	gtk_widget_insert_after (self->frame, GTK_WIDGET (self), self->label);
	layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));

	child = gtk_layout_manager_get_layout_child (layout_manager, self->frame);
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
gcr_section_new (const gchar *title)
{
	return g_object_new (GCR_TYPE_SECTION, "title", title, NULL);
}

void
gcr_section_add_child (GcrSection  *self,
                       const gchar *description,
                       GtkWidget   *child)
{
	GtkWidget *row, *box, *label;
	g_return_if_fail (GCR_IS_SECTION (self));

	row = gtk_list_box_row_new ();
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
	label = gtk_label_new (description);
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
	g_object_set (child,
		      "margin-start", 12,
		      "margin-end", 12,
		      "margin-top", 8,
		      "margin-bottom", 8,
		      "halign", GTK_ALIGN_END,
		      NULL);
	gtk_size_group_add_widget (self->size_group, label);
	gtk_box_append (GTK_BOX (box), label);
	gtk_box_append (GTK_BOX (box), child);
	gtk_list_box_insert (GTK_LIST_BOX (self->listbox), row, -1);
}
