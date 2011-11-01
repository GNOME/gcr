/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-viewer-tool.c: Command line utility

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr.h"

#include "gcr-dbus-constants.h"
#define DEBUG_FLAG GCR_DEBUG_PROMPT
#include "gcr-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <pango/pango.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#define LOG_ERRORS 1
#define GRAB_KEYBOARD 1
#define QUIT_TIMEOUT 10

static GcrSystemPrompter *the_prompter = NULL;

GType   gcr_prompter_dialog_get_type           (void) G_GNUC_CONST;
#define GCR_TYPE_PROMPTER_DIALOG               (gcr_prompter_dialog_get_type ())
#define GCR_PROMPTER_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_PROMPTER_DIALOG, GcrPrompterDialog))
#define GCR_IS_PROMPTER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_PROMPTER_DIALOG))

typedef enum {
	PROMPT_NONE,
	PROMPT_CONFIRMING,
	PROMPT_PASSWORDING
} PromptMode;

enum {
	PROP_0,
	PROP_PASSWORD_VISIBLE,
	PROP_CONFIRM_VISIBLE,
	PROP_WARNING_VISIBLE,
	PROP_CHOICE_VISIBLE,
};

typedef struct {
	GtkDialog parent;
	GtkWidget *spinner;
	GtkWidget *image;
	GtkEntryBuffer *password_buffer;
	GtkEntryBuffer *confirm_buffer;
	PromptMode mode;
	GdkDevice *grabbed_device;
	gulong grab_broken_id;
	guint quit_timeout;
} GcrPrompterDialog;

typedef struct {
	GtkDialogClass parent;
} GcrPrompterDialogClass;

G_DEFINE_TYPE (GcrPrompterDialog, gcr_prompter_dialog, GTK_TYPE_DIALOG);

static gboolean
on_timeout_quit ()
{
	gtk_main_quit ();
	return FALSE; /* Don't run again */
}

static void
start_timeout (GcrPrompterDialog *self)
{
	if (g_getenv ("GCR_PERSIST") != NULL)
		return;

	if (!self->quit_timeout)
		self->quit_timeout = g_timeout_add_seconds (QUIT_TIMEOUT, on_timeout_quit, NULL);
}

static void
stop_timeout (GcrPrompterDialog *self)
{
	if (self->quit_timeout)
		g_source_remove (self->quit_timeout);
	self->quit_timeout = 0;
}

static void
on_open_prompt (GcrSystemPrompter *prompter,
                gpointer user_data)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);
	gtk_widget_show (GTK_WIDGET (self));
	stop_timeout (self);
}

static void
on_close_prompt (GcrSystemPrompter *prompter,
                 gpointer user_data)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);
	gtk_widget_hide (GTK_WIDGET (self));
	start_timeout (self);
}

static gboolean
on_prompt_confirm (GcrSystemPrompter *prompter,
                   gpointer user_data)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);
	GObject *obj;

	g_return_val_if_fail (self->mode == PROMPT_NONE, FALSE);

	self->mode = PROMPT_CONFIRMING;
	gtk_image_set_from_stock (GTK_IMAGE (self->image),
	                          GTK_STOCK_DIALOG_QUESTION,
	                          GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	gtk_widget_show (self->image);
	gtk_widget_hide (self->spinner);

	obj = G_OBJECT (self);
	g_object_notify (obj, "password-visible");
	g_object_notify (obj, "confirm-visible");
	g_object_notify (obj, "warning-visible");
	g_object_notify (obj, "choice-visible");

	return TRUE;
}

static gboolean
on_prompt_password (GcrSystemPrompter *prompter,
                    gpointer user_data)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);
	GObject *obj;

	g_return_val_if_fail (self->mode == PROMPT_NONE, FALSE);

	self->mode = PROMPT_PASSWORDING;
	gtk_image_set_from_stock (GTK_IMAGE (self->image),
	                          GTK_STOCK_DIALOG_AUTHENTICATION,
	                          GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	gtk_widget_show (self->image);
	gtk_widget_hide (self->spinner);

	obj = G_OBJECT (self);
	g_object_notify (obj, "password-visible");
	g_object_notify (obj, "confirm-visible");
	g_object_notify (obj, "warning-visible");
	g_object_notify (obj, "choice-visible");

	return TRUE;
}

