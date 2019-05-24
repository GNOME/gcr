/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-viewer-widget: Widget for viewer

   Copyright (C) 2011 Collabora Ltd.

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr/gcr-importer.h"
#include "gcr/gcr-marshal.h"
#include "gcr/gcr-parser.h"

#include "gcr-display-scrolled.h"
#include "gcr-failure-renderer.h"
#include "gcr-renderer.h"
#include "gcr-unlock-renderer.h"
#include "gcr-viewer-widget.h"
#include "gcr-viewer.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <locale.h>
#include <string.h>

/**
 * SECTION:gcr-viewer-widget
 * @title: GcrViewerWidget
 * @short_description: A widget which shows certificates or keys
 *
 * A viewer widget which can display certificates and keys that are
 * located in files.
 */

enum {
	PROP_0,
	PROP_PARSER,
	PROP_DISPLAY_NAME
};

/**
 * GcrViewerWidget:
 *
 * A viewer widget object.
 */

/**
 * GcrViewerWidgetClass:
 *
 * Class for #GcrViewerWidget
 */

/*
 * Not yet figured out how to expose these without locking down our
 * implementation, the parent class we derive from.
 */

#define GCR_VIEWER_WIDGET_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_VIEWER_WIDGET, GcrViewerWidgetClass))
#define GCR_IS_VIEWER_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_VIEWER_WIDGET))
#define GCR_VIEWER_WIDGET_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_VIEWER_WIDGET, GcrViewerWidgetClass))

typedef struct _GcrViewerWidgetClass GcrViewerWidgetClass;
typedef struct _GcrViewerWidgetPrivate GcrViewerWidgetPrivate;

struct _GcrViewerWidget {
	/*< private >*/
	GtkBox parent;
	GcrViewerWidgetPrivate *pv;
};

struct _GcrViewerWidgetClass {
	GtkBoxClass parent_class;

	void       (*added)        (GcrViewerWidget *widget,
	                            GcrRenderer *renderer,
	                            GcrParsed *parsed);
};

struct _GcrViewerWidgetPrivate {
	GcrViewer *viewer;
	GtkInfoBar *message_bar;
	GtkLabel *message_label;
	GQueue *files_to_load;
	GcrParser *parser;
	GCancellable *cancellable;
	GList *unlocks;
	gboolean loading;
	gchar *display_name;
	gboolean display_name_explicit;
};

enum {
	ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void viewer_load_next_file (GcrViewerWidget *self);
static void viewer_stop_loading_files (GcrViewerWidget *self);

G_DEFINE_TYPE_WITH_PRIVATE (GcrViewerWidget, gcr_viewer_widget, GTK_TYPE_BOX);

static const gchar *
get_parsed_label_or_display_name (GcrViewerWidget *self,
                                  GcrParser *parser)
{
	const gchar *label;

	label = gcr_parser_get_parsed_label (parser);
	if (label == NULL)
		label = self->pv->display_name;

	return label;
}

static void
on_parser_parsed (GcrParser *parser,
                  gpointer user_data)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (user_data);
	GckAttributes *attrs;
	GcrRenderer *renderer;
	const gchar *label;
	gboolean actual = TRUE;

	label = get_parsed_label_or_display_name (self, parser);
	attrs = gcr_parser_get_parsed_attributes (parser);

	renderer = gcr_renderer_create (label, attrs);

	if (renderer == NULL) {
		renderer = gcr_failure_renderer_new_unsupported (label);
		actual = FALSE;
	}

	/* And show the data */
	gcr_viewer_add_renderer (self->pv->viewer, renderer);

	/* Let callers know we're rendering data */
	if (actual == TRUE)
		g_signal_emit (self, signals[ADDED], 0, renderer,
		               gcr_parser_get_parsed (parser));

	g_object_unref (renderer);
}

static gboolean
on_parser_authenticate_for_unlock (GcrParser *parser,
                                   guint count,
                                   gpointer user_data)
{
	GcrUnlockRenderer *unlock = GCR_UNLOCK_RENDERER (user_data);
	const gchar *password;

	if (count == 0) {
		password = _gcr_unlock_renderer_get_password (unlock);
		gcr_parser_add_password (parser, password);
	}

	return TRUE;
}

