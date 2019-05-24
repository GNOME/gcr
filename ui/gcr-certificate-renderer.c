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

#include "gcr/gcr-certificate.h"
#include "gcr/gcr-certificate-extensions.h"
#include "gcr/gcr-fingerprint.h"
#include "gcr/gcr-icons.h"
#include "gcr/gcr-oids.h"
#include "gcr/gcr-simple-certificate.h"

#include "gcr-certificate-exporter.h"
#include "gcr-certificate-renderer.h"
#include "gcr-certificate-renderer-private.h"
#include "gcr-deprecated.h"
#include "gcr-display-view.h"
#include "gcr-renderer.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"
#include "egg/egg-oid.h"
#include "egg/egg-hex.h"

#include "gck/gck.h"

#include <gdk/gdk.h>
#include <glib/gi18n-lib.h>

/**
 * GcrCertificateRenderer:
 *
 * An implementation of #GcrRenderer which renders certificates.
 */

/**
 * GcrCertificateRendererClass:
 * @parent_class: The parent class.
 *
 * The class for #GcrCertificateRenderer.
 */

enum {
	PROP_0,
	PROP_CERTIFICATE,
	PROP_LABEL,
	PROP_ATTRIBUTES
};

struct _GcrCertificateRendererPrivate {
	GcrCertificate *opt_cert;
	GckAttributes *opt_attrs;
	guint key_size;
	gchar *label;
};

static void gcr_renderer_iface_init (GcrRendererIface *iface);
static void gcr_renderer_certificate_iface_init (GcrCertificateIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateRenderer, gcr_certificate_renderer, G_TYPE_OBJECT,
	G_ADD_PRIVATE (GcrCertificateRenderer);
	G_IMPLEMENT_INTERFACE (GCR_TYPE_RENDERER, gcr_renderer_iface_init);
	GCR_CERTIFICATE_MIXIN_IMPLEMENT_COMPARABLE ();
	G_IMPLEMENT_INTERFACE (GCR_TYPE_CERTIFICATE, gcr_renderer_certificate_iface_init);
);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gchar*
calculate_label (GcrCertificateRenderer *self)
{
	gchar *label;

	if (self->pv->label)
		return g_strdup (self->pv->label);

	if (self->pv->opt_attrs) {
		if (gck_attributes_find_string (self->pv->opt_attrs, CKA_LABEL, &label))
			return label;
	}

	label = gcr_certificate_get_subject_cn (GCR_CERTIFICATE (self));
	if (label != NULL)
		return label;

	return g_strdup (_("Certificate"));
}

static gboolean
append_extension_basic_constraints (GcrRenderer *renderer,
                                    GcrDisplayView *view,
                                    GBytes *data)
{
	gboolean is_ca = FALSE;
	gint path_len = -1;
	gchar *number;

	if (!_gcr_certificate_extension_basic_constraints (data, &is_ca, &path_len))
		return FALSE;

	_gcr_display_view_append_heading (view, renderer, _("Basic Constraints"));

	_gcr_display_view_append_value (view, renderer, _("Certificate Authority"),
	                                is_ca ? _("Yes") : _("No"), FALSE);

	number = g_strdup_printf ("%d", path_len);
	_gcr_display_view_append_value (view, renderer, _("Max Path Length"),
	                                path_len < 0 ? _("Unlimited") : number, FALSE);
	g_free (number);

	return TRUE;
}

static gboolean
append_extension_extended_key_usage (GcrRenderer *renderer,
                                     GcrDisplayView *view,
                                     GBytes *data)
{
	GQuark *oids;
	GString *text;
	guint i;

	oids = _gcr_certificate_extension_extended_key_usage (data);
	if (oids == NULL)
		return FALSE;

	_gcr_display_view_append_heading (view, renderer, _("Extended Key Usage"));

	text = g_string_new ("");
	for (i = 0; oids[i] != 0; i++) {
		if (i > 0)
			g_string_append_unichar (text, GCR_DISPLAY_VIEW_LINE_BREAK);
		g_string_append (text, egg_oid_get_description (oids[i]));
	}

	g_free (oids);

	_gcr_display_view_append_value (view, renderer, _("Allowed Purposes"),
	                                text->str, FALSE);

	g_string_free (text, TRUE);

	return TRUE;
}

