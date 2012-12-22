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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "gcr/gcr-oids.h"
#include "gcr/gcr-subject-public-key.h"

#include "gcr-certificate-renderer-private.h"
#include "gcr-certificate-request-renderer.h"
#include "gcr-display-view.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"
#include "egg/egg-oid.h"

#include "gck/gck.h"

#include <glib/gi18n-lib.h>

/**
 * GcrCertificateRequestRenderer:
 *
 * An implementation of #GcrRenderer which renders certificate requests
 */

/**
 * GcrCertificateRequestRendererClass:
 * @parent_class: The parent class
 *
 * The class for #GcrCertificateRequestRenderer
 */

enum {
	PROP_0,
	PROP_LABEL,
	PROP_ATTRIBUTES
};

struct _GcrCertificateRequestRendererPrivate {
	GckAttributes *attrs;
	gchar *label;

	guint key_size;
	gulong type;
	GNode *asn;
};

static void     _gcr_certificate_request_renderer_iface    (GcrRendererIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateRequestRenderer, _gcr_certificate_request_renderer, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GCR_TYPE_RENDERER, _gcr_certificate_request_renderer_iface);
);

static gchar*
calculate_label (GcrCertificateRequestRenderer *self)
{
	gchar *label = NULL;

	if (self->pv->label)
		return g_strdup (self->pv->label);

	if (self->pv->attrs) {
		if (gck_attributes_find_string (self->pv->attrs, CKA_LABEL, &label))
			return label;
	}

	if (self->pv->asn && self->pv->type == CKQ_GCR_PKCS10) {
		label = egg_dn_read_part (egg_asn1x_node (self->pv->asn,
		                                          "certificationRequestInfo",
		                                          "subject",
		                                          "rdnSequence",
		                                          NULL), "CN");
	}

	if (label != NULL)
		return label;

	return g_strdup (_("Certificate request"));
}

static void
_gcr_certificate_request_renderer_init (GcrCertificateRequestRenderer *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, GCR_TYPE_CERTIFICATE_REQUEST_RENDERER,
	                                         GcrCertificateRequestRendererPrivate));
}

static void
_gcr_certificate_request_renderer_finalize (GObject *obj)
{
	GcrCertificateRequestRenderer *self = GCR_CERTIFICATE_REQUEST_RENDERER (obj);

	if (self->pv->attrs)
		gck_attributes_unref (self->pv->attrs);
	self->pv->attrs = NULL;

	g_free (self->pv->label);
	self->pv->label = NULL;

	egg_asn1x_destroy (self->pv->asn);

	G_OBJECT_CLASS (_gcr_certificate_request_renderer_parent_class)->finalize (obj);
}

static void
_gcr_certificate_request_renderer_set_property (GObject *obj,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
	GcrCertificateRequestRenderer *self = GCR_CERTIFICATE_REQUEST_RENDERER (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_free (self->pv->label);
		self->pv->label = g_value_dup_string (value);
		g_object_notify (obj, "label");
		gcr_renderer_emit_data_changed (GCR_RENDERER (self));
		break;
	case PROP_ATTRIBUTES:
		_gcr_certificate_request_renderer_set_attributes (self, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
_gcr_certificate_request_renderer_get_property (GObject *obj,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
	GcrCertificateRequestRenderer *self = GCR_CERTIFICATE_REQUEST_RENDERER (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_take_string (value, calculate_label (self));
		break;
	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, self->pv->attrs);
		break;
	default:
		gcr_certificate_mixin_get_property (obj, prop_id, value, pspec);
		break;
	}
}

static void
_gcr_certificate_request_renderer_class_init (GcrCertificateRequestRendererClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GckBuilder builder = GCK_BUILDER_INIT;

	_gcr_oids_init ();

	g_type_class_add_private (klass, sizeof (GcrCertificateRequestRendererPrivate));

	gobject_class->finalize = _gcr_certificate_request_renderer_finalize;
	gobject_class->set_property = _gcr_certificate_request_renderer_set_property;
	gobject_class->get_property = _gcr_certificate_request_renderer_get_property;

	/**
	 * GcrCertificateRequestRenderer:attributes:
	 *
	 * The certificate attributes to display. One of the attributes must be
	 * a CKA_VALUE type attribute which contains a DER encoded certificate.
	 */
	g_object_class_install_property (gobject_class, PROP_ATTRIBUTES,
	           g_param_spec_boxed ("attributes", "Attributes", "Certificate pkcs11 attributes",
	                               GCK_TYPE_ATTRIBUTES, G_PARAM_READWRITE));

	/**
	 * GcrCertificateRequestRenderer:label:
	 *
	 * The label to display.
	 */

	g_object_class_install_property (gobject_class, PROP_LABEL,
	           g_param_spec_string ("label", "Label", "Certificate Label",
	                                "", G_PARAM_READWRITE));

	/* Register this as a renderer which can be loaded */
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_GCR_CERTIFICATE_REQUEST);
	gck_builder_add_ulong (&builder, CKA_GCR_CERTIFICATE_REQUEST_TYPE, CKQ_GCR_PKCS10);
	gcr_renderer_register (GCR_TYPE_CERTIFICATE_REQUEST_RENDERER, gck_builder_end (&builder));

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_GCR_CERTIFICATE_REQUEST);
	gck_builder_add_ulong (&builder, CKA_GCR_CERTIFICATE_REQUEST_TYPE, CKQ_GCR_SPKAC);
	gcr_renderer_register (GCR_TYPE_CERTIFICATE_REQUEST_RENDERER, gck_builder_end (&builder));
}