static void
on_unlock_renderer_clicked (GcrUnlockRenderer *unlock,
                            gpointer user_data)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (user_data);
	GError *error = NULL;
	GBytes *data;
	gulong sig;

	/* Override our main authenticate signal handler */
	sig = g_signal_connect (self->pv->parser, "authenticate",
	                        G_CALLBACK (on_parser_authenticate_for_unlock), unlock);

	data = _gcr_unlock_renderer_get_locked_data (unlock);
	if (gcr_parser_parse_bytes (self->pv->parser, data, &error)) {

		/* Done with this unlock renderer */
		gcr_viewer_remove_renderer (self->pv->viewer, GCR_RENDERER (unlock));
		self->pv->unlocks = g_list_remove (self->pv->unlocks, unlock);
		g_object_unref (unlock);

	} else if (g_error_matches (error, GCR_DATA_ERROR, GCR_ERROR_LOCKED)){
		_gcr_unlock_renderer_show_warning (unlock,  _("The password was incorrect"));
		_gcr_unlock_renderer_focus_password (unlock);
		_gcr_unlock_renderer_set_password (unlock, "");
		g_error_free (error);

	} else {
		_gcr_unlock_renderer_show_warning (unlock, error->message);
		g_error_free (error);
	}

	g_signal_handler_disconnect (self->pv->parser, sig);
}

static gboolean
on_parser_authenticate_for_data (GcrParser *parser,
                                 guint count,
                                 gpointer user_data)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (user_data);
	GcrUnlockRenderer *unlock;

	unlock = _gcr_unlock_renderer_new_for_parsed (parser);
	if (unlock != NULL) {
		g_object_set (unlock, "label", get_parsed_label_or_display_name (self, parser), NULL);
		gcr_viewer_add_renderer (self->pv->viewer, GCR_RENDERER (unlock));
		g_signal_connect (unlock, "unlock-clicked", G_CALLBACK (on_unlock_renderer_clicked), self);
		self->pv->unlocks = g_list_prepend (self->pv->unlocks, unlock);
	}

	return TRUE;
}

static void
gcr_viewer_widget_init (GcrViewerWidget *self)
{
	GtkWidget *area;

	self->pv = gcr_viewer_widget_get_instance_private (self);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
	                                GTK_ORIENTATION_VERTICAL);

	self->pv->viewer = gcr_viewer_new_scrolled ();
	gtk_box_pack_start (GTK_BOX (self), GTK_WIDGET (self->pv->viewer), TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (self->pv->viewer));

	self->pv->message_label = GTK_LABEL (gtk_label_new (""));
	gtk_label_set_use_markup (self->pv->message_label, TRUE);
	gtk_label_set_ellipsize (self->pv->message_label, PANGO_ELLIPSIZE_END);
	gtk_widget_show (GTK_WIDGET (self->pv->message_label));

	self->pv->message_bar = GTK_INFO_BAR (gtk_info_bar_new ());
	gtk_box_pack_start (GTK_BOX (self), GTK_WIDGET (self->pv->message_bar), FALSE, TRUE, 0);
	area = gtk_info_bar_get_content_area (self->pv->message_bar);
	gtk_container_add (GTK_CONTAINER (area), GTK_WIDGET (self->pv->message_label));

	self->pv->files_to_load = g_queue_new ();
	self->pv->parser = gcr_parser_new ();
	self->pv->cancellable = g_cancellable_new ();
	self->pv->unlocks = NULL;

	g_signal_connect (self->pv->parser, "parsed", G_CALLBACK (on_parser_parsed), self);
	g_signal_connect_after (self->pv->parser, "authenticate", G_CALLBACK (on_parser_authenticate_for_data), self);
}