static gboolean
append_extension_subject_key_identifier (GcrRenderer *renderer,
                                         GcrDisplayView *view,
                                         GBytes *data)
{
	gpointer keyid;
	gsize n_keyid;

	keyid = _gcr_certificate_extension_subject_key_identifier (data, &n_keyid);
	if (keyid == NULL)
		return FALSE;

	_gcr_display_view_append_heading (view, renderer, _("Subject Key Identifier"));
	_gcr_display_view_append_hex (view, renderer, _("Key Identifier"), keyid, n_keyid);

	g_free (keyid);

	return TRUE;
}

static const struct {
	guint usage;
	const gchar *description;
} usage_descriptions[] = {
	{ GCR_KEY_USAGE_DIGITAL_SIGNATURE, N_("Digital signature") },
	{ GCR_KEY_USAGE_KEY_ENCIPHERMENT, N_("Key encipherment") },
	{ GCR_KEY_USAGE_DATA_ENCIPHERMENT, N_("Data encipherment") },
	{ GCR_KEY_USAGE_KEY_AGREEMENT, N_("Key agreement") },
	{ GCR_KEY_USAGE_KEY_CERT_SIGN, N_("Certificate signature") },
	{ GCR_KEY_USAGE_CRL_SIGN, N_("Revocation list signature") }
};

static gboolean
append_extension_key_usage (GcrRenderer *renderer,
                            GcrDisplayView *view,
                            GBytes *data)
{
	gulong key_usage;
	GString *text;
	guint i;

	if (!_gcr_certificate_extension_key_usage (data, &key_usage))
		return FALSE;

	text = g_string_new ("");

	for (i = 0; i < G_N_ELEMENTS (usage_descriptions); i++) {
		if (key_usage & usage_descriptions[i].usage) {
			if (text->len > 0)
				g_string_append_unichar (text, GCR_DISPLAY_VIEW_LINE_BREAK);
			g_string_append (text, _(usage_descriptions[i].description));
		}
	}

	_gcr_display_view_append_heading (view, renderer, _("Key Usage"));
	_gcr_display_view_append_value (view, renderer, _("Usages"), text->str, FALSE);

	g_string_free (text, TRUE);

	return TRUE;
}

static gboolean
append_extension_subject_alt_name (GcrRenderer *renderer,
                                   GcrDisplayView *view,
                                   GBytes *data)
{
	GArray *general_names;
	GcrGeneralName *general;
	guint i;

	general_names = _gcr_certificate_extension_subject_alt_name (data);
	if (general_names == NULL)
		return FALSE;

	_gcr_display_view_append_heading (view, renderer, _("Subject Alternative Names"));

	for (i = 0; i < general_names->len; i++) {
		general = &g_array_index (general_names, GcrGeneralName, i);
		if (general->display == NULL)
			_gcr_display_view_append_hex (view, renderer, general->description,
			                              g_bytes_get_data (general->raw, NULL),
			                              g_bytes_get_size (general->raw));
		else
			_gcr_display_view_append_value (view, renderer, general->description,
			                                general->display, FALSE);
	}

	_gcr_general_names_free (general_names);

	return TRUE;
}

static gboolean
append_extension_hex (GcrRenderer *renderer,
                      GcrDisplayView *view,
                      GQuark oid,
                      gconstpointer data,
                      gsize n_data)
{
	const gchar *text;

	_gcr_display_view_append_heading (view, renderer, _("Extension"));

	/* Extension type */
	text = egg_oid_get_description (oid);
	_gcr_display_view_append_value (view, renderer, _("Identifier"), text, FALSE);
	_gcr_display_view_append_hex (view, renderer, _("Value"), data, n_data);

	return TRUE;
}