static gboolean
append_extension_request (GcrRenderer *renderer,
                          GcrDisplayView *view,
                          GNode *attribute)
{
	GBytes *value;
	GNode *node;
	GNode *asn;
	guint i;

	node = egg_asn1x_node (attribute, "values", 1, NULL);
	if (node == NULL)
		return FALSE;

	value = egg_asn1x_get_element_raw (node);
	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "ExtensionRequest", value);
	if (asn == NULL)
		return FALSE;

	for (i = 1; TRUE; i++) {
		node = egg_asn1x_node (asn, i, NULL);
		if (node == NULL)
			break;
		_gcr_certificate_renderer_append_extension (renderer, view, node);
	}

	egg_asn1x_destroy (asn);
	return TRUE;
}

static void
append_attribute (GcrRenderer *renderer,
                  GcrDisplayView *view,
                  GNode *attribute)
{
	GQuark oid;
	GBytes *value;
	const gchar *text;
	GNode *node;
	gboolean ret = FALSE;
	gint i;

	/* Dig out the OID */
	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (attribute, "type", NULL));
	g_return_if_fail (oid);

	if (oid == GCR_OID_PKCS9_ATTRIBUTE_EXTENSION_REQ)
		ret = append_extension_request (renderer, view, attribute);

	if (!ret) {
		_gcr_display_view_append_heading (view, renderer, _("Attribute"));

		/* Extension type */
		text = egg_oid_get_description (oid);
		_gcr_display_view_append_value (view, renderer, _("Type"), text, FALSE);

		for (i = 1; TRUE; i++) {
			node = egg_asn1x_node (attribute, "values", i, NULL);
			if (node == NULL)
				break;
			value = egg_asn1x_get_element_raw (node);
			_gcr_display_view_append_hex (view, renderer, _("Value"),
			                              g_bytes_get_data (value, NULL),
			                              g_bytes_get_size (value));
			g_bytes_unref (value);
		}
	}
}

static guint
ensure_key_size (GcrCertificateRequestRenderer *self,
                 GNode *public_key)
{
	if (self->pv->key_size)
		return self->pv->key_size;

	self->pv->key_size = _gcr_subject_public_key_calculate_size (public_key);
	return self->pv->key_size;
}

