/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include <gcr-gtk4/gcr-certificate-widget.h>
#include "gcr/gcr-certificate-extensions.h"
#include "gcr/gcr-fingerprint.h"
#include "gcr/gcr-oids.h"

#include "gcr-section.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"
#include "egg/egg-oid.h"
#include "egg/egg-hex.h"

struct _GcrCertificateWidget
{
	GtkWidget parent_instance;

	GcrCertificate *certificate;
	GtkWidget *reveal_button;
	GtkWidget *revealer;
	GtkWidget *secondary_info;
	GtkSizeGroup *size_group;
};

G_DEFINE_TYPE (GcrCertificateWidget, gcr_certificate_widget, GTK_TYPE_WIDGET)

enum {
	PROP_CERTIFICATE = 1,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
gcr_certificate_widget_finalize (GObject *object)
{
	GtkWidget *child;
	GcrCertificateWidget *self = (GcrCertificateWidget *)object;

	g_clear_object (&self->size_group);
	while ((child = gtk_widget_get_first_child (GTK_WIDGET (self)))) {
		gtk_widget_unparent (child);
	}

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
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
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
	gtk_orientable_set_orientation (GTK_ORIENTABLE (gtk_widget_get_layout_manager (GTK_WIDGET (self))), GTK_ORIENTATION_VERTICAL);
	self->reveal_button = gtk_button_new_with_label ("…");
	gtk_widget_set_halign (self->reveal_button, GTK_ALIGN_CENTER);
	gtk_widget_insert_before (self->reveal_button, GTK_WIDGET (self), NULL);
	self->secondary_info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	self->revealer = g_object_new (GTK_TYPE_REVEALER,
				       "child", self->secondary_info,
				       "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
				       NULL);
	gtk_widget_insert_after (self->revealer, GTK_WIDGET (self), self->reveal_button);
	g_signal_connect (self->reveal_button, "clicked", G_CALLBACK (on_reveal_button_clicked), self);
	self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
}

static GtkWidget*
_gcr_certificate_widget_add_section (GcrCertificateWidget *self,
			             const gchar          *title,
				     gboolean              important)
{
	GtkWidget *widget;

	g_assert (GCR_IS_CERTIFICATE_WIDGET (self));

	widget = gcr_section_new (title);

	gtk_size_group_add_widget (self->size_group, widget);
	if (important)
		gtk_widget_insert_before (widget, GTK_WIDGET (self), self->reveal_button);
	else
		gtk_box_append (GTK_BOX (self->secondary_info), widget);

	return widget;
}

static GtkWidget*
create_value_label (const gchar *label)
{
	return g_object_new (GTK_TYPE_LABEL,
			     "label", label,
			     "xalign", 1.0,
			     "halign", GTK_ALIGN_END,
			     "hexpand", TRUE,
			     "selectable", TRUE,
			     "wrap", TRUE,
			     NULL);
}

static gchar*
calculate_label (GcrCertificateWidget *self)
{
	gchar *label;

	label = gcr_certificate_get_subject_cn (self->certificate);
	if (label != NULL)
		return label;

	return g_strdup (_("Certificate"));
}

static void
on_parsed_dn_part (guint index,
                   GQuark oid,
                   GNode *value,
                   gpointer user_data)
{
	GtkWidget *section = user_data;
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

	gcr_section_add_child (GCR_SECTION (section), field, create_value_label (display));

	g_free (field);
	g_free (display);
}

static inline gchar *
hex_encode_bytes (GBytes *bytes)
{
	gsize size;
	gconstpointer data = g_bytes_get_data (bytes, &size);
	return egg_hex_encode_full (data, size, TRUE, " ", 1);
}

static void
append_subject_public_key (GcrCertificateWidget *self,
                           GcrSection           *section,
                           GNode                *subject_public_key)
{
	guint key_nbits;
	const gchar *text;
	gchar *display;
	GBytes *value;
	guchar *raw;
	gsize n_raw;
	GQuark oid;
	guint bits;

	key_nbits = gcr_certificate_get_key_size (self->certificate);

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (subject_public_key,
	                                                  "algorithm", "algorithm", NULL));
	text = egg_oid_get_description (oid);
	gcr_section_add_child (section, _("Key Algorithm"), create_value_label (text));

