/*
 * Copyright (C) 2010 Stefan Walter
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gcr/gcr-fingerprint.h"
#include "gcr/gcr-icons.h"
#include "gcr/gcr-subject-public-key.h"

#include "gcr-key-renderer.h"
#include "gcr-display-view.h"
#include "gcr-renderer.h"
#include "gcr-viewer.h"

#include "gck/gck.h"

#include "egg/egg-asn1x.h"

#include <gdk/gdk.h>
#include <glib/gi18n-lib.h>

/**
 * GcrKeyRenderer:
 *
 * An implementation of #GcrRenderer which renders keys.
 */

/**
 * GcrKeyRendererClass:
 * @parent_class: The parent class.
 *
 * The class for #GcrKeyRenderer.
 */

enum {
	PROP_0,
	PROP_LABEL,
	PROP_ATTRIBUTES,
	PROP_OBJECT
};

struct _GcrKeyRendererPrivate {
	guint key_size;
	gchar *label;
	GckAttributes *attributes;
	GckObject *object;
	GIcon *icon;
	gulong notify_sig;
	GBytes *spk;
};

static void gcr_key_renderer_renderer_iface (GcrRendererIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrKeyRenderer, gcr_key_renderer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_RENDERER, gcr_key_renderer_renderer_iface));

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gchar*
calculate_label (GcrKeyRenderer *self)
{
	gchar *label;

	if (self->pv->label)
		return g_strdup (self->pv->label);

	if (self->pv->attributes) {
		if (gck_attributes_find_string (self->pv->attributes, CKA_LABEL, &label))
			return label;
	}

	return g_strdup (_("Key"));
}

static GckAttributes *
calculate_attrs (GcrKeyRenderer *self)
{
	if (self->pv->attributes)
		return gck_attributes_ref (self->pv->attributes);

	if (GCK_IS_OBJECT_CACHE (self->pv->object))
		return gck_object_cache_get_attributes (GCK_OBJECT_CACHE (self->pv->object));

	return NULL;
}

static guchar *
calculate_fingerprint (GcrKeyRenderer *self,
                       GckAttributes *attrs,
                       GChecksumType algorithm,
                       gsize *n_fingerprint)
{
	if (self->pv->spk)
		return gcr_fingerprint_from_subject_public_key_info (g_bytes_get_data (self->pv->spk, NULL),
		                                                     g_bytes_get_size (self->pv->spk),
		                                                     algorithm, n_fingerprint);

	return gcr_fingerprint_from_attributes (attrs, algorithm, n_fingerprint);
}

static void
on_subject_public_key (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
	GcrKeyRenderer *self = GCR_KEY_RENDERER (user_data);
	GError *error = NULL;
	GNode *node;

	node = _gcr_subject_public_key_load_finish (result, &error);
	if (error != NULL) {
		g_message ("couldn't load key information: %s", error->message);
		g_clear_error (&error);

	} else {
		if (self->pv->spk)
			g_bytes_unref (self->pv->spk);
		self->pv->spk = NULL;

		self->pv->spk = egg_asn1x_encode (node, NULL);
		if (self->pv->spk == NULL)
			g_warning ("invalid subjectPublicKey loaded: %s",
			           egg_asn1x_message (node));
		egg_asn1x_destroy (node);

		gcr_renderer_emit_data_changed (GCR_RENDERER (self));
	}

	g_object_unref (self);
}

static void
update_subject_public_key (GcrKeyRenderer *self)
{
	if (self->pv->spk)
		g_bytes_unref (self->pv->spk);
	self->pv->spk = NULL;

	if (!self->pv->object)
		return;

	_gcr_subject_public_key_load_async (self->pv->object, NULL,
	                                    on_subject_public_key,
	                                    g_object_ref (self));
}

static void
on_object_cache_attributes (GObject *obj,
                            GParamSpec *spec,
                            gpointer user_data)
{
	GcrKeyRenderer *self = GCR_KEY_RENDERER (user_data);
	update_subject_public_key (self);
	gcr_renderer_emit_data_changed (GCR_RENDERER (self));
}

static void
gcr_key_renderer_init (GcrKeyRenderer *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, GCR_TYPE_KEY_RENDERER, GcrKeyRendererPrivate));
	self->pv->icon = g_themed_icon_new (GCR_ICON_KEY);
}

static void
gcr_key_renderer_dispose (GObject *obj)
{
	GcrKeyRenderer *self = GCR_KEY_RENDERER (obj);

	if (self->pv->spk)
		g_bytes_unref (self->pv->spk);
	self->pv->spk = NULL;

	if (self->pv->object && self->pv->notify_sig) {
		g_signal_handler_disconnect (self->pv->object, self->pv->notify_sig);
		self->pv->notify_sig = 0;
	}
	g_clear_object (&self->pv->object);

	G_OBJECT_CLASS (gcr_key_renderer_parent_class)->dispose (obj);
}