static void
render_pkcs10_certificate_req (GcrCertificateRequestRenderer *self,
                               GcrDisplayView *view)
{
	GcrRenderer *renderer = GCR_RENDERER (self);
	GNode *public_key;
	GNode *attribute;
	GNode *subject;
	gchar *display;
	gulong version;
	guint bits;
	guint i;

	display = calculate_label (self);
	_gcr_display_view_append_title (view, renderer, display);
	g_free (display);

	_gcr_display_view_append_content (view, renderer, _("Certificate request"), NULL);

	subject = egg_asn1x_node (self->pv->asn, "certificationRequestInfo",
	                          "subject", "rdnSequence", NULL);
	display = egg_dn_read_part (subject, "CN");
	_gcr_display_view_append_content (view, renderer, _("Identity"), display);
	g_free (display);

	_gcr_display_view_start_details (view, renderer);

	/* The subject */
	_gcr_display_view_append_heading (view, renderer, _("Subject Name"));
	_gcr_certificate_renderer_append_distinguished_name (renderer, view, subject);

	/* The certificate request type */
	_gcr_display_view_append_heading (view, renderer, _("Certificate request"));
	_gcr_display_view_append_value (view, renderer, _("Type"), "PKCS#10", FALSE);
	if (!egg_asn1x_get_integer_as_ulong (egg_asn1x_node (self->pv->asn,
	                                                     "certificationRequestInfo",
	                                                     "version", NULL), &version))
		g_return_if_reached ();
	display = g_strdup_printf ("%lu", version + 1);
	_gcr_display_view_append_value (view, renderer, _("Version"), display, FALSE);
	g_free (display);

	_gcr_display_view_append_heading (view, renderer, _("Public Key Info"));
	public_key = egg_asn1x_node (self->pv->asn, "certificationRequestInfo", "subjectPKInfo", NULL);
	bits = ensure_key_size (self, public_key);
	_gcr_certificate_renderer_append_subject_public_key (renderer, view,
	                                                     bits, public_key);

	/* Attributes */
	for (i = 1; TRUE; ++i) {
		/* Make sure it is present */
		attribute = egg_asn1x_node (self->pv->asn, "certificationRequestInfo", "attributes", i, NULL);
		if (attribute == NULL)
			break;
		append_attribute (renderer, view, attribute);
	}

	/* Signature */
	_gcr_display_view_append_heading (view, renderer, _("Signature"));
	_gcr_certificate_renderer_append_signature (renderer, view, self->pv->asn);
}

static void
render_spkac_certificate_req (GcrCertificateRequestRenderer *self,
                              GcrDisplayView *view)
{
	GcrRenderer *renderer = GCR_RENDERER (self);
	GNode *public_key;
	gchar *display;
	guint bits;

	display = calculate_label (self);
	_gcr_display_view_append_title (view, renderer, display);
	g_free (display);

	_gcr_display_view_append_content (view, renderer, _("Certificate request"), NULL);

	_gcr_display_view_start_details (view, renderer);

	/* The certificate request type */
	_gcr_display_view_append_heading (view, renderer, _("Certificate request"));
	_gcr_display_view_append_value (view, renderer, _("Type"), "SPKAC", FALSE);

	display = egg_asn1x_get_string_as_utf8 (egg_asn1x_node (self->pv->asn, "publicKeyAndChallenge",
	                                                        "challenge", NULL), NULL);
	_gcr_display_view_append_value (view, renderer, _("Challenge"), display, FALSE);
	g_free (display);

	_gcr_display_view_append_heading (view, renderer, _("Public Key Info"));
	public_key = egg_asn1x_node (self->pv->asn, "publicKeyAndChallenge", "spki", NULL);
	bits = ensure_key_size (self, public_key);
	_gcr_certificate_renderer_append_subject_public_key (renderer, view,
	                                                     bits, public_key);

	/* Signature */
	_gcr_display_view_append_heading (view, renderer, _("Signature"));
	_gcr_certificate_renderer_append_signature (renderer, view, self->pv->asn);
}

static void
gcr_certificate_request_renderer_render (GcrRenderer *renderer,
                                     GcrViewer *viewer)
{
	GcrCertificateRequestRenderer *self;
	GcrDisplayView *view;
	GIcon *icon;

	self = GCR_CERTIFICATE_REQUEST_RENDERER (renderer);

	if (GCR_IS_DISPLAY_VIEW (viewer)) {
		view = GCR_DISPLAY_VIEW (viewer);

	} else {
		g_warning ("GcrCertificateRequestRenderer only works with internal specific "
		           "GcrViewer returned by gcr_viewer_new().");
		return;
	}

	_gcr_display_view_begin (view, renderer);

	icon = g_themed_icon_new ("dialog-question");
	_gcr_display_view_set_icon (view, GCR_RENDERER (self), icon);
	g_object_unref (icon);

	switch (self->pv->type) {
	case CKQ_GCR_PKCS10:
		render_pkcs10_certificate_req (self, view);
		break;
	case CKQ_GCR_SPKAC:
		render_spkac_certificate_req (self, view);
		break;
	default:
		g_warning ("unknown request type in GcrCertificateRequestRenderer");
		break;
	}

	_gcr_display_view_end (view, renderer);
}