static void
on_export_completed (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GcrCertificateExporter *exporter = GCR_CERTIFICATE_EXPORTER (source);
	GError *error = NULL;
	GtkWidget *dialog;

	if (!_gcr_certificate_exporter_export_finish (exporter, result, &error)) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			dialog = gtk_message_dialog_new_with_markup (parent,
				  GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
				  GTK_BUTTONS_OK, "<big>%s</big>\n\n%s",
				  _("Couldn’t export the certificate."),
				  error->message);
			gtk_widget_show (dialog);
			g_signal_connect (dialog, "delete-event",
					  G_CALLBACK (gtk_widget_destroy), dialog);
			g_signal_connect_swapped(dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
		}
	}

	/* Matches ref in on_certificate_export */
	if (parent)
		g_object_unref (parent);
}

static void
on_certificate_export (GtkMenuItem *menuitem, gpointer user_data)
{
	GcrCertificateRenderer *self = GCR_CERTIFICATE_RENDERER (user_data);
	GcrCertificateExporter *exporter;
	gchar *label;
	GtkWidget *parent;

	label = calculate_label (self);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (menuitem));
	if (parent && !GTK_IS_WINDOW (parent))
		parent = NULL;

	exporter = _gcr_certificate_exporter_new (GCR_CERTIFICATE (self), label,
	                                          GTK_WINDOW (parent));

	g_free (label);

	_gcr_certificate_exporter_export_async (exporter, NULL, on_export_completed,
	                                        parent ? g_object_ref (parent) : NULL);
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
gcr_certificate_renderer_init (GcrCertificateRenderer *self)
{
	self->pv = gcr_certificate_renderer_get_instance_private (self);
}

static void
gcr_certificate_renderer_dispose (GObject *obj)
{
	GcrCertificateRenderer *self = GCR_CERTIFICATE_RENDERER (obj);

	if (self->pv->opt_cert)
		g_object_unref (self->pv->opt_cert);
	self->pv->opt_cert = NULL;

	G_OBJECT_CLASS (gcr_certificate_renderer_parent_class)->dispose (obj);
}

static void
gcr_certificate_renderer_finalize (GObject *obj)
{
	GcrCertificateRenderer *self = GCR_CERTIFICATE_RENDERER (obj);

	g_assert (!self->pv->opt_cert);

	if (self->pv->opt_attrs)
		gck_attributes_unref (self->pv->opt_attrs);
	self->pv->opt_attrs = NULL;

	g_free (self->pv->label);
	self->pv->label = NULL;

	G_OBJECT_CLASS (gcr_certificate_renderer_parent_class)->finalize (obj);
}