	value = egg_asn1x_get_element_raw (egg_asn1x_node (subject_public_key,
	                                                   "algorithm", "parameters", NULL));
	if (value) {
		display = hex_encode_bytes (value);
		gcr_section_add_child (section, _("Key Parameters"), create_value_label (display));
		g_clear_pointer (&display, g_free);
		g_bytes_unref (value);
	}

	if (key_nbits > 0) {
		display = g_strdup_printf ("%u", key_nbits);
		gcr_section_add_child (section, _("Key Size"), create_value_label (display));
		g_clear_pointer (&display, g_free);
	}

	value = egg_asn1x_get_element_raw (subject_public_key);
	raw = gcr_fingerprint_from_subject_public_key_info (g_bytes_get_data (value, NULL),
	                                                    g_bytes_get_size (value),
	                                                    G_CHECKSUM_SHA1, &n_raw);
	g_bytes_unref (value);
	display = egg_hex_encode_full (raw, n_raw, TRUE, " ", 1);
	g_free (raw);
	gcr_section_add_child (section, _("Key SHA1 Fingerprint"), create_value_label (text));
	g_clear_pointer (&display, g_free);

	value = egg_asn1x_get_bits_as_raw (egg_asn1x_node (subject_public_key, "subjectPublicKey", NULL), &bits);
	display = egg_hex_encode_full (g_bytes_get_data (value, NULL), bits / 8, TRUE, " ", 1);
	gcr_section_add_child (section, _("Public Key"), create_value_label (text));
	g_clear_pointer (&display, g_free);
	g_bytes_unref (value);
}

static GcrSection *
append_extension_basic_constraints (GcrCertificateWidget *self,
                                    GBytes               *data)
{
	GcrSection *section;
	gboolean is_ca = FALSE;
	gint path_len = -1;
	gchar *number;

	if (!_gcr_certificate_extension_basic_constraints (data, &is_ca, &path_len))
		return NULL;

	section = GCR_SECTION (_gcr_certificate_widget_add_section (self, _("Basic Constraints"), FALSE));
	gcr_section_add_child (section, _("Certificate Authority"), create_value_label (is_ca ? _("Yes") : _("No")));

	number = g_strdup_printf ("%d", path_len);
	gcr_section_add_child (section, _("Max Path Length"), create_value_label (path_len < 0 ? _("Unlimited") : number));
	g_free (number);

	return section;
}

static GcrSection *
append_extension_extended_key_usage (GcrCertificateWidget *self,
                                     GBytes               *data)
{
	GcrSection *section;
	GQuark *oids;
	GString *text;
	guint i;

	oids = _gcr_certificate_extension_extended_key_usage (data);
	if (oids == NULL)
		return NULL;

	text = g_string_new ("");
	for (i = 0; oids[i] != 0; i++) {
		if (i > 0)
			g_string_append_unichar (text, '\n');
		g_string_append (text, egg_oid_get_description (oids[i]));
	}

	g_free (oids);

	section = GCR_SECTION (_gcr_certificate_widget_add_section (self, _("Extended Key Usage"), FALSE));
	gcr_section_add_child (section, _("Allowed Purposes"), create_value_label (text->str));

	g_string_free (text, TRUE);

	return section;
}

static GcrSection *
append_extension_subject_key_identifier (GcrCertificateWidget *self,
                                         GBytes *data)
{
	GcrSection *section;
	gpointer keyid;
	gsize n_keyid;

	keyid = _gcr_certificate_extension_subject_key_identifier (data, &n_keyid);
	if (keyid == NULL)
		return NULL;

	section = GCR_SECTION (_gcr_certificate_widget_add_section (self, _("Subject Key Identifier"), FALSE));
	gchar *display = egg_hex_encode_full (keyid, n_keyid, TRUE, " ", 1);
	g_free (keyid);
	gcr_section_add_child (section, _("Key Identifier"), create_value_label (display));
	g_free (display);

	return section;
}