static void
gcr_key_renderer_finalize (GObject *obj)
{
	GcrKeyRenderer *self = GCR_KEY_RENDERER (obj);

	if (self->pv->attributes)
		gck_attributes_unref (self->pv->attributes);
	self->pv->attributes = NULL;

	g_free (self->pv->label);
	self->pv->label = NULL;

	if (self->pv->icon)
		g_object_unref (self->pv->icon);
	self->pv->icon = NULL;

	G_OBJECT_CLASS (gcr_key_renderer_parent_class)->finalize (obj);
}

static void
gcr_key_renderer_set_property (GObject *obj, guint prop_id, const GValue *value,
                               GParamSpec *pspec)
{
	GcrKeyRenderer *self = GCR_KEY_RENDERER (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_free (self->pv->label);
		self->pv->label = g_value_dup_string (value);
		g_object_notify (obj, "label");
		gcr_renderer_emit_data_changed (GCR_RENDERER (self));
		break;
	case PROP_ATTRIBUTES:
		gck_attributes_unref (self->pv->attributes);
		self->pv->attributes = g_value_dup_boxed (value);
		gcr_renderer_emit_data_changed (GCR_RENDERER (self));
		break;
	case PROP_OBJECT:
		g_clear_object (&self->pv->object);
		self->pv->object = g_value_dup_object (value);
		if (self->pv->object) {
			gck_attributes_unref (self->pv->attributes);
			self->pv->attributes = NULL;
		}
		if (GCK_IS_OBJECT_CACHE (self->pv->object)) {
			self->pv->notify_sig = g_signal_connect (self->pv->object,
			                                         "notify::attributes",
			                                         G_CALLBACK (on_object_cache_attributes),
			                                         self);
			on_object_cache_attributes (G_OBJECT (self->pv->object), NULL, self);
		}
		g_object_notify (obj, "attributes");
		g_object_notify (obj, "object");
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_key_renderer_get_property (GObject *obj, guint prop_id, GValue *value,
                               GParamSpec *pspec)
{
	GcrKeyRenderer *self = GCR_KEY_RENDERER (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_take_string (value, calculate_label (self));
		break;
	case PROP_ATTRIBUTES:
		g_value_take_boxed (value, calculate_attrs (self));
		break;
	case PROP_OBJECT:
		g_value_set_object (value, self->pv->object);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_key_renderer_class_init (GcrKeyRendererClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GckBuilder builder = GCK_BUILDER_INIT;

	gcr_key_renderer_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (GcrKeyRendererPrivate));

	gobject_class->dispose = gcr_key_renderer_dispose;
	gobject_class->finalize = gcr_key_renderer_finalize;
	gobject_class->set_property = gcr_key_renderer_set_property;
	gobject_class->get_property = gcr_key_renderer_get_property;

	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_ATTRIBUTES, "attributes");

	g_object_class_install_property (gobject_class, PROP_OBJECT,
	            g_param_spec_object ("object", "Object", "Key Object", GCK_TYPE_OBJECT,
	                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/* Register this as a view which can be loaded */
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_PRIVATE_KEY);
	gcr_renderer_register (GCR_TYPE_KEY_RENDERER, gck_builder_end (&builder));
}

static void
gcr_key_renderer_real_render (GcrRenderer *renderer, GcrViewer *viewer)
{
	GcrKeyRenderer *self;
	GcrDisplayView *view;
	const gchar *text = "";
	GckAttributes *attrs;
	gpointer fingerprint;
	gsize n_fingerprint;
	gchar *display;
	gulong klass;
	gulong key_type;
	guint size;

	self = GCR_KEY_RENDERER (renderer);

	if (GCR_IS_DISPLAY_VIEW (viewer)) {
		view = GCR_DISPLAY_VIEW (viewer);

	} else {
		g_warning ("GcrKeyRenderer only works with internal specific "
		           "GcrViewer returned by gcr_viewer_new().");
		return;
	}

	_gcr_display_view_begin (view, renderer);

	attrs = calculate_attrs (self);
	if (attrs == NULL) {
		_gcr_display_view_end (view, renderer);
		return;
	}

	if (!gck_attributes_find_ulong (attrs, CKA_CLASS, &klass) ||
	    !gck_attributes_find_ulong (attrs, CKA_KEY_TYPE, &key_type)) {
		g_warning ("private key does not have the CKA_CLASS and CKA_KEY_TYPE attributes");
		_gcr_display_view_end (view, renderer);
		gck_attributes_unref (attrs);
		return;
	}

	_gcr_display_view_set_icon (view, renderer, self->pv->icon);

	display = calculate_label (self);
	_gcr_display_view_append_title (view, renderer, display);
	g_free (display);

	if (klass == CKO_PRIVATE_KEY) {
		if (key_type == CKK_RSA)
			text = _("Private RSA Key");
		else if (key_type == CKK_DSA)
			text = _("Private DSA Key");
		else if (key_type == CKK_ECDSA)
			text = _("Private ECDSA Key");
		else
			text = _("Private Key");
	} else if (klass == CKO_PUBLIC_KEY) {
		if (key_type == CKK_RSA)
			text = _("Public DSA Key");
		else if (key_type == CKK_DSA)
			text = _("Public DSA Key");
		else if (key_type == CKK_EC)
			text = _("Public Elliptic Curve Key");
		else
			text = _("Public Key");
	}

	_gcr_display_view_append_content (view, renderer, text, NULL);

	size = _gcr_subject_public_key_attributes_size (attrs);
	if (size > 0) {
		display = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%u bit", "%u bits", size), size);
		_gcr_display_view_append_content (view, renderer, _("Strength"), display);
		g_free (display);
	}

	_gcr_display_view_start_details (view, renderer);

	if (key_type == CKK_RSA)
		text = _("RSA");
	else if (key_type == CKK_DSA)
		text = _("DSA");
	else if (key_type == CKK_EC)
		text = _("Elliptic Curve");
	else
		text = _("Unknown");
	_gcr_display_view_append_value (view, renderer, _("Algorithm"), text, FALSE);

	if (size == 0)
		display = g_strdup (_("Unknown"));
	else
		display = g_strdup_printf ("%u", size);
	_gcr_display_view_append_value (view, renderer, _("Size"), display, FALSE);
	g_free (display);

	/* Fingerprints */
	_gcr_display_view_append_heading (view, renderer, _("Fingerprints"));

	fingerprint = calculate_fingerprint (self, attrs, G_CHECKSUM_SHA1, &n_fingerprint);
	if (fingerprint) {
		_gcr_display_view_append_hex (view, renderer, _("SHA1"), fingerprint, n_fingerprint);
		g_free (fingerprint);
	}
	fingerprint = calculate_fingerprint (self, attrs, G_CHECKSUM_SHA256, &n_fingerprint);
	if (fingerprint) {
		_gcr_display_view_append_hex (view, renderer, _("SHA256"), fingerprint, n_fingerprint);
		g_free (fingerprint);
	}

	_gcr_display_view_end (view, renderer);
	gck_attributes_unref (attrs);
}

static void
gcr_key_renderer_renderer_iface (GcrRendererIface *iface)
{
	iface->render_view = gcr_key_renderer_real_render;
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gcr_key_renderer_new:
 * @label: (allow-none): label describing the key
 * @attrs: (allow-none): key to display, or %NULL
 *
 * Create a new key renderer which renders a given key in the attributes.
 *
 * Returns: (transfer full): a newly allocated #GcrKeyRenderer, which should be
 *          freed with g_object_unref()
 */
GcrKeyRenderer*
gcr_key_renderer_new (const gchar *label, GckAttributes *attrs)
{
	return g_object_new (GCR_TYPE_KEY_RENDERER, "label", label, "attributes", attrs, NULL);
}

/**
 * gcr_key_renderer_set_attributes:
 * @self: The key renderer
 * @attrs: (allow-none): the attributes to display
 *
 * Get the attributes displayed in the renderer. The attributes should represent
 * either an RSA, DSA, or ECDSA key in PKCS\#11 style.
 */
void
gcr_key_renderer_set_attributes (GcrKeyRenderer *self, GckAttributes *attrs)
{
	g_return_if_fail (GCR_IS_KEY_RENDERER (self));

	if (self->pv->attributes)
		gck_attributes_unref (self->pv->attributes);
	self->pv->attributes = attrs;
	if (self->pv->attributes)
		gck_attributes_ref (self->pv->attributes);

	g_object_notify (G_OBJECT (self), "attributes");
	gcr_renderer_emit_data_changed (GCR_RENDERER (self));
}

/**
 * gcr_key_renderer_get_attributes:
 * @self: The key renderer
 *
 * Get the attributes displayed in the renderer.
 *
 * Returns: (transfer none) (allow-none): the attributes, owned by the renderer
 */
GckAttributes*
gcr_key_renderer_get_attributes (GcrKeyRenderer *self)
{
	g_return_val_if_fail (GCR_IS_KEY_RENDERER (self), NULL);
	return self->pv->attributes;
}