static void
gcr_certificate_renderer_set_property (GObject *obj, guint prop_id, const GValue *value,
                                     GParamSpec *pspec)
{
	GcrCertificateRenderer *self = GCR_CERTIFICATE_RENDERER (obj);

	switch (prop_id) {
	case PROP_CERTIFICATE:
		gcr_certificate_renderer_set_certificate (self, g_value_get_object (value));
		break;
	case PROP_LABEL:
		g_free (self->pv->label);
		self->pv->label = g_value_dup_string (value);
		g_object_notify (obj, "label");
		gcr_renderer_emit_data_changed (GCR_RENDERER (self));
		break;
	case PROP_ATTRIBUTES:
		gck_attributes_unref (self->pv->opt_attrs);
		self->pv->opt_attrs = g_value_get_boxed (value);
		if (self->pv->opt_attrs)
			gck_attributes_ref (self->pv->opt_attrs);
		if (self->pv->opt_cert) {
			g_object_unref (self->pv->opt_cert);
			g_object_notify (G_OBJECT (self), "certificate");
			self->pv->opt_cert = NULL;
		}
		gcr_renderer_emit_data_changed (GCR_RENDERER (self));
		g_object_notify (G_OBJECT (self), "attributes");
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_certificate_renderer_get_property (GObject *obj, guint prop_id, GValue *value,
                                     GParamSpec *pspec)
{
	GcrCertificateRenderer *self = GCR_CERTIFICATE_RENDERER (obj);

	switch (prop_id) {
	case PROP_CERTIFICATE:
		g_value_set_object (value, self->pv->opt_cert);
		break;
	case PROP_LABEL:
		g_value_take_string (value, calculate_label (self));
		break;
	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, self->pv->opt_attrs);
		break;
	default:
		gcr_certificate_mixin_get_property (obj, prop_id, value, pspec);
		break;
	}
}

static void
gcr_certificate_renderer_class_init (GcrCertificateRendererClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GckBuilder builder = GCK_BUILDER_INIT;

	gcr_certificate_renderer_parent_class = g_type_class_peek_parent (klass);

	gobject_class->dispose = gcr_certificate_renderer_dispose;
	gobject_class->finalize = gcr_certificate_renderer_finalize;
	gobject_class->set_property = gcr_certificate_renderer_set_property;
	gobject_class->get_property = gcr_certificate_renderer_get_property;

	/**
	 * GcrCertificateRenderer:certificate:
	 *
	 * The certificate to display. May be %NULL.
	 */
	g_object_class_install_property (gobject_class, PROP_CERTIFICATE,
	           g_param_spec_object ("certificate", "Certificate", "Certificate to display.",
	                                GCR_TYPE_CERTIFICATE, G_PARAM_READWRITE));

	/**
	 * GcrCertificateRenderer:attributes:
	 *
	 * The certificate attributes to display. One of the attributes must be
	 * a CKA_VALUE type attribute which contains a DER encoded certificate.
	 */
	g_object_class_install_property (gobject_class, PROP_ATTRIBUTES,
	           g_param_spec_boxed ("attributes", "Attributes", "Certificate pkcs11 attributes",
	                               GCK_TYPE_ATTRIBUTES, G_PARAM_READWRITE));

	/**
	 * GcrCertificateRenderer:label:
	 *
	 * The label to display.
	 */
	g_object_class_install_property (gobject_class, PROP_LABEL,
	           g_param_spec_string ("label", "Label", "Certificate Label",
	                                "", G_PARAM_READWRITE));

	gcr_certificate_mixin_class_init (gobject_class);

	/* Register this as a renderer which can be loaded */
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_CERTIFICATE);
	gcr_renderer_register (GCR_TYPE_CERTIFICATE_RENDERER, gck_builder_end (&builder));
}