static void
on_responded (GcrSystemPrompter *prompter,
              gpointer user_data)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);
	gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
	gtk_widget_hide (GTK_WIDGET (self->image));
	gtk_widget_show (GTK_WIDGET (self->spinner));
	self->mode = PROMPT_NONE;
}

static const gchar *
grab_status_message (GdkGrabStatus status)
{
	switch (status) {
	case GDK_GRAB_SUCCESS:
		g_return_val_if_reached ("");
	case GDK_GRAB_ALREADY_GRABBED:
		return "already grabbed";
	case GDK_GRAB_INVALID_TIME:
		return "invalid time";
	case GDK_GRAB_NOT_VIEWABLE:
		return "not viewable";
	case GDK_GRAB_FROZEN:
		return "frozen";
	default:
		g_message ("unknown grab status: %d", (int)status);
		return "unknown";
	}
}

static gboolean
on_grab_broken (GtkWidget *widget,
                GdkEventGrabBroken *event,
                gpointer user_data)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);
	if (self->grabbed_device && event->keyboard)
		self->grabbed_device = NULL;
	return TRUE;
}

static gboolean
grab_keyboard (GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);
	GdkGrabStatus status;
	guint32 at;

	GdkDevice *device = NULL;
	GdkDeviceManager *manager;
	GdkDisplay *display;
	GList *devices, *l;

	if (self->grabbed_device || !GRAB_KEYBOARD)
		return FALSE;

	display = gtk_widget_get_display (widget);
	manager = gdk_display_get_device_manager (display);
	devices = gdk_device_manager_list_devices (manager, GDK_DEVICE_TYPE_MASTER);
	for (l = devices; l; l = g_list_next (l)) {
		device = l->data;
		if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
			break;
	}
	g_list_free (devices);

	if (!device) {
		g_message ("couldn't find device to grab");
		return FALSE;
	}

	at = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
	status = gdk_device_grab (device, gtk_widget_get_window (widget),
	                          GDK_OWNERSHIP_WINDOW, TRUE,
	                          GDK_KEY_PRESS | GDK_KEY_RELEASE, NULL, at);
	if (status == GDK_GRAB_SUCCESS) {
		self->grab_broken_id = g_signal_connect (widget, "grab-broken-event",
		                                         G_CALLBACK (on_grab_broken), self);
		gtk_device_grab_add (widget, device, TRUE);
		self->grabbed_device = device;
	} else {
		g_message ("could not grab keyboard: %s", grab_status_message (status));
	}

	/* Always return false, so event is handled elsewhere */
	return FALSE;
}


static gboolean
ungrab_keyboard (GtkWidget *widget,
                 GdkEvent *event,
                 gpointer user_data)
{
	guint32 at = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (user_data);

	if (self->grabbed_device) {
		g_signal_handler_disconnect (widget, self->grab_broken_id);
		gdk_device_ungrab (self->grabbed_device, at);
		gtk_device_grab_remove (widget, self->grabbed_device);
		self->grabbed_device = NULL;
		self->grab_broken_id = 0;
	}

	/* Always return false, so event is handled elsewhere */
	return FALSE;
}

static gboolean
window_state_changed (GtkWidget *widget,
                      GdkEventWindowState *event,
                      gpointer user_data)
{
	GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (widget));

	if (state & GDK_WINDOW_STATE_WITHDRAWN ||
	    state & GDK_WINDOW_STATE_ICONIFIED ||
	    state & GDK_WINDOW_STATE_FULLSCREEN ||
	    state & GDK_WINDOW_STATE_MAXIMIZED)
		ungrab_keyboard (widget, (GdkEvent*)event, user_data);
	else
		grab_keyboard (widget, (GdkEvent*)event, user_data);

	return FALSE;
}

