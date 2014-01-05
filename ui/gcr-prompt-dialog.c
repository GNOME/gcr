/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Stefan Walter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stef@thewalter.net>
 */

#include "config.h"

#define DEBUG_FLAG GCR_DEBUG_PROMPT
#include "gcr/gcr-debug.h"
#include "gcr/gcr-prompt.h"

#include "gcr-prompt-dialog.h"
#include "gcr-secure-entry-buffer.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>

/**
 * SECTION:gcr-prompt-dialog
 * @title: GcrPromptDialog
 * @short_description: a GTK+ dialog prompt
 *
 * A #GcrPrompt implementation which shows a GTK+ dialog. The dialog will
 * remain visible (but insensitive) between prompts. If the user cancels the
 * dialog between prompts, then the dialog will be hidden.
 */

/**
 * GcrPromptDialog:
 *
 * A #GcrPrompt implementation which shows a GTK+ dialog.
 */

/**
 * GcrPromptDialogClass:
 * @parent_class: parent class
 *
 * The class for #GcrPromptDialog.
 */

#ifdef GCR_DISABLE_GRABS
#define GRAB_KEYBOARD 0
#else
#define GRAB_KEYBOARD 1
#endif

typedef enum {
	PROMPT_NONE,
	PROMPT_CONFIRMING,
	PROMPT_PASSWORDING
} PromptMode;

enum {
	PROP_0,
	PROP_MESSAGE,
	PROP_DESCRIPTION,
	PROP_WARNING,
	PROP_CHOICE_LABEL,
	PROP_CHOICE_CHOSEN,
	PROP_PASSWORD_NEW,
	PROP_PASSWORD_STRENGTH,
	PROP_CALLER_WINDOW,
	PROP_CONTINUE_LABEL,
	PROP_CANCEL_LABEL,

	PROP_PASSWORD_VISIBLE,
	PROP_CONFIRM_VISIBLE,
	PROP_WARNING_VISIBLE,
	PROP_CHOICE_VISIBLE,
};

struct _GcrPromptDialogPrivate {
	gchar *title;
	gchar *message;
	gchar *description;
	gchar *warning;
	gchar *choice_label;
	gboolean choice_chosen;
	gboolean password_new;
	guint password_strength;
	gchar *caller_window;
	gchar *continue_label;
	gchar *cancel_label;

	GSimpleAsyncResult *async_result;
	GcrPromptReply last_reply;
	GtkWidget *widget_grid;
	GtkWidget *continue_button;
	GtkWidget *spinner;
	GtkWidget *image;
	GtkWidget *password_entry;
	GtkEntryBuffer *password_buffer;
	GtkEntryBuffer *confirm_buffer;
	PromptMode mode;
	GdkDevice *grabbed_device;
	gulong grab_broken_id;
	gboolean grab_disabled;
	gboolean was_closed;
};

static void     gcr_prompt_dialog_prompt_iface       (GcrPromptIface *iface);

static gboolean ungrab_keyboard                      (GtkWidget *win,
                                                      GdkEvent *event,
                                                      gpointer unused);

G_DEFINE_TYPE_WITH_CODE (GcrPromptDialog, gcr_prompt_dialog, GTK_TYPE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_PROMPT, gcr_prompt_dialog_prompt_iface);
);

static void
update_transient_for (GcrPromptDialog *self)
{
	GdkDisplay *display;
	GdkWindow *transient_for;
	GdkWindow *window;
	gint64 handle;
	gchar *end;

	if (self->pv->caller_window == NULL || g_str_equal (self->pv->caller_window, "")) {
		gtk_window_set_modal (GTK_WINDOW (self), FALSE);
		return;
	}

	window = gtk_widget_get_window (GTK_WIDGET (self));
	if (window == NULL)
		return;

	handle = g_ascii_strtoll (self->pv->caller_window, &end, 10);
	if (!end || *end != '\0') {
		g_warning ("couldn't parse caller-window property: %s", self->pv->caller_window);
		return;
	}

	display = gtk_widget_get_display (GTK_WIDGET (self));
	transient_for = gdk_x11_window_foreign_new_for_display (display, (Window)handle);
	if (transient_for == NULL) {
		g_warning ("caller-window property doesn't represent a window on current display: %s",
		           self->pv->caller_window);
	} else {
		gdk_window_set_transient_for (window, transient_for);
		g_object_unref (transient_for);
	}

	gtk_window_set_modal (GTK_WINDOW (self), TRUE);
}