static void
gcr_certificate_renderer_render (GcrRenderer *renderer, GcrViewer *viewer)
{
	GcrCertificateRenderer *self;
	GNode *extension;
	gconstpointer data;
	gsize n_data;
	GcrDisplayView *view;
	GcrCertificate *cert;
	GBytes *number;
	gulong version;
	guint bits, index;
	gchar *display;
	GBytes *bytes;
	GNode *asn;
	GDate date;
	GIcon *icon;

	self = GCR_CERTIFICATE_RENDERER (renderer);

	if (GCR_IS_DISPLAY_VIEW (viewer)) {
		view = GCR_DISPLAY_VIEW (viewer);

	} else {
		g_warning ("GcrCertificateRenderer only works with internal specific "
		           "GcrViewer returned by gcr_viewer_new().");
		return;
	}

	_gcr_display_view_begin (view, renderer);
	cert = GCR_CERTIFICATE (self);

	data = gcr_certificate_get_der_data (cert, &n_data);
	if (!data) {
		_gcr_display_view_end (view, renderer);
		return;
	}

	icon = gcr_certificate_get_icon (cert);
	_gcr_display_view_set_icon (view, GCR_RENDERER (self), icon);
	g_object_unref (icon);

	bytes = g_bytes_new_static (data, n_data);
	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "Certificate", bytes);
	g_return_if_fail (asn != NULL);
	g_bytes_unref (bytes);

	display = calculate_label (self);
	_gcr_display_view_append_title (view, renderer, display);
	g_free (display);

	display = egg_dn_read_part (egg_asn1x_node (asn, "tbsCertificate", "subject", "rdnSequence", NULL), "CN");
	_gcr_display_view_append_content (view, renderer, _("Identity"), display);
	g_free (display);

	display = egg_dn_read_part (egg_asn1x_node (asn, "tbsCertificate", "issuer", "rdnSequence", NULL), "CN");
	_gcr_display_view_append_content (view, renderer, _("Verified by"), display);
	g_free (display);

	if (egg_asn1x_get_time_as_date (egg_asn1x_node (asn, "tbsCertificate", "validity", "notAfter", NULL), &date)) {
		display = g_malloc0 (128);
		if (!g_date_strftime (display, 128, "%x", &date))
			g_return_if_reached ();
		_gcr_display_view_append_content (view, renderer, _("Expires"), display);
		g_free (display);
	}

	_gcr_display_view_start_details (view, renderer);

	/* The subject */
	_gcr_display_view_append_heading (view, renderer, _("Subject Name"));
	_gcr_certificate_renderer_append_distinguished_name (renderer, view,
	                                                     egg_asn1x_node (asn, "tbsCertificate", "subject", "rdnSequence", NULL));

	/* The Issuer */
	_gcr_display_view_append_heading (view, renderer, _("Issuer Name"));
	_gcr_certificate_renderer_append_distinguished_name (renderer, view,
	                                                     egg_asn1x_node (asn, "tbsCertificate", "issuer", "rdnSequence", NULL));

	/* The Issued Parameters */
	_gcr_display_view_append_heading (view, renderer, _("Issued Certificate"));

	if (!egg_asn1x_get_integer_as_ulong (egg_asn1x_node (asn, "tbsCertificate", "version", NULL), &version))
		g_return_if_reached ();
	display = g_strdup_printf ("%lu", version + 1);
	_gcr_display_view_append_value (view, renderer, _("Version"), display, FALSE);
	g_free (display);

	number = egg_asn1x_get_integer_as_raw (egg_asn1x_node (asn, "tbsCertificate", "serialNumber", NULL));
	g_return_if_fail (number != NULL);
	_gcr_display_view_append_hex (view, renderer, _("Serial Number"),
	                              g_bytes_get_data (number, NULL),
	                              g_bytes_get_size (number));
	g_bytes_unref (number);

	display = g_malloc0 (128);
	if (egg_asn1x_get_time_as_date (egg_asn1x_node (asn, "tbsCertificate", "validity", "notBefore", NULL), &date)) {
		if (!g_date_strftime (display, 128, "%Y-%m-%d", &date))
			g_return_if_reached ();
		_gcr_display_view_append_value (view, renderer, _("Not Valid Before"), display, FALSE);
	}
	if (egg_asn1x_get_time_as_date (egg_asn1x_node (asn, "tbsCertificate", "validity", "notAfter", NULL), &date)) {
		if (!g_date_strftime (display, 128, "%Y-%m-%d", &date))
			g_return_if_reached ();
		_gcr_display_view_append_value (view, renderer, _("Not Valid After"), display, FALSE);
	}
	g_free (display);

	/* Fingerprints */
	_gcr_display_view_append_heading (view, renderer, _("Certificate Fingerprints"));

	_gcr_display_view_append_fingerprint (view, renderer, data, n_data, "SHA1", G_CHECKSUM_SHA1);
	_gcr_display_view_append_fingerprint (view, renderer, data, n_data, "MD5", G_CHECKSUM_MD5);

	/* Public Key Info */
	_gcr_display_view_append_heading (view, renderer, _("Public Key Info"));
	bits = gcr_certificate_get_key_size (cert);
	_gcr_certificate_renderer_append_subject_public_key (renderer, view, bits,
	                                                     egg_asn1x_node (asn, "tbsCertificate",
	                                                                     "subjectPublicKeyInfo", NULL));

	/* Extensions */
	for (index = 1; TRUE; ++index) {
		extension = egg_asn1x_node (asn, "tbsCertificate", "extensions", index, NULL);
		if (extension == NULL)
			break;
		_gcr_certificate_renderer_append_extension (renderer, view, extension);
	}

	/* Signature */
	_gcr_display_view_append_heading (view, renderer, _("Signature"));
	_gcr_certificate_renderer_append_signature (renderer, view, asn);

	egg_asn1x_destroy (asn);
	_gcr_display_view_end (view, renderer);
}