static void
gcr_viewer_widget_get_property (GObject *obj,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (obj);

	switch (prop_id) {
	case PROP_PARSER:
		g_value_set_object (value, gcr_viewer_widget_get_parser (self));
		break;
	case PROP_DISPLAY_NAME:
		g_value_set_string (value, gcr_viewer_widget_get_display_name (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_viewer_widget_set_property (GObject *obj,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (obj);

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		gcr_viewer_widget_set_display_name (self, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_viewer_widget_dispose (GObject *obj)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (obj);
	GList *l;

	g_signal_handlers_disconnect_by_func (self->pv->parser, on_parser_parsed, self);

	for (l = self->pv->unlocks; l != NULL; l = g_list_next (l)) {
		g_signal_handlers_disconnect_by_func (l->data, on_unlock_renderer_clicked, self);
		g_object_unref (l->data);
	}
	g_list_free (self->pv->unlocks);
	self->pv->unlocks = NULL;

	while (!g_queue_is_empty (self->pv->files_to_load))
		g_object_unref (g_queue_pop_head (self->pv->files_to_load));

	g_cancellable_cancel (self->pv->cancellable);

	G_OBJECT_CLASS (gcr_viewer_widget_parent_class)->dispose (obj);
}

static void
gcr_viewer_widget_finalize (GObject *obj)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (obj);

	g_assert (g_queue_is_empty (self->pv->files_to_load));
	g_queue_free (self->pv->files_to_load);

	g_free (self->pv->display_name);
	g_object_unref (self->pv->cancellable);
	g_object_unref (self->pv->parser);

	G_OBJECT_CLASS (gcr_viewer_widget_parent_class)->finalize (obj);
}

static void
gcr_viewer_widget_class_init (GcrViewerWidgetClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = gcr_viewer_widget_dispose;
	gobject_class->finalize = gcr_viewer_widget_finalize;
	gobject_class->get_property = gcr_viewer_widget_get_property;
	gobject_class->set_property = gcr_viewer_widget_set_property;

	/**
	 * GcrViewerWidget:parser:
	 *
	 * The parser used to parse loaded data into viewable items.
	 */
	g_object_class_install_property (gobject_class, PROP_PARSER,
	           g_param_spec_object ("parser", "Parser", "Parser used to parse viewable items",
	                                GCR_TYPE_PARSER, G_PARAM_READABLE));

	/**
	 * GcrViewerWidget:display-name:
	 *
	 * Display name for data being displayed. This is automatically
	 * calculated from a loaded file, or can be explicitly set.
	 *
	 * Used as a hint when displaying a title for the data, but may be
	 * overridden by the parsed data.
	 */
	g_object_class_install_property (gobject_class, PROP_DISPLAY_NAME,
	            g_param_spec_string ("display-name", "Display name", "Display name",
	                                 NULL, G_PARAM_READWRITE));

	/**
	 * GcrViewerWidget::added:
	 * @self: the viewer widget
	 * @renderer: (type GcrUi.Renderer): the renderer that was added
	 * @parsed: (type Gcr.Parsed): the parsed item that was added
	 *
	 * This signal is emitted when an item is added to the viewer widget.
	 */
	signals[ADDED] = g_signal_new ("added", GCR_TYPE_VIEWER_WIDGET, G_SIGNAL_RUN_LAST,
	                               G_STRUCT_OFFSET (GcrViewerWidgetClass, added),
	                               NULL, NULL, _gcr_marshal_VOID__OBJECT_BOXED,
	                               G_TYPE_NONE, 2, G_TYPE_OBJECT, GCR_TYPE_PARSED);
}

static void
on_parser_parse_stream_returned (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (user_data);
	GError *error = NULL;
	GcrRenderer *renderer;

	gcr_parser_parse_stream_finish (self->pv->parser, result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
	    g_error_matches (error, GCR_DATA_ERROR, GCR_ERROR_CANCELLED)) {
		viewer_stop_loading_files (self);

	} else if (g_error_matches (error, GCR_DATA_ERROR, GCR_ERROR_LOCKED)) {
		/* Just skip this one, an unlock renderer was added */

	} else if (error) {
		renderer = gcr_failure_renderer_new (self->pv->display_name, error);
		gcr_viewer_add_renderer (self->pv->viewer, renderer);
		g_object_unref (renderer);
		g_error_free (error);
	}

	viewer_load_next_file (self);
}

static void
update_display_name (GcrViewerWidget *self,
                     gchar *display_name)
{
	if (!self->pv->display_name_explicit) {
		g_free (self->pv->display_name);
		self->pv->display_name = g_strdup (display_name);
		g_object_notify (G_OBJECT (self), "display-name");
	}
}

static void
on_file_read_returned (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
	GcrViewerWidget *self = GCR_VIEWER_WIDGET (user_data);
	GFile *file = G_FILE (source);
	GError *error = NULL;
	GFileInputStream *fis;
	GcrRenderer *renderer;
	gchar *basename, *display_name;

	fis = g_file_read_finish (file, result, &error);

	basename = g_file_get_basename (file);
	display_name = g_filename_display_name (basename);
	g_free (basename);

	update_display_name (self, display_name);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		viewer_stop_loading_files (self);

	} else if (error) {
		renderer = gcr_failure_renderer_new (self->pv->display_name, error);
		gcr_viewer_add_renderer (self->pv->viewer, renderer);
		g_object_unref (renderer);
		g_error_free (error);

		viewer_load_next_file (self);

	} else {
		gcr_parser_set_filename (self->pv->parser, display_name);
		gcr_parser_parse_stream_async (self->pv->parser,
		                               G_INPUT_STREAM (fis),
		                               self->pv->cancellable,
		                               on_parser_parse_stream_returned,
		                               self);
		g_object_unref (fis);
	}
	g_free (display_name);
}

static void
viewer_stop_loading_files (GcrViewerWidget *self)
{
	self->pv->loading = FALSE;
}

static void
viewer_load_next_file (GcrViewerWidget *self)
{
	GFile* file;

	file = g_queue_pop_head (self->pv->files_to_load);
	if (file == NULL) {
		viewer_stop_loading_files (self);
		return;
	}

	g_file_read_async (file, G_PRIORITY_DEFAULT, self->pv->cancellable,
	                   on_file_read_returned, self);

	g_object_unref (file);
}

/**
 * gcr_viewer_widget_new:
 *
 * Create a new viewer widget.
 *
 * Returns: (transfer full): A new #GcrViewerWidget object
 */
GcrViewerWidget *
gcr_viewer_widget_new (void)
{
	return g_object_new (GCR_TYPE_VIEWER_WIDGET, NULL);
}

/**
 * gcr_viewer_widget_load_file:
 * @self: a viewer widget
 * @file: a file to load
 *
 * Display contents of a file in the viewer widget. Multiple files can
 * be loaded.
 */
void
gcr_viewer_widget_load_file (GcrViewerWidget *self,
                             GFile *file)
{
	g_return_if_fail (GCR_IS_VIEWER_WIDGET (self));
	g_return_if_fail (G_IS_FILE (file));

	g_queue_push_tail (self->pv->files_to_load, g_object_ref (file));

	if (!self->pv->loading)
		viewer_load_next_file (self);
}

/**
 * gcr_viewer_widget_load_bytes:
 * @self: a viewer widget
 * @display_name: (allow-none): label for the loaded data
 * @data: data to load
 *
 * Parse and load some data to be displayed into the viewer widgets. The data
 * may contain multiple parseable items if the format can contain multiple
 * items.
 */
void
gcr_viewer_widget_load_bytes (GcrViewerWidget *self,
                              const gchar *display_name,
                              GBytes *data)
{
	GError *error = NULL;
	GcrRenderer *renderer;

	g_return_if_fail (GCR_IS_VIEWER_WIDGET (self));
	g_return_if_fail (data != NULL);

	g_free (self->pv->display_name);
	self->pv->display_name = g_strdup (display_name);

	if (!gcr_parser_parse_bytes (self->pv->parser, data, &error)) {
		renderer = gcr_failure_renderer_new (display_name, error);
		gcr_viewer_add_renderer (self->pv->viewer, renderer);
		g_object_unref (renderer);
		g_error_free (error);
	}
}

/**
 * gcr_viewer_widget_load_data:
 * @self: a viewer widget
 * @display_name: (allow-none): label for the loaded data
 * @data: (array length=n_data): data to load
 * @n_data: length of data to load
 *
 * Parse and load some data to be displayed into the viewer widgets. The data
 * may contain multiple parseable items if the format can contain multiple
 * items.
 *
 * This function will copy the data. Use gcr_viewer_widget_load_bytes() to avoid
 * copying the data.
 */
void
gcr_viewer_widget_load_data (GcrViewerWidget *self,
                             const gchar *display_name,
                             const guchar *data,
                             gsize n_data)
{
	GBytes *bytes;

	g_return_if_fail (GCR_IS_VIEWER_WIDGET (self));

	bytes = g_bytes_new (data, n_data);
	gcr_viewer_widget_load_bytes (self, display_name, bytes);
	g_bytes_unref (bytes);
}

/**
 * gcr_viewer_widget_get_viewer:
 * @self: a viewer widget
 *
 * Get the viewer used to display the viewable items.
 *
 * Returns: (transfer none): the viewer
 */
GcrViewer *
gcr_viewer_widget_get_viewer (GcrViewerWidget *self)
{
	g_return_val_if_fail (GCR_IS_VIEWER_WIDGET (self), NULL);
	return self->pv->viewer;
}

/**
 * gcr_viewer_widget_get_parser:
 * @self: a viewer widget
 *
 * Get the parser used to parse loaded data into viewable items.
 *
 * Returns: (transfer none): the parser
 */
GcrParser *
gcr_viewer_widget_get_parser (GcrViewerWidget *self)
{
	g_return_val_if_fail (GCR_IS_VIEWER_WIDGET (self), NULL);
	return self->pv->parser;
}

/**
 * gcr_viewer_widget_show_error:
 * @self: a viewer widget
 * @message: descriptive error message
 * @error: (allow-none): detailed error
 *
 * Show an error on the viewer widget. This is displayed on a info bar near
 * the edge of the widget.
 */
void
gcr_viewer_widget_show_error (GcrViewerWidget *self,
                              const gchar *message,
                              GError *error)
{
	gchar *markup;

	g_return_if_fail (GCR_IS_VIEWER_WIDGET (self));
	g_return_if_fail (message != NULL);

	if (error)
		markup = g_markup_printf_escaped ("<b>%s</b>: %s", message, error->message);
	else
		markup = g_markup_printf_escaped ("%s", message);

	gtk_info_bar_set_message_type (self->pv->message_bar, GTK_MESSAGE_ERROR);
	gtk_label_set_markup (self->pv->message_label, markup);
	gtk_widget_show (GTK_WIDGET (self->pv->message_bar));
	g_free (markup);
}

/**
 * gcr_viewer_widget_clear_error:
 * @self: a viewer widget
 *
 * Clear the error displayed on the viewer widget.
 */
void
gcr_viewer_widget_clear_error (GcrViewerWidget *self)
{
	g_return_if_fail (GCR_IS_VIEWER_WIDGET (self));
	gtk_widget_hide (GTK_WIDGET (self->pv->message_bar));
}

/**
 * gcr_viewer_widget_get_display_name:
 * @self: a viewer widget
 *
 * Get the display name for data being displayed. This is automatically
 * calculated from a loaded file, or can be explicitly set.
 *
 * Used as a hint when displaying a title for the data, but may be
 * overridden by the parsed data.
 *
 * Returns: the display name
 */
const gchar *
gcr_viewer_widget_get_display_name (GcrViewerWidget *self)
{
	g_return_val_if_fail (GCR_IS_VIEWER_WIDGET (self), NULL);

	if (!self->pv->display_name_explicit && !self->pv->display_name)
		self->pv->display_name = g_strdup (_("Certificate Viewer"));

	return self->pv->display_name;
}

/**
 * gcr_viewer_widget_set_display_name:
 * @self: a viewer widget
 * @display_name: the display name
 *
 * Set the display name for data being displayed. Once explicitly
 * set it will no longer be calculated automatically by loading data.
 *
 * Used as a hint when displaying a title for the data, but may be
 * overridden by the parsed data.
 */
void
gcr_viewer_widget_set_display_name (GcrViewerWidget *self,
                                    const gchar *display_name)
{
	g_return_if_fail (GCR_IS_VIEWER_WIDGET (self));

	g_free (self->pv->display_name);
	self->pv->display_name = g_strdup (display_name);
	self->pv->display_name_explicit = TRUE;
	g_object_notify (G_OBJECT (self), "display-name");
}
