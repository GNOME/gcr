/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gcr-section.h"

struct _GcrSection
{
	GtkGrid parent_instance;

	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *image;
	GtkWidget *listbox;
	GtkSizeGroup *size_group;
};

G_DEFINE_TYPE (GcrSection, gcr_section, GTK_TYPE_GRID)

enum {
	PROP_TITLE = 1,
	PROP_ICON,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
gcr_section_dispose (GObject *object)
{
	GcrSection *self = (GcrSection *)object;

	g_clear_object (&self->size_group);
	/* g_clear_pointer (&self->label, gtk_widget_unparent); */
	/* g_clear_pointer (&self->image, gtk_widget_unparent); */
	/* g_clear_pointer (&self->frame, gtk_widget_unparent); */

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
	case PROP_ICON:
		{
			GIcon *icon;
			gtk_image_get_gicon (GTK_IMAGE (self->image), &icon, NULL);
			g_value_set_object (value, icon);
		}
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
	case PROP_ICON:
		if (!self->image)
		{
			self->image = gtk_image_new ();
			gtk_grid_attach (GTK_GRID (self), self->image, 1, 0, 1, 1);
			gtk_container_child_set (GTK_CONTAINER (self), self->frame, "width", 2, NULL);
		}

		gtk_image_set_from_gicon (GTK_IMAGE (self->image), g_value_get_object (value), GTK_ICON_SIZE_BUTTON);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gcr_section_class_init (GcrSectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gcr_section_dispose;
	object_class->get_property = gcr_section_get_property;
	object_class->set_property = gcr_section_set_property;
	obj_properties[PROP_TITLE] =
		g_param_spec_string ("title",
		                     "Title",
		                     "The title of the section",
		                     NULL,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	obj_properties[PROP_ICON] =
		g_param_spec_object ("icon",
		                     "Icon",
		                     "The Icon to use",
		                     G_TYPE_ICON,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

static void
gcr_section_init (GcrSection *self)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
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
	gtk_style_context_add_class (gtk_widget_get_style_context (self->label), "heading");
	gtk_style_context_add_class (gtk_widget_get_style_context (self->label), "h4");
	self->listbox = gtk_list_box_new ();
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->listbox), GTK_SELECTION_NONE);
	self->frame = gtk_frame_new (NULL);
	g_object_set (G_OBJECT (self->frame),
		      "child", self->listbox,
		      "hexpand", TRUE,
		      NULL);
	self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
   gtk_container_add (GTK_CONTAINER (self), self->label);
   gtk_container_add (GTK_CONTAINER (self), self->frame);

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

GtkWidget *
gcr_section_new_with_icon (const gchar *title,
                           GIcon       *icon)
{
	return g_object_new (GCR_TYPE_SECTION, "title", title, "icon", icon, NULL);
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
	gtk_container_add (GTK_CONTAINER (row), box);
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
	gtk_container_add (GTK_CONTAINER (box), label);
	gtk_container_add (GTK_CONTAINER (box), child);
	gtk_container_add (GTK_CONTAINER (self->listbox), row);
}