static void
gcr_certificate_renderer_populate_popup (GcrRenderer *self, GcrViewer *viewer,
                                         GtkMenu *menu)
{
	GtkWidget *item;

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_label (_("Export Certificate\xE2\x80\xA6"));
	gtk_widget_show (item);
	g_signal_connect_data (item, "activate", G_CALLBACK (on_certificate_export),
	                       g_object_ref (self), (GClosureNotify)g_object_unref, 0);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
}

static void
gcr_renderer_iface_init (GcrRendererIface *iface)
{
	iface->populate_popup = gcr_certificate_renderer_populate_popup;
	iface->render_view = gcr_certificate_renderer_render;
}

static const guchar *
gcr_certificate_renderer_get_der_data (GcrCertificate *cert,
                                       gsize *n_data)
{
	GcrCertificateRenderer *self = GCR_CERTIFICATE_RENDERER (cert);
	const GckAttribute *attr;

	g_assert (n_data);

	if (self->pv->opt_cert)
		return gcr_certificate_get_der_data (self->pv->opt_cert, n_data);

	if (self->pv->opt_attrs) {
		attr = gck_attributes_find (self->pv->opt_attrs, CKA_VALUE);
		g_return_val_if_fail (attr, NULL);
		*n_data = attr->length;
		return attr->value;
	}

	return NULL;
}

static void
gcr_renderer_certificate_iface_init (GcrCertificateIface *iface)
{
	iface->get_der_data = gcr_certificate_renderer_get_der_data;
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gcr_certificate_renderer_new:
 * @certificate: The certificate to display
 *
 * Create a new certificate renderer to display the certificate.
 *
 * Returns: (transfer full): a newly allocated #GcrCertificateRenderer, which
 *          should be released with g_object_unref()
 */
GcrCertificateRenderer *
gcr_certificate_renderer_new (GcrCertificate *certificate)
{
	return g_object_new (GCR_TYPE_CERTIFICATE_RENDERER, "certificate", certificate, NULL);
}

/**
 * gcr_certificate_renderer_new_for_attributes:
 * @label: (allow-none): the label to display
 * @attrs: The attributes to display
 *
 * Create a new certificate renderer to display the label and attributes. One
 * of the attributes should be a CKA_VALUE type attribute containing a DER
 * encoded certificate.
 *
 * Returns: (transfer full): a newly allocated #GcrCertificateRenderer, which
 *          should be released with g_object_unref()
 */
GcrCertificateRenderer *
gcr_certificate_renderer_new_for_attributes (const gchar *label, struct _GckAttributes *attrs)
{
	return g_object_new (GCR_TYPE_CERTIFICATE_RENDERER, "label", label, "attributes", attrs, NULL);
}

/**
 * gcr_certificate_renderer_get_certificate:
 * @self: The renderer
 *
 * Get the certificate displayed in the renderer. If no certificate was
 * explicitly set, then the renderer will return itself since it acts as
 * a valid certificate.
 *
 * Returns: (transfer none): The certificate, owned by the renderer.
 */
GcrCertificate *
gcr_certificate_renderer_get_certificate (GcrCertificateRenderer *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_RENDERER (self), NULL);
	if (self->pv->opt_cert)
		return self->pv->opt_cert;
	return GCR_CERTIFICATE (self);
}

/**
 * gcr_certificate_renderer_set_certificate:
 * @self: The renderer
 * @certificate: (allow-none): the certificate to display
 *
 * Set a certificate to display in the renderer.
 */
void
gcr_certificate_renderer_set_certificate (GcrCertificateRenderer *self, GcrCertificate *certificate)
{
	g_return_if_fail (GCR_IS_CERTIFICATE_RENDERER (self));

	if (self->pv->opt_cert)
		g_object_unref (self->pv->opt_cert);
	self->pv->opt_cert = certificate;
	if (self->pv->opt_cert)
		g_object_ref (self->pv->opt_cert);

	if (self->pv->opt_attrs) {
		gck_attributes_unref (self->pv->opt_attrs);
		self->pv->opt_attrs = NULL;
	}

	gcr_renderer_emit_data_changed (GCR_RENDERER (self));
	g_object_notify (G_OBJECT (self), "certificate");
}