static void
on_password_changed (GtkEditable *editable,
                     gpointer user_data)
{
	int upper, lower, digit, misc;
	const char *password;
	gdouble pwstrength;
	int length, i;

	password = gtk_entry_get_text (GTK_ENTRY (editable));

	/*
	 * This code is based on the Master Password dialog in Firefox
	 * (pref-masterpass.js)
	 * Original code triple-licensed under the MPL, GPL, and LGPL
	 * so is license-compatible with this file
	 */

	length = strlen (password);
	upper = 0;
	lower = 0;
	digit = 0;
	misc = 0;

	for ( i = 0; i < length ; i++) {
		if (g_ascii_isdigit (password[i]))
			digit++;
		else if (g_ascii_islower (password[i]))
			lower++;
		else if (g_ascii_isupper (password[i]))
			upper++;
		else
			misc++;
	}

	if (length > 5)
		length = 5;
	if (digit > 3)
		digit = 3;
	if (upper > 3)
		upper = 3;
	if (misc > 3)
		misc = 3;

	pwstrength = ((length * 0.1) - 0.2) +
	              (digit * 0.1) +
	              (misc * 0.15) +
	              (upper * 0.1);

	if (pwstrength < 0.0)
		pwstrength = 0.0;
	if (pwstrength > 1.0)
		pwstrength = 1.0;

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (user_data), pwstrength);
}

static void
handle_password_response (GcrPrompterDialog *self)
{
	const gchar *password;
	const gchar *confirm;
	const gchar *env;
	gint strength;

	password = gtk_entry_buffer_get_text (self->password_buffer);

	/* Is it a new password? */
	if (gcr_system_prompter_get_password_new (the_prompter)) {
		confirm = gtk_entry_buffer_get_text (self->confirm_buffer);

		/* Do the passwords match? */
		if (!g_str_equal (password, confirm)) {
			gcr_system_prompter_set_warning (the_prompter, _("Passwords do not match."));
			return;
		}

		/* Don't allow blank passwords if in paranoid mode */
		env = g_getenv ("GNOME_KEYRING_PARANOID");
		if (env && *env) {
			gcr_system_prompter_set_warning (the_prompter, _("Password cannot be blank"));
			return;
		}
	}

	if (g_str_equal (password, ""))
		strength = 0;
	else
		strength = 1;
	gcr_system_prompter_set_password_strength (the_prompter, strength);

	gcr_system_prompter_respond_with_password (the_prompter, password);
}

static void
gcr_prompter_dialog_realize (GtkWidget *widget)
{
	gboolean modal = FALSE;
	const gchar *value;
	gulong caller_window_id;
	GdkWindow *caller_window;
	GdkWindow *self_window;
	GdkDisplay *display;
	gchar *end;

	GTK_WIDGET_CLASS (gcr_prompter_dialog_parent_class)->realize (widget);

	value= gcr_system_prompter_get_caller_window (the_prompter);
	if (value) {
		caller_window_id = strtoul (value, &end, 10);
		if (caller_window_id && end && end[0] == '\0') {
			display = gtk_widget_get_display (widget);
			caller_window = gdk_x11_window_foreign_new_for_display (display, caller_window_id);
			if (caller_window) {
				self_window = gtk_widget_get_window (widget);
				gdk_window_set_transient_for (self_window, caller_window);
				g_object_unref (caller_window);
				modal = TRUE;
			}
		}
	}

	gtk_window_set_modal (GTK_WINDOW (widget), modal);
}

static void
gcr_prompter_dialog_response (GtkDialog *dialog,
                              gint response_id)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (dialog);

	if (response_id != GTK_RESPONSE_OK)
		gcr_system_prompter_respond_cancelled (the_prompter);

	else if (self->mode == PROMPT_PASSWORDING)
		handle_password_response (self);

	else if (self->mode == PROMPT_CONFIRMING)
		gcr_system_prompter_respond_confirmed (the_prompter);

	else
		g_return_if_reached ();
}