static const struct {
	guint usage;
	const gchar *description;
} usage_descriptions[] = {
	{ GCR_KEY_USAGE_DIGITAL_SIGNATURE, N_("Digital signature") },
	{ GCR_KEY_USAGE_NON_REPUDIATION, N_("Non repudiation") },
	{ GCR_KEY_USAGE_KEY_ENCIPHERMENT, N_("Key encipherment") },
	{ GCR_KEY_USAGE_DATA_ENCIPHERMENT, N_("Data encipherment") },
	{ GCR_KEY_USAGE_KEY_AGREEMENT, N_("Key agreement") },
	{ GCR_KEY_USAGE_KEY_CERT_SIGN, N_("Certificate signature") },
	{ GCR_KEY_USAGE_CRL_SIGN, N_("Revocation list signature") },
	{ GCR_KEY_USAGE_ENCIPHER_ONLY, N_("Encipher only") },
	{ GCR_KEY_USAGE_DECIPHER_ONLY, N_("Decipher only") }
};

static GcrSection *
append_extension_key_usage (GcrCertificateWidget *self,
                            GBytes *data)
{
	GcrSection *section;
	gulong key_usage;
	GString *text;
	guint i;

	if (!_gcr_certificate_extension_key_usage (data, &key_usage))
		return NULL;

	text = g_string_new ("");

	for (i = 0; i < G_N_ELEMENTS (usage_descriptions); i++) {
		if (key_usage & usage_descriptions[i].usage) {
			if (text->len > 0)
				g_string_append_unichar (text, '\n');
			g_string_append (text, _(usage_descriptions[i].description));
		}
	}

	section = GCR_SECTION (_gcr_certificate_widget_add_section (self, _("Key Usage"), FALSE));
	gcr_section_add_child (section, _("Usages"), create_value_label (text->str));

	g_string_free (text, TRUE);

	return section;
}

static GcrSection *
append_extension_subject_alt_name (GcrCertificateWidget *self,
                                   GBytes *data)
{
	GcrSection *section;
	GArray *general_names;
	GcrGeneralName *general;
	guint i;

	general_names = _gcr_certificate_extension_subject_alt_name (data);
	if (general_names == NULL)
		return FALSE;

	section = GCR_SECTION (_gcr_certificate_widget_add_section (self, _("Subject Alternative Names"), FALSE));

	for (i = 0; i < general_names->len; i++) {
		general = &g_array_index (general_names, GcrGeneralName, i);
		if (general->display == NULL) {
			gchar *display = hex_encode_bytes (general->raw);
			gcr_section_add_child (section, general->description, create_value_label (display));
			g_free (display);
		} else
			gcr_section_add_child (section, general->description, create_value_label (general->display));
	}

	_gcr_general_names_free (general_names);

	return section;
}

static GcrSection *
append_extension_hex (GcrCertificateWidget *self,
                      GQuark oid,
                      GBytes *value)
{
	GcrSection *section;
	const gchar *text;
	gchar *display;

	section = GCR_SECTION (_gcr_certificate_widget_add_section (self, _("Extension"), FALSE));

	/* Extension type */
	text = egg_oid_get_description (oid);
	gcr_section_add_child (section, _("Identifier"), create_value_label (text));
	display = hex_encode_bytes (value);
	gcr_section_add_child (section, _("Value"), create_value_label (display));
	g_free (display);

	return section;
}