/**
 * gcr_certificate_renderer_get_attributes:
 * @self: The renderer
 *
 * Get the PKCS\#11 attributes, if any, set for this renderer to display.
 *
 * Returns: (allow-none) (transfer none): the attributes, owned by the renderer
 *
 * Deprecated: 3.6: Use gcr_renderer_get_attributes() instead
 */
GckAttributes *
gcr_certificate_renderer_get_attributes (GcrCertificateRenderer *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_RENDERER (self), NULL);
	return gcr_renderer_get_attributes (GCR_RENDERER (self));
}

/**
 * gcr_certificate_renderer_set_attributes:
 * @self: The renderer
 * @attrs: (allow-none): attributes to set
 *
 * Set the PKCS\#11 attributes for this renderer to display. One of the attributes
 * should be a CKA_VALUE type attribute containing a DER encoded certificate.
 *
 * Deprecated: 3.6: Use gcr_renderer_set_attributes() instead
 */
void
gcr_certificate_renderer_set_attributes (GcrCertificateRenderer *self, GckAttributes *attrs)
{
	g_return_if_fail (GCR_IS_CERTIFICATE_RENDERER (self));
	gcr_renderer_set_attributes (GCR_RENDERER (self), attrs);
}

typedef struct {
	GcrRenderer *renderer;
	GcrDisplayView *view;
} AppendDnClosure;

static void
on_parsed_dn_part (guint index,
                   GQuark oid,
                   GNode *value,
                   gpointer user_data)
{
	GcrRenderer *renderer = ((AppendDnClosure *)user_data)->renderer;
	GcrDisplayView *view = ((AppendDnClosure *)user_data)->view;
	const gchar *attr;
	const gchar *desc;
	gchar *field = NULL;
	gchar *display;

	attr = egg_oid_get_name (oid);
	desc = egg_oid_get_description (oid);

	/* Combine them into something sane */
	if (attr && desc) {
		if (strcmp (attr, desc) == 0)
			field = g_strdup (attr);
		else
			field = g_strdup_printf ("%s (%s)", attr, desc);
	} else if (!attr && !desc) {
		field = g_strdup ("");
	} else if (attr) {
		field = g_strdup (attr);
	} else if (desc) {
		field = g_strdup (desc);
	} else {
		g_assert_not_reached ();
	}

	display = egg_dn_print_value (oid, value);
	if (display == NULL)
		display = g_strdup ("");

	_gcr_display_view_append_value (view, renderer, field, display, FALSE);
	g_free (field);
	g_free (display);
}


void
_gcr_certificate_renderer_append_distinguished_name (GcrRenderer *renderer,
                                                     GcrDisplayView *view,
                                                     GNode *dn)
{
	AppendDnClosure closure;

	g_return_if_fail (GCR_IS_RENDERER (renderer));
	g_return_if_fail (GCR_IS_DISPLAY_VIEW (view));
	g_return_if_fail (dn != NULL);

	closure.renderer = renderer;
	closure.view = view;
	egg_dn_parse (dn, on_parsed_dn_part, &closure);
}