static void
_gcr_certificate_request_renderer_iface (GcrRendererIface *iface)
{
	iface->render_view = gcr_certificate_request_renderer_render;
}

/**
 * gcr_certificate_request_renderer_new_for_attributes:
 * @label: (allow-none): the label to display
 * @attrs: the attributes to display
 *
 * Create a new certificate request renderer to display the label and attributes.
 * One of the attributes should be a CKA_VALUE type attribute containing a DER
 * encoded PKCS\#10 certificate request or an SPKAC request.
 *
 * Returns: (transfer full): a newly allocated #GcrCertificateRequestRenderer, which
 *          should be released with g_object_unref()
 */
GcrRenderer *
_gcr_certificate_request_renderer_new_for_attributes (const gchar *label,
                                                  GckAttributes *attrs)
{
	return g_object_new (GCR_TYPE_CERTIFICATE_REQUEST_RENDERER,
	                     "label", label,
	                     "attributes", attrs,
	                     NULL);
}

/**
 * gcr_certificate_request_renderer_get_attributes:
 * @self: the renderer
 *
 * Get the PKCS\#11 attributes, if any, set for this renderer to display.
 *
 * Returns: (allow-none) (transfer none): the attributes, owned by the renderer
 */
GckAttributes *
_gcr_certificate_request_renderer_get_attributes (GcrCertificateRequestRenderer *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST_RENDERER (self), NULL);
	return self->pv->attrs;
}

/**
 * gcr_certificate_request_renderer_set_attributes:
 * @self: the renderer
 * @attrs: (allow-none): attributes to set
 *
 * Set the PKCS\#11 attributes for this renderer to display. One of the attributes
 * should be a CKA_VALUE type attribute containing a DER encoded PKCS\#10
 * certificate request or an SPKAC request.
 */
void
_gcr_certificate_request_renderer_set_attributes (GcrCertificateRequestRenderer *self,
                                              GckAttributes *attrs)
{
	const GckAttribute *value;
	GNode *asn = NULL;
	gulong type = 0;
	GBytes *bytes;

	g_return_if_fail (GCR_IS_CERTIFICATE_REQUEST_RENDERER (self));

	if (attrs) {
		value = gck_attributes_find (attrs, CKA_VALUE);
		if (value == NULL) {
			g_warning ("no CKA_VALUE found in attributes passed to "
				   "GcrCertificateRequestRenderer attributes property");
			return;
		}

		bytes = g_bytes_new_with_free_func (value->value, value->length,
		                                      gck_attributes_unref, gck_attributes_ref (attrs));

		asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "pkcs-10-CertificationRequest", bytes);
		if (asn != NULL) {
			type = CKQ_GCR_PKCS10;
		} else {
			asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SignedPublicKeyAndChallenge", bytes);
			if (asn != NULL) {
				type = CKQ_GCR_SPKAC;
			} else {
				g_warning ("the data contained in the CKA_VALUE attribute passed to "
				           "GcrCertificateRequestRenderer was not valid DER encoded PKCS#10 "
				           "or SPKAC");
			}
		}

		g_bytes_unref (bytes);

		if (type == 0)
			return;

		gck_attributes_ref (attrs);
	}

	if (self->pv->attrs)
		gck_attributes_unref (self->pv->attrs);
	self->pv->attrs = attrs;
	self->pv->asn = asn;
	self->pv->type = type;
	self->pv->key_size = 0; /* calculated later */

	gcr_renderer_emit_data_changed (GCR_RENDERER (self));
	g_object_notify (G_OBJECT (self), "attributes");
}