static void
append_extension (GcrCertificateWidget *self,
                  GNode *node)
{
	GQuark oid;
	GBytes *value;
	gboolean critical;
	GcrSection *section = NULL;

	/* Dig out the OID */
	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (node, "extnID", NULL));
	g_return_if_fail (oid);

	/* Extension value */
	value = egg_asn1x_get_string_as_bytes (egg_asn1x_node (node, "extnValue", NULL));

	/* The custom parsers */
	if (oid == GCR_OID_BASIC_CONSTRAINTS)
		section = append_extension_basic_constraints (self, value);
	else if (oid == GCR_OID_EXTENDED_KEY_USAGE)
		section = append_extension_extended_key_usage (self, value);
	else if (oid == GCR_OID_SUBJECT_KEY_IDENTIFIER)
		section = append_extension_subject_key_identifier (self, value);
	else if (oid == GCR_OID_KEY_USAGE)
		section = append_extension_key_usage (self, value);
	else if (oid == GCR_OID_SUBJECT_ALT_NAME)
		section = append_extension_subject_alt_name (self, value);

	/* Otherwise the default raw display */
	if (!section) {
		section = append_extension_hex (self, oid, value);
	}

	/* Critical */
	if (section && egg_asn1x_get_boolean (egg_asn1x_node (node, "critical", NULL), &critical)) {
		gcr_section_add_child (section, _("Critical"), create_value_label (critical ? _("Yes") : _("No")));
	}

	g_bytes_unref (value);
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
	GtkWidget *section, *label;
	PangoAttrList *attributes;
	gchar *display;
	GBytes *bytes, *number;
	GNode *asn, *subject_public_key;
	GQuark oid;
	gconstpointer data;
	gsize n_data;
	GDateTime *datetime;
	gulong version;
	guint bits, index;

	g_return_if_fail (GCR_IS_CERTIFICATE_WIDGET (self));

	g_set_object (&self->certificate, certificate);

	data = gcr_certificate_get_der_data (self->certificate, &n_data);
	if (!data) {
		g_set_object (&self->certificate, NULL);
	}

	display = calculate_label (self);
	section = _gcr_certificate_widget_add_section (self, display, TRUE);
	g_clear_pointer (&display, g_free);

	bytes = g_bytes_new_static (data, n_data);
	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "Certificate", bytes);
	g_bytes_unref (bytes);

	display = egg_dn_read_part (egg_asn1x_node (asn, "tbsCertificate", "subject", "rdnSequence", NULL), "CN");
	gcr_section_add_child (GCR_SECTION (section), _("Identity"), create_value_label (display));
	g_clear_pointer (&display, g_free);

	display = egg_dn_read_part (egg_asn1x_node (asn, "tbsCertificate", "issuer", "rdnSequence", NULL), "CN");
	gcr_section_add_child (GCR_SECTION (section), _("Verified by"), create_value_label (display));
	g_clear_pointer (&display, g_free);

	datetime = egg_asn1x_get_time_as_date_time (egg_asn1x_node (asn, "tbsCertificate", "validity", "notAfter", NULL));
	if (datetime) {
		display = g_date_time_format (datetime, "%x");
		g_return_if_fail (display != NULL);
		gcr_section_add_child (GCR_SECTION (section), _("Expires"), create_value_label (display));
		g_clear_pointer (&display, g_free);
		g_clear_pointer (&datetime, g_date_time_unref);
	}

	/* The subject */
	section = _gcr_certificate_widget_add_section (self, _("Subject Name"), FALSE);
	egg_dn_parse (egg_asn1x_node (asn, "tbsCertificate", "subject", "rdnSequence", NULL), on_parsed_dn_part, section);

	/* The Issuer */
	section = _gcr_certificate_widget_add_section (self, _("Issuer Name"), FALSE);
	egg_dn_parse (egg_asn1x_node (asn, "tbsCertificate", "issuer", "rdnSequence", NULL), on_parsed_dn_part, section);

	/* The Issued Parameters */
	section = _gcr_certificate_widget_add_section (self, _("Issued Certificate"), FALSE);

	if (!egg_asn1x_get_integer_as_ulong (egg_asn1x_node (asn, "tbsCertificate", "version", NULL), &version)) {
		g_critical ("Unable to parse certificate version");
	} else {
		display = g_strdup_printf ("%lu", version + 1);
		gcr_section_add_child (GCR_SECTION (section), _("Version"), create_value_label (display));
		g_clear_pointer (&display, g_free);
	}

	number = egg_asn1x_get_integer_as_raw (egg_asn1x_node (asn, "tbsCertificate", "serialNumber", NULL));
	if (!number) {
		g_critical ("Unable to parse certificate serial number");
	} else {
		display = hex_encode_bytes (number);
		gcr_section_add_child (GCR_SECTION (section), _("Serial Number"), create_value_label (display));
		g_clear_pointer (&display, g_free);
		g_bytes_unref (number);
	}

	datetime = egg_asn1x_get_time_as_date_time (egg_asn1x_node (asn, "tbsCertificate", "validity", "notBefore", NULL));
	if (datetime) {
		display = g_date_time_format (datetime, "%x");
		g_return_if_fail (display != NULL);
		gcr_section_add_child (GCR_SECTION (section), _("Not Valid Before"), create_value_label (display));
		g_clear_pointer (&display, g_free);
		g_clear_pointer (&datetime, g_date_time_unref);
	}
	datetime = egg_asn1x_get_time_as_date_time (egg_asn1x_node (asn, "tbsCertificate", "validity", "notAfter", NULL));
	if (datetime) {
		display = g_date_time_format (datetime, "%x");
		g_return_if_fail (display != NULL);
		gcr_section_add_child (GCR_SECTION (section), _("Not Valid After"), create_value_label (display));
		g_clear_pointer (&display, g_free);
		g_clear_pointer (&datetime, g_date_time_unref);
	}

	/* Fingerprints */
	section = _gcr_certificate_widget_add_section (self, _("Certificate Fingerprints"), FALSE);
	display = g_compute_checksum_for_bytes (G_CHECKSUM_SHA1, bytes);
	gcr_section_add_child (GCR_SECTION (section), "SHA1", create_value_label (display));
	g_clear_pointer (&display, g_free);
	display = g_compute_checksum_for_bytes (G_CHECKSUM_MD5, bytes);
	gcr_section_add_child (GCR_SECTION (section), "MD5", create_value_label (display));
	g_clear_pointer (&display, g_free);

	/* Public Key Info */
	section = _gcr_certificate_widget_add_section (self, _("Public Key Info"), FALSE);
	subject_public_key = egg_asn1x_node (asn, "tbsCertificate", "subjectPublicKeyInfo", NULL);
	append_subject_public_key (self, GCR_SECTION (section), subject_public_key);

	/* Extensions */
	for (index = 1; TRUE; ++index) {
		GNode *extension = egg_asn1x_node (asn, "tbsCertificate", "extensions", index, NULL);
		if (extension == NULL)
			break;
		append_extension (self, extension);
	}

	/* Signature */
	section = _gcr_certificate_widget_add_section (self, _("Signature"), FALSE);

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, "signatureAlgorithm", "algorithm", NULL));
	gcr_section_add_child (GCR_SECTION (section), _("Signature Algorithm"), create_value_label (egg_oid_get_description (oid)));

	bytes = egg_asn1x_get_element_raw (egg_asn1x_node (asn, "signatureAlgorithm", "parameters", NULL));
	if (bytes) {
		display = hex_encode_bytes (bytes);
		gcr_section_add_child (GCR_SECTION (section), _("Signature Parameters"), create_value_label (display));
		g_clear_pointer (&display, g_free);
		g_bytes_unref (bytes);
	}

	bytes = egg_asn1x_get_bits_as_raw (egg_asn1x_node (asn, "signature", NULL), &bits);
	display = egg_hex_encode_full (g_bytes_get_data (bytes, NULL), bits / 8, TRUE, " ", 1);
	g_bytes_unref (bytes);
	label = create_value_label (display);
	attributes = pango_attr_list_new ();
	pango_attr_list_insert (attributes, pango_attr_family_new ("Monospace"));
	gtk_label_set_attributes (GTK_LABEL (label), attributes);
	pango_attr_list_unref (attributes);
	gcr_section_add_child (GCR_SECTION (section), _("Signature"), label);
	g_clear_pointer (&display, g_free);

	egg_asn1x_destroy (asn);
}