static void
gcr_prompt_dialog_init (GcrPromptDialog *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, GCR_TYPE_PROMPT_DIALOG,
	                                        GcrPromptDialogPrivate);

	/*
	 * This is a stupid hack to work around to help the window act like
	 * a normal object with regards to reference counting and unref.
	 */
	gtk_window_set_has_user_ref_count (GTK_WINDOW (self), FALSE);
}

static void
gcr_prompt_dialog_set_property (GObject *obj,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (obj);

	switch (prop_id) {
	case PROP_MESSAGE:
		g_free (self->pv->message);
		self->pv->message = g_value_dup_string (value);
		g_object_notify (obj, "message");
		break;
	case PROP_DESCRIPTION:
		g_free (self->pv->description);
		self->pv->description = g_value_dup_string (value);
		g_object_notify (obj, "description");
		break;
	case PROP_WARNING:
		g_free (self->pv->warning);
		self->pv->warning = g_value_dup_string (value);
		if (self->pv->warning && self->pv->warning[0] == '\0') {
			g_free (self->pv->warning);
			self->pv->warning = NULL;
		}
		g_object_notify (obj, "warning");
		g_object_notify (obj, "warning-visible");
		break;
	case PROP_CHOICE_LABEL:
		g_free (self->pv->choice_label);
		self->pv->choice_label = g_value_dup_string (value);
		if (self->pv->choice_label && self->pv->choice_label[0] == '\0') {
			g_free (self->pv->choice_label);
			self->pv->choice_label = NULL;
		}
		g_object_notify (obj, "choice-label");
		g_object_notify (obj, "choice-visible");
		break;
	case PROP_CHOICE_CHOSEN:
		self->pv->choice_chosen = g_value_get_boolean (value);
		g_object_notify (obj, "choice-chosen");
		break;
	case PROP_PASSWORD_NEW:
		self->pv->password_new = g_value_get_boolean (value);
		g_object_notify (obj, "password-new");
		g_object_notify (obj, "confirm-visible");
		break;
	case PROP_CALLER_WINDOW:
		g_free (self->pv->caller_window);
		self->pv->caller_window = g_value_dup_string (value);
		if (self->pv->caller_window && self->pv->caller_window[0] == '\0') {
			g_free (self->pv->caller_window);
			self->pv->caller_window = NULL;
		}
		update_transient_for (self);
		g_object_notify (obj, "caller-window");
		break;
	case PROP_CONTINUE_LABEL:
		g_free (self->pv->continue_label);
		self->pv->continue_label = g_value_dup_string (value);
		g_object_notify (obj, "continue-label");
		break;
	case PROP_CANCEL_LABEL:
		g_free (self->pv->cancel_label);
		self->pv->cancel_label = g_value_dup_string (value);
		g_object_notify (obj, "cancel-label");
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_prompt_dialog_get_property (GObject *obj,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (obj);

	switch (prop_id) {
	case PROP_MESSAGE:
		g_value_set_string (value, self->pv->message);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, self->pv->description);
		break;
	case PROP_WARNING:
		g_value_set_string (value, self->pv->warning);
		break;
	case PROP_CHOICE_LABEL:
		g_value_set_string (value, self->pv->choice_label);
		break;
	case PROP_CHOICE_CHOSEN:
		g_value_set_boolean (value, self->pv->choice_chosen);
		break;
	case PROP_PASSWORD_NEW:
		g_value_set_boolean (value, self->pv->password_new);
		break;
	case PROP_PASSWORD_STRENGTH:
		g_value_set_int (value, self->pv->password_strength);
		break;
	case PROP_CALLER_WINDOW:
		g_value_set_string (value, self->pv->caller_window);
		break;
	case PROP_PASSWORD_VISIBLE:
		g_value_set_boolean (value, self->pv->mode == PROMPT_PASSWORDING);
		break;
	case PROP_CONFIRM_VISIBLE:
		g_value_set_boolean (value, self->pv->password_new &&
		                            self->pv->mode == PROMPT_PASSWORDING);
		break;
	case PROP_WARNING_VISIBLE:
		g_value_set_boolean (value, self->pv->warning && self->pv->warning[0]);
		break;
	case PROP_CHOICE_VISIBLE:
		g_value_set_boolean (value, self->pv->choice_label && self->pv->choice_label[0]);
		break;
	case PROP_CONTINUE_LABEL:
		g_value_set_string (value, self->pv->continue_label);
		break;
	case PROP_CANCEL_LABEL:
		g_value_set_string (value, self->pv->cancel_label);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
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


static const gchar*
grab_status_message (GdkGrabStatus status)
{
	switch (status) {
	case GDK_GRAB_SUCCESS:
		g_return_val_if_reached ("");
		break;
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
                GdkEventGrabBroken * event,
                gpointer user_data)
{
	ungrab_keyboard (widget, (GdkEvent *)event, user_data);
	return TRUE;
}

static gboolean
grab_keyboard (GtkWidget *widget,
               GdkEvent *event,
               gpointer user_data)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (user_data);
	GdkGrabStatus status;
	guint32 at;
	GdkDevice *device = NULL;
	GdkDeviceManager *manager;
	GdkDisplay *display;
	GList *devices, *l;

	if (self->pv->grabbed_device || !GRAB_KEYBOARD)
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
	                          GDK_OWNERSHIP_APPLICATION, TRUE,
	                          GDK_KEY_PRESS | GDK_KEY_RELEASE, NULL, at);
	if (status == GDK_GRAB_SUCCESS) {
		self->pv->grab_broken_id = g_signal_connect (widget, "grab-broken-event",
		                                             G_CALLBACK (on_grab_broken), self);
		gtk_device_grab_add (widget, device, TRUE);
		self->pv->grabbed_device = device;
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
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (user_data);

	if (self->pv->grabbed_device) {
		g_signal_handler_disconnect (widget, self->pv->grab_broken_id);
		gdk_device_ungrab (self->pv->grabbed_device, at);
		gtk_device_grab_remove (widget, self->pv->grabbed_device);
		self->pv->grabbed_device = NULL;
		self->pv->grab_broken_id = 0;
	}

	/* Always return false, so event is handled elsewhere */
	return FALSE;
}

static gboolean
window_state_changed (GtkWidget *win, GdkEventWindowState *event, gpointer data)
{
	GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (win));
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (data);

	if (state & GDK_WINDOW_STATE_WITHDRAWN ||
	    state & GDK_WINDOW_STATE_ICONIFIED ||
	    state & GDK_WINDOW_STATE_FULLSCREEN ||
	    state & GDK_WINDOW_STATE_MAXIMIZED) {
		self->pv->grab_disabled = TRUE;
		ungrab_keyboard (win, (GdkEvent*)event, data);
	} else if (self->pv->grab_disabled) {
		self->pv->grab_disabled = FALSE;
		grab_keyboard (win, (GdkEvent*)event, data);
	}

	return FALSE;
}

static void
gcr_prompt_dialog_constructed (GObject *obj)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (obj);
	GtkDialog *dialog;
	PangoAttrList *attrs;
	GtkWidget *widget;
	GtkWidget *entry;
	GtkWidget *content;
	GtkWidget *button;
	GtkGrid *grid;

	G_OBJECT_CLASS (gcr_prompt_dialog_parent_class)->constructed (obj);

	dialog = GTK_DIALOG (self);
	button = gtk_dialog_add_button (dialog, _("_Cancel"), GTK_RESPONSE_CANCEL);
	g_object_bind_property (self, "cancel-label", button, "label", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	button = gtk_dialog_add_button (dialog, _("_OK"), GTK_RESPONSE_OK);
	g_object_bind_property (self, "continue-label", button, "label", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	self->pv->continue_button = button;

	gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

	content = gtk_dialog_get_content_area (dialog);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_container_set_border_width (GTK_CONTAINER (grid), 6);
	gtk_widget_set_hexpand (GTK_WIDGET (grid), TRUE);
	gtk_grid_set_column_homogeneous (grid, FALSE);
	gtk_grid_set_column_spacing (grid, 12);
	gtk_grid_set_row_spacing (grid, 6);

	/* The prompt image */
	self->pv->image = gtk_image_new_from_icon_name ("dialog-password", GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_valign (self->pv->image, GTK_ALIGN_START);
	gtk_grid_attach (grid, self->pv->image, -1, 0, 1, 4);
	gtk_widget_show (self->pv->image);

	/* The prompt spinner on the continue button */
	widget = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
	                                             GTK_RESPONSE_OK);
	self->pv->spinner = gtk_spinner_new ();
	gtk_button_set_image (GTK_BUTTON (widget), self->pv->spinner);
	gtk_button_set_image_position (GTK_BUTTON (widget), GTK_POS_LEFT);

	/* The message label */
	widget = gtk_label_new ("");
	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_LARGE));
	gtk_label_set_attributes (GTK_LABEL (widget), attrs);
	pango_attr_list_unref (attrs);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_bottom (widget, 8);
	g_object_bind_property (self, "message", widget, "label", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 0, 2, 1);
	gtk_widget_show (widget);

	/* The description label */
	widget = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_margin_bottom (widget, 4);
	g_object_bind_property (self, "description", widget, "label", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);
	gtk_widget_show (widget);

	/* The password label */
	widget = gtk_label_new (_("Password:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, FALSE);
	g_object_bind_property (self, "password-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 2, 1, 1);

	/* The password entry */
	self->pv->password_buffer = gcr_secure_entry_buffer_new ();
	entry = gtk_entry_new_with_buffer (self->pv->password_buffer);
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_set_hexpand (entry, TRUE);
	g_object_bind_property (self, "password-visible", entry, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, entry, 1, 2, 1, 1);
	self->pv->password_entry = entry;

	/* The confirm label */
	widget = gtk_label_new (_("Confirm:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, FALSE);
	g_object_bind_property (self, "confirm-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 3, 1, 1);

	/* The confirm entry */
	self->pv->confirm_buffer = gcr_secure_entry_buffer_new ();
	widget = gtk_entry_new_with_buffer (self->pv->confirm_buffer);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
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
	gtk_widget_set_hexpand (widget, FALSE);
	g_object_bind_property (self, "warning", widget, "label", G_BINDING_DEFAULT);
	g_object_bind_property (self, "warning-visible", widget, "visible", G_BINDING_DEFAULT);
	gtk_grid_attach (grid, widget, 0, 5, 2, 1);
	gtk_widget_show (widget);

	/* The checkbox */
	widget = gtk_check_button_new ();
	g_object_bind_property (self, "choice-label", widget, "label", G_BINDING_DEFAULT);
	g_object_bind_property (self, "choice-visible", widget, "visible", G_BINDING_DEFAULT);
	g_object_bind_property (self, "choice-chosen", widget, "active", G_BINDING_BIDIRECTIONAL);
	gtk_widget_set_hexpand (widget, FALSE);
	gtk_grid_attach (grid, widget, 0, 6, 2, 1);

	gtk_container_add (GTK_CONTAINER (content), GTK_WIDGET (grid));
	gtk_widget_show (GTK_WIDGET (grid));
	self->pv->widget_grid = GTK_WIDGET (grid);

	g_signal_connect (self, "map-event", G_CALLBACK (grab_keyboard), self);
	g_signal_connect (self, "unmap-event", G_CALLBACK (ungrab_keyboard), self);
	g_signal_connect (self, "window-state-event", G_CALLBACK (window_state_changed), self);

}

static gboolean
handle_password_response (GcrPromptDialog *self)
{
	const gchar *password;
	const gchar *confirm;
	const gchar *env;
	gint strength;

	password = gtk_entry_buffer_get_text (self->pv->password_buffer);

	/* Is it a new password? */
	if (self->pv->password_new) {
		confirm = gtk_entry_buffer_get_text (self->pv->confirm_buffer);

		/* Do the passwords match? */
		if (!g_str_equal (password, confirm)) {
			gcr_prompt_set_warning (GCR_PROMPT (self), _("Passwords do not match."));
			return FALSE;
		}

		/* Don't allow blank passwords if in paranoid mode */
		env = g_getenv ("GNOME_KEYRING_PARANOID");
		if (env && *env) {
			gcr_prompt_set_warning (GCR_PROMPT (self), _("Password cannot be blank"));
			return FALSE;
		}
	}

	if (g_str_equal (password, ""))
		strength = 0;
	else
		strength = 1;

	self->pv->password_strength = strength;
	g_object_notify (G_OBJECT (self), "password-strength");
	return TRUE;
}

static void
gcr_prompt_dialog_response (GtkDialog *dialog,
                            gint response_id)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (dialog);
	GSimpleAsyncResult *res;

	/*
	 * If this is called while no prompting is going on, then the dialog
	 * is waiting for the caller to perform some action. Close the dialog.
	 */

	if (self->pv->mode == PROMPT_NONE) {
		g_return_if_fail (response_id != GTK_RESPONSE_OK);
		gcr_prompt_close (GCR_PROMPT (self));
		return;
	}

	switch (response_id) {
	case GTK_RESPONSE_OK:
		switch (self->pv->mode) {
		case PROMPT_PASSWORDING:
			if (!handle_password_response (self))
				return;
			break;
		default:
			break;
		}
		self->pv->last_reply = GCR_PROMPT_REPLY_CONTINUE;
		break;

	default:
		self->pv->last_reply = GCR_PROMPT_REPLY_CANCEL;
		break;
	}

	gtk_widget_set_sensitive (self->pv->continue_button, FALSE);
	gtk_widget_set_sensitive (self->pv->widget_grid, FALSE);
	gtk_widget_show (self->pv->spinner);
	gtk_spinner_start (GTK_SPINNER (self->pv->spinner));
	self->pv->mode = PROMPT_NONE;

	res = self->pv->async_result;
	self->pv->async_result = NULL;

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

static void
gcr_prompt_dialog_dispose (GObject *obj)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (obj);

	gcr_prompt_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_DELETE_EVENT);
	g_assert (self->pv->async_result == NULL);

	gcr_prompt_close (GCR_PROMPT (self));

	ungrab_keyboard (GTK_WIDGET (self), NULL, self);
	g_assert (self->pv->grabbed_device == NULL);

	G_OBJECT_CLASS (gcr_prompt_dialog_parent_class)->dispose (obj);
}

static void
gcr_prompt_dialog_finalize (GObject *obj)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (obj);

	g_free (self->pv->title);
	g_free (self->pv->message);
	g_free (self->pv->description);
	g_free (self->pv->warning);
	g_free (self->pv->choice_label);
	g_free (self->pv->caller_window);

	g_object_unref (self->pv->password_buffer);
	g_object_unref (self->pv->confirm_buffer);

	G_OBJECT_CLASS (gcr_prompt_dialog_parent_class)->finalize (obj);
}

static void
gcr_prompt_dialog_class_init (GcrPromptDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	gobject_class->constructed = gcr_prompt_dialog_constructed;
	gobject_class->get_property = gcr_prompt_dialog_get_property;
	gobject_class->set_property = gcr_prompt_dialog_set_property;
	gobject_class->dispose = gcr_prompt_dialog_dispose;
	gobject_class->finalize = gcr_prompt_dialog_finalize;

	dialog_class->response = gcr_prompt_dialog_response;

	g_type_class_add_private (gobject_class, sizeof (GcrPromptDialogPrivate));

	g_object_class_override_property (gobject_class, PROP_MESSAGE, "message");

	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");

	g_object_class_override_property (gobject_class, PROP_WARNING, "warning");

	g_object_class_override_property (gobject_class, PROP_PASSWORD_NEW, "password-new");

	g_object_class_override_property (gobject_class, PROP_PASSWORD_STRENGTH, "password-strength");

	g_object_class_override_property (gobject_class, PROP_CHOICE_LABEL, "choice-label");

	g_object_class_override_property (gobject_class, PROP_CHOICE_CHOSEN, "choice-chosen");

	g_object_class_override_property (gobject_class, PROP_CALLER_WINDOW, "caller-window");

	g_object_class_override_property (gobject_class, PROP_CONTINUE_LABEL, "continue-label");

	g_object_class_override_property (gobject_class, PROP_CANCEL_LABEL, "cancel-label");

	/**
	 * GcrPromptDialog:password-visible:
	 *
	 * Whether the password entry is visible or not.
	 */
	g_object_class_install_property (gobject_class, PROP_PASSWORD_VISIBLE,
	           g_param_spec_boolean ("password-visible", "Password visible", "Password field is visible",
	                                 FALSE, G_PARAM_READABLE));

	/**
	 * GcrPromptDialog:confirm-visible:
	 *
	 * Whether the password confirm entry is visible or not.
	 */
	g_object_class_install_property (gobject_class, PROP_CONFIRM_VISIBLE,
	           g_param_spec_boolean ("confirm-visible", "Confirm visible", "Confirm field is visible",
	                                 FALSE, G_PARAM_READABLE));

	/**
	 * GcrPromptDialog:warning-visible:
	 *
	 * Whether the warning label is visible or not.
	 */
	g_object_class_install_property (gobject_class, PROP_WARNING_VISIBLE,
	           g_param_spec_boolean ("warning-visible", "Warning visible", "Warning is visible",
	                                 FALSE, G_PARAM_READABLE));

	/**
	 * GcrPromptDialog:choice-visible:
	 *
	 * Whether the choice check box is visible or not.
	 */
	g_object_class_install_property (gobject_class, PROP_CHOICE_VISIBLE,
	           g_param_spec_boolean ("choice-visible", "Choice visible", "Choice is visible",
	                                 FALSE, G_PARAM_READABLE));
}

static void
gcr_prompt_dialog_password_async (GcrPrompt *prompt,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (prompt);
	GObject *obj;

	if (self->pv->async_result != NULL) {
		g_warning ("this prompt is already prompting");
		return;
	}

	self->pv->mode = PROMPT_PASSWORDING;
	self->pv->async_result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                                    gcr_prompt_dialog_password_async);

	gtk_entry_buffer_set_text (self->pv->password_buffer, "", 0);
	gtk_entry_buffer_set_text (self->pv->confirm_buffer, "", 0);

	if (self->pv->was_closed) {
		self->pv->last_reply = GCR_PROMPT_REPLY_CANCEL;
		g_simple_async_result_complete_in_idle (self->pv->async_result);
		return;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (self->pv->image),
	                              "dialog-password", GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_sensitive (self->pv->continue_button, TRUE);
	gtk_widget_set_sensitive (self->pv->widget_grid, TRUE);
	gtk_widget_hide (self->pv->spinner);
	gtk_spinner_stop (GTK_SPINNER (self->pv->spinner));

	obj = G_OBJECT (self);
	g_object_notify (obj, "password-visible");
	g_object_notify (obj, "confirm-visible");
	g_object_notify (obj, "warning-visible");
	g_object_notify (obj, "choice-visible");

	gtk_widget_grab_focus (self->pv->password_entry);
	gtk_widget_show (GTK_WIDGET (self));
}

static const gchar *
gcr_prompt_dialog_password_finish (GcrPrompt *prompt,
                                   GAsyncResult *result,
                                   GError **error)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (prompt);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
	                      gcr_prompt_dialog_password_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	if (self->pv->last_reply == GCR_PROMPT_REPLY_CONTINUE)
		return gtk_entry_buffer_get_text (self->pv->password_buffer);
	return NULL;
}

static void
gcr_prompt_dialog_confirm_async (GcrPrompt *prompt,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (prompt);
	GtkWidget *button;
	GObject *obj;

	if (self->pv->async_result != NULL) {
		g_warning ("this prompt is already prompting");
		return;
	}

	self->pv->mode = PROMPT_CONFIRMING;
	self->pv->async_result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                                    gcr_prompt_dialog_confirm_async);

	if (self->pv->was_closed) {
		self->pv->last_reply = GCR_PROMPT_REPLY_CANCEL;
		g_simple_async_result_complete_in_idle (self->pv->async_result);
		return;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (self->pv->image),
	                              "dialog-question", GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_sensitive (self->pv->continue_button, TRUE);
	gtk_widget_set_sensitive (self->pv->widget_grid, TRUE);
	gtk_widget_hide (self->pv->spinner);
	gtk_spinner_stop (GTK_SPINNER (self->pv->spinner));

	button = gtk_dialog_get_widget_for_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
	gtk_widget_grab_focus (button);

	obj = G_OBJECT (self);
	g_object_notify (obj, "password-visible");
	g_object_notify (obj, "confirm-visible");
	g_object_notify (obj, "warning-visible");
	g_object_notify (obj, "choice-visible");

	gtk_widget_show (GTK_WIDGET (self));
}

static GcrPromptReply
gcr_prompt_dialog_confirm_finish (GcrPrompt *prompt,
                                  GAsyncResult *result,
                                  GError **error)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (prompt);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
	                      gcr_prompt_dialog_confirm_async), GCR_PROMPT_REPLY_CANCEL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return GCR_PROMPT_REPLY_CANCEL;

	return self->pv->last_reply;
}

static void
gcr_prompt_dialog_close (GcrPrompt *prompt)
{
	GcrPromptDialog *self = GCR_PROMPT_DIALOG (prompt);
	if (!self->pv->was_closed) {
		self->pv->was_closed = TRUE;
		gtk_widget_hide (GTK_WIDGET (self));
	}
}

static void
gcr_prompt_dialog_prompt_iface (GcrPromptIface *iface)
{
	iface->prompt_password_async = gcr_prompt_dialog_password_async;
	iface->prompt_password_finish = gcr_prompt_dialog_password_finish;
	iface->prompt_confirm_async = gcr_prompt_dialog_confirm_async;
	iface->prompt_confirm_finish = gcr_prompt_dialog_confirm_finish;
	iface->prompt_close = gcr_prompt_dialog_close;
}