void
_gcr_certificate_renderer_append_subject_public_key (GcrRenderer *renderer,
                                                     GcrDisplayView *view,
                                                     guint key_nbits,
                                                     GNode *subject_public_key)
{
	const gchar *text;
	gchar *display;
	GBytes *value;
	guchar *raw;
	gsize n_raw;
	GQuark oid;
	guint bits;

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (subject_public_key,
	                                                  "algorithm", "algorithm", NULL));
	text = egg_oid_get_description (oid);
	_gcr_display_view_append_value (view, renderer, _("Key Algorithm"), text, FALSE);

	value = egg_asn1x_get_element_raw (egg_asn1x_node (subject_public_key,
	                                                   "algorithm", "parameters", NULL));
	if (value) {
		_gcr_display_view_append_hex (view, renderer, _("Key Parameters"),
		                              g_bytes_get_data (value, NULL),
		                              g_bytes_get_size (value));
		g_bytes_unref (value);
	}

	if (key_nbits > 0) {
		display = g_strdup_printf ("%u", key_nbits);
		_gcr_display_view_append_value (view, renderer, _("Key Size"), display, FALSE);
		g_free (display);
	}

	value = egg_asn1x_get_element_raw (subject_public_key);
	raw = gcr_fingerprint_from_subject_public_key_info (g_bytes_get_data (value, NULL),
	                                                    g_bytes_get_size (value),
	                                                    G_CHECKSUM_SHA1, &n_raw);
	_gcr_display_view_append_hex (view, renderer, _("Key SHA1 Fingerprint"), raw, n_raw);
	g_bytes_unref (value);
	g_free (raw);

	value = egg_asn1x_get_bits_as_raw (egg_asn1x_node (subject_public_key, "subjectPublicKey", NULL), &bits);
	_gcr_display_view_append_hex (view, renderer, _("Public Key"),
	                              g_bytes_get_data (value, NULL), bits / 8);
	g_bytes_unref (value);
}

void
_gcr_certificate_renderer_append_signature (GcrRenderer *renderer,
                                            GcrDisplayView *view,
                                            GNode *asn)
{
	const gchar *text;
	GBytes *value;
	GQuark oid;
	guint bits;

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, "signatureAlgorithm", "algorithm", NULL));
	text = egg_oid_get_description (oid);
	_gcr_display_view_append_value (view, renderer, _("Signature Algorithm"), text, FALSE);

	value = egg_asn1x_get_element_raw (egg_asn1x_node (asn, "signatureAlgorithm", "parameters", NULL));
	if (value) {
		_gcr_display_view_append_hex (view, renderer, _("Signature Parameters"),
		                              g_bytes_get_data (value, NULL),
		                              g_bytes_get_size (value));
		g_bytes_unref (value);
	}

	value = egg_asn1x_get_bits_as_raw (egg_asn1x_node (asn, "signature", NULL), &bits);
	_gcr_display_view_append_hex (view, renderer, _("Signature"),
	                              g_bytes_get_data (value, NULL), bits / 8);
	g_bytes_unref (value);
}

void
_gcr_certificate_renderer_append_extension (GcrRenderer *renderer,
                                            GcrDisplayView *view,
                                            GNode *node)
{
	GQuark oid;
	GBytes *value;
	gboolean critical;
	gboolean ret = FALSE;

	/* Dig out the OID */
	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (node, "extnID", NULL));
	g_return_if_fail (oid);

	/* Extension value */
	value = egg_asn1x_get_string_as_bytes (egg_asn1x_node (node, "extnValue", NULL));

	/* The custom parsers */
	if (oid == GCR_OID_BASIC_CONSTRAINTS)
		ret = append_extension_basic_constraints (renderer, view, value);
	else if (oid == GCR_OID_EXTENDED_KEY_USAGE)
		ret = append_extension_extended_key_usage (renderer, view, value);
	else if (oid == GCR_OID_SUBJECT_KEY_IDENTIFIER)
		ret = append_extension_subject_key_identifier (renderer, view, value);
	else if (oid == GCR_OID_KEY_USAGE)
		ret = append_extension_key_usage (renderer, view, value);
	else if (oid == GCR_OID_SUBJECT_ALT_NAME)
		ret = append_extension_subject_alt_name (renderer, view, value);

	/* Otherwise the default raw display */
	if (ret == FALSE)
		ret = append_extension_hex (renderer, view, oid,
		                            g_bytes_get_data (value, NULL),
		                            g_bytes_get_size (value));

	/* Critical */
	if (ret == TRUE && egg_asn1x_get_boolean (egg_asn1x_node (node, "critical", NULL), &critical)) {
		_gcr_display_view_append_value (view, renderer, _("Critical"),
		                                critical ? _("Yes") : _("No"), FALSE);
	}
}