static void
gcr_prompter_dialog_init (GcrPrompterDialog *self)
{
	PangoAttrList *attrs;
	GtkDialog *dialog;
	GtkWidget *widget;
	GtkWidget *entry;
	GtkWidget *content;
	GtkGrid *grid;

	g_assert (GCR_IS_SYSTEM_PROMPTER (the_prompter));

	g_signal_connect (the_prompter, "open", G_CALLBACK (on_open_prompt), self);
	g_signal_connect (the_prompter, "prompt-password", G_CALLBACK (on_prompt_password), self);
	g_signal_connect (the_prompter, "prompt-confirm", G_CALLBACK (on_prompt_confirm), self);
	g_signal_connect (the_prompter, "responded", G_CALLBACK (on_responded), self);
	g_signal_connect (the_prompter, "close", G_CALLBACK (on_close_prompt), self);

	dialog = GTK_DIALOG (self);
	gtk_dialog_add_buttons (dialog,
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                        _("Continue"), GTK_RESPONSE_OK,
	                        NULL);

	content = gtk_dialog_get_content_area (dialog);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_container_set_border_width (GTK_CONTAINER (grid), 6);
	gtk_widget_set_hexpand (GTK_WIDGET (grid), TRUE);
	gtk_grid_set_column_spacing (grid, 12);
	gtk_grid_set_row_spacing (grid, 6);

	/* The prompt image */
	self->image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
	                                        GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_valign (self->image, GTK_ALIGN_START);
	gtk_grid_attach (grid, self->image, -1, 0, 1, 4);
	gtk_widget_show (self->image);

	/* The prompt spinner */
	self->spinner = gtk_spinner_new ();
	gtk_widget_set_valign (self->image, GTK_ALIGN_START);
	gtk_grid_attach (grid, self->spinner, -2, -1, 1, 4);
	gtk_widget_show (self->spinner);

	/* The message label */
	widget = gtk_label_new ("");
	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_LARGE));
	gtk_label_set_attributes (GTK_LABEL (widget), attrs);
	pango_attr_list_unref (attrs);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_bottom (widget, 8);
	g_object_bind_property (the_prompter, "message", widget, "label", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 0, 2, 1);
	gtk_widget_show (widget);

	/* The description label */
	widget = gtk_label_new ("");
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_bottom (widget, 4);
	g_object_bind_property (the_prompter, "description", widget, "label", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);
	gtk_widget_show (widget);

	/* The password label */
	widget = gtk_label_new (_("Password:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	g_object_bind_property (self, "password-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 2, 1, 1);

	/* The password entry */
	self->password_buffer = gcr_secure_entry_buffer_new ();
	entry = gtk_entry_new_with_buffer (self->password_buffer);
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
	gtk_widget_set_hexpand (widget, TRUE);
	g_object_bind_property (self, "password-visible", entry, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, entry, 1, 2, 1, 1);

	/* The confirm label */
	widget = gtk_label_new (_("Confirm:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	g_object_bind_property (self, "confirm-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 3, 1, 1);

	/* The confirm entry */
	self->confirm_buffer = gcr_secure_entry_buffer_new ();
	widget = gtk_entry_new_with_buffer (self->password_buffer);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);
	g_object_bind_property (self, "confirm-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 1, 3, 1, 1);

	/* The quality progress bar */
	widget = gtk_progress_bar_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	g_object_bind_property (self, "confirm-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 1, 4, 1, 1);
	g_signal_connect (entry, "changed", G_CALLBACK (on_password_changed), widget);

	/* The warning */
	widget = gtk_label_new ("");
	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_style_new (PANGO_STYLE_ITALIC));
	gtk_label_set_attributes (GTK_LABEL (widget), attrs);
	pango_attr_list_unref (attrs);
	g_object_bind_property (the_prompter, "warning", widget, "label", G_BINDING_DEFAULT);
	g_object_bind_property (self, "warning-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 5, 2, 1);
	gtk_widget_show (widget);

	/* The checkbox */
	widget = gtk_check_button_new ();
	g_object_bind_property (the_prompter, "choice-label", widget, "label", G_BINDING_DEFAULT);
	g_object_bind_property (self, "choice-visible", widget, "visible", G_BINDING_DEFAULT);
	g_object_bind_property (the_prompter, "choice-chosen", widget, "active", G_BINDING_BIDIRECTIONAL);
	gtk_grid_attach (grid, widget, 0, 6, 2, 1);

	gtk_container_add (GTK_CONTAINER (content), GTK_WIDGET (grid));
	gtk_widget_show (GTK_WIDGET (grid));

	g_signal_connect (self, "map-event", G_CALLBACK (grab_keyboard), self);
	g_signal_connect (self, "unmap-event", G_CALLBACK (ungrab_keyboard), self);
	g_signal_connect (self, "window-state-event", G_CALLBACK (window_state_changed), self);
}

static void
gcr_prompter_dialog_get_property (GObject *obj,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	GcrPrompterDialog *self = GCR_PROMPTER_DIALOG (obj);
	const gchar *string;

	switch (prop_id)
	{
	case PROP_PASSWORD_VISIBLE:
		g_value_set_boolean (value, self->mode == PROMPT_PASSWORDING);
		break;
	case PROP_CONFIRM_VISIBLE:
		g_value_set_boolean (value, gcr_system_prompter_get_password_new (the_prompter) &&
		                            self->mode == PROMPT_PASSWORDING);
		break;
	case PROP_WARNING_VISIBLE:
		string = gcr_system_prompter_get_warning (the_prompter);
		g_value_set_boolean (value, string && string[0]);
		break;
	case PROP_CHOICE_VISIBLE:
		string = gcr_system_prompter_get_choice_label (the_prompter);
		g_value_set_boolean (value, string && string[0]);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_prompter_dialog_finalize (GObject *obj)
{
	G_OBJECT_GET_CLASS (gcr_prompter_dialog_parent_class)->finalize (obj);
}

static void
gcr_prompter_dialog_class_init (GcrPrompterDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->realize = gcr_prompter_dialog_realize;

	gobject_class->finalize = gcr_prompter_dialog_finalize;
	gobject_class->get_property = gcr_prompter_dialog_get_property;

	dialog_class->response = gcr_prompter_dialog_response;

	g_object_class_install_property (gobject_class, PROP_PASSWORD_VISIBLE,
	           g_param_spec_boolean ("password-visible", "", "",
	                                 FALSE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_CONFIRM_VISIBLE,
	           g_param_spec_boolean ("confirm-visible", "", "",
	                                 FALSE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_WARNING_VISIBLE,
	           g_param_spec_boolean ("warning-visible", "", "",
	                                 FALSE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_CHOICE_VISIBLE,
	           g_param_spec_boolean ("choice-visible", "", "",
	                                 FALSE, G_PARAM_READABLE));
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
	gcr_system_prompter_register (the_prompter, connection);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{

}

static void
on_name_lost (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	GtkDialog *dialog;
	guint owner_id;

	g_type_init ();
	gtk_init (&argc, &argv);

#ifdef HAVE_LOCALE_H
	/* internationalisation */
	setlocale (LC_ALL, "");
#endif

#ifdef HAVE_GETTEXT
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

	the_prompter = gcr_system_prompter_new ();
	dialog = g_object_new (GCR_TYPE_PROMPTER_DIALOG, NULL);
	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
	                           GCR_DBUS_PROMPTER_BUS_NAME,
	                           G_BUS_NAME_OWNER_FLAGS_NONE,
	                           on_bus_acquired,
	                           on_name_acquired,
	                           on_name_lost,
	                           NULL,
	                           NULL);

	gtk_main ();

	g_bus_unown_name (owner_id);
	g_object_unref (the_prompter);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	return 0;
}
