/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */
#include "config.h"

#include <gio/gio.h>

#include "gcr-certificate-extension-private.h"
#include "gcr-certificate-extensions-private.h"

#include "gcr/gcr-oids.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"
#include "egg/egg-oid.h"

#include <glib/gi18n-lib.h>

/* Parse error */

G_DEFINE_QUARK (gcr-certificate-extension-parse-error-quark, gcr_certificate_extension_parse_error)

static inline void
set_general_parse_error (GError **error, const char *message)
{
	g_set_error_literal (error,
	                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR,
	                     GCR_CERTIFICATE_EXTENSION_PARSE_ERROR_GENERAL,
	                     message);
}

/* Basic Constraints */

/**
 * GcrCertificateExtensionBasicConstraints:
 *
 * A certificate extension that can be used to identify the type of the
 * certificate subject (whether it is a certificate authority or not).
 */

struct _GcrCertificateExtensionBasicConstraints
{
	GcrCertificateExtension parent_instance;

	gboolean is_ca;
	int path_len_constraint;
};

G_DEFINE_TYPE (GcrCertificateExtensionBasicConstraints,
               gcr_certificate_extension_basic_constraints,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static void
gcr_certificate_extension_basic_constraints_class_init (GcrCertificateExtensionBasicConstraintsClass *klass)
{
}

static void
gcr_certificate_extension_basic_constraints_init (GcrCertificateExtensionBasicConstraints *self)
{
}

/**
 * gcr_certificate_extension_basic_constraints_is_ca:
 *
 * Returns whether the certificate us a certificate authority (CA) certificate
 * or an end entity certificate.
 *
 * Returns: The value of "cA".
 */
gboolean
gcr_certificate_extension_basic_constraints_is_ca (GcrCertificateExtensionBasicConstraints *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS (self), FALSE);

	return self->is_ca;
}

/**
 * gcr_certificate_extension_basic_constraints_get_path_len_constraint:
 *
 * Returns the maximum number of CAs that are allowed in the chain below this
 * certificate.
 *
 * If this is not set, this method returns -1.
 *
 * Note that this field doesn't really make sense if
 * [method@Gcr.CertificateExtensionBasicConstraints.is_ca] is false.
 *
 * Returns: The value of "pathLenConstraint", or -1 if not set.
 */
int
gcr_certificate_extension_basic_constraints_get_path_len_constraint (GcrCertificateExtensionBasicConstraints *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS (self), -1);

	return self->path_len_constraint;
}

GcrCertificateExtension *
_gcr_certificate_extension_basic_constraints_parse (GQuark oid,
                                                    gboolean critical,
                                                    GBytes *value,
                                                    GError **error)
{
	GcrCertificateExtensionBasicConstraints *ret = NULL;
	GNode *asn = NULL;
	GNode *node;
	unsigned long temp;
	gboolean is_ca;
	int path_len;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "BasicConstraints", value);
	if (asn == NULL) {
		set_general_parse_error (error,
		                         "Couldn't decode BasicConstraints");
		goto out;
	}

	node = egg_asn1x_node (asn, "pathLenConstraint", NULL);
	if (!egg_asn1x_have (node)) {
		path_len = -1;
	} else if (!egg_asn1x_get_integer_as_ulong (node, &temp)) {
		set_general_parse_error (error,
		                         "Couldn't decode pathLenConstraint as integer");
		goto out;
	} else {
		path_len = temp;
	}

	node = egg_asn1x_node (asn, "cA", NULL);
	if (!egg_asn1x_have (node)) {
		is_ca = FALSE;
	} else if (!egg_asn1x_get_boolean (node, &is_ca)) {
		set_general_parse_error (error,
		                         "Couldn't decode cA as boolean");
		goto out;
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_BASIC_CONSTRAINTS,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->is_ca = is_ca;
	ret->path_len_constraint = path_len;

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}

/*  Key Usage */

/**
 * GcrCertificateExtensionKeyUsage:
 *
 * A certificate extension that can be used to restrict the usages of a given
 * certificate.
 */

struct _GcrCertificateExtensionKeyUsage
{
	GcrCertificateExtension parent_instance;

	unsigned long usages;
};

G_DEFINE_TYPE (GcrCertificateExtensionKeyUsage,
               gcr_certificate_extension_key_usage,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static const struct {
	unsigned int usage;
	const char *description;
} key_usage_descriptions[] = {
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

static void
gcr_certificate_extension_key_usage_class_init (GcrCertificateExtensionKeyUsageClass *klass)
{
}

static void
gcr_certificate_extension_key_usage_init (GcrCertificateExtensionKeyUsage *self)
{
}

/**
 * gcr_certificate_extension_key_usage_get_usages:
 *
 * Returns the bit string describing the usages.
 */
unsigned long
gcr_certificate_extension_key_usage_get_usages (GcrCertificateExtensionKeyUsage *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_KEY_USAGE (self), 0);

	return self->usages;
}

/**
 * gcr_certificate_extension_key_usage_get_descriptions:
 *
 * Returns a user-friendly list of description of the key usages.
 *
 * Returns: (transfer full): The descriptions.
 */
GStrv
gcr_certificate_extension_key_usage_get_descriptions (GcrCertificateExtensionKeyUsage *self)
{
	GStrvBuilder *values;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_KEY_USAGE (self), NULL);

	values = g_strv_builder_new ();
	for (size_t i = 0; i < G_N_ELEMENTS (key_usage_descriptions); i++) {
		if (self->usages & key_usage_descriptions[i].usage) {
			g_strv_builder_add (values, _(key_usage_descriptions[i].description));
		}
	}

	return g_strv_builder_unref_to_strv (values);
}

static unsigned long
_gcr_reverse_bits (unsigned long num, unsigned int n_bits)
{
    unsigned long reverse_num = 0;
    for (unsigned int i = 0; i < n_bits; i++) {
        if ((num & (1 << i)))
            reverse_num |= 1 << ((n_bits - 1) - i);
    }
    return reverse_num;
}

GcrCertificateExtension *
_gcr_certificate_extension_key_usage_parse (GQuark oid,
                                            gboolean critical,
                                            GBytes *value,
                                            GError **error)
{
	GcrCertificateExtensionKeyUsage *ret = NULL;
	GNode *asn = NULL;
	unsigned long key_usage;
	unsigned int n_bits;

	g_return_val_if_fail (value != NULL, FALSE);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "KeyUsage", value);
	if (asn == NULL) {
		set_general_parse_error (error,
		                         "Couldn't decode KeyUsage");
		goto out;
	}

	if (!egg_asn1x_get_bits_as_ulong (asn, &key_usage, &n_bits)) {
		set_general_parse_error (error,
		                         "Couldn't parse KeyUsage as bits");
		goto out;
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_KEY_USAGE,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->usages = _gcr_reverse_bits (key_usage, n_bits);

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}

/* Extended Key Usage */

struct _GcrCertificateExtensionExtendedKeyUsage
{
	GcrCertificateExtension parent_instance;

	GQuark *oids;
};

G_DEFINE_TYPE (GcrCertificateExtensionExtendedKeyUsage,
               gcr_certificate_extension_extended_key_usage,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static void
gcr_certificate_extension_extended_key_usage_finalize (GObject *obj)
{
	GcrCertificateExtensionExtendedKeyUsage *self = GCR_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE (obj);

	G_OBJECT_CLASS (gcr_certificate_extension_extended_key_usage_parent_class)->finalize (obj);

	g_free (self->oids);
}

static void
gcr_certificate_extension_extended_key_usage_class_init (GcrCertificateExtensionExtendedKeyUsageClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_certificate_extension_extended_key_usage_finalize;
}

static void
gcr_certificate_extension_extended_key_usage_init (GcrCertificateExtensionExtendedKeyUsage *self)
{
	self->oids = NULL;
}

/**
 * gcr_certificate_extension_extended_key_usage_get_oids:
 *
 * Returns the list of OIDs of the extended key usages.
 *
 * Returns: (transfer full): The OIDs.
 */
GStrv
gcr_certificate_extension_extended_key_usage_get_oids (GcrCertificateExtensionExtendedKeyUsage *self)
{
	GStrvBuilder *text;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE (self), NULL);

	text = g_strv_builder_new ();
	for (size_t i = 0; self->oids[i] != 0; i++) {
		g_strv_builder_add (text, g_quark_to_string (self->oids[i]));
	}

	return g_strv_builder_unref_to_strv (text);
}

/**
 * gcr_certificate_extension_extended_key_usage_get_descriptions:
 *
 * Returns a user-friendly list of description of the key usages.
 *
 * Returns: (transfer full): The descriptions.
 */
GStrv
gcr_certificate_extension_extended_key_usage_get_descriptions (GcrCertificateExtensionExtendedKeyUsage *self)
{
	GStrvBuilder *text;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE (self), NULL);

	text = g_strv_builder_new ();
	for (size_t i = 0; self->oids[i] != 0; i++) {
		g_strv_builder_add (text, egg_oid_get_description (self->oids[i]));
	}

	return g_strv_builder_unref_to_strv (text);
}

GcrCertificateExtension *
_gcr_certificate_extension_extended_key_usage_parse (GQuark oid,
                                                     gboolean critical,
                                                     GBytes *value,
                                                     GError **error)
{
	GcrCertificateExtensionExtendedKeyUsage *ret = NULL;
	GNode *asn = NULL;
	GArray *array;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "ExtKeyUsageSyntax", value);
	if (asn == NULL) {
		set_general_parse_error (error,
		                         "Couldn't decode ExtKeyUsageSyntax");
		goto out;
	}

	array = g_array_new (TRUE, TRUE, sizeof (GQuark));
	for (int i = 0; TRUE; ++i) {
		GNode *node;
		GQuark oid;

		node = egg_asn1x_node (asn, i + 1, NULL);
		if (node == NULL)
			break;
		oid = egg_asn1x_get_oid_as_quark (node);
		g_array_append_val (array, oid);
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_EXTENDED_KEY_USAGE,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->oids = (GQuark*) g_array_free (array, FALSE);

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}

/* Subject Key Identifier */

struct _GcrCertificateExtensionSubjectKeyIdentifier
{
	GcrCertificateExtension parent_instance;

	GBytes *keyid;
};

G_DEFINE_TYPE (GcrCertificateExtensionSubjectKeyIdentifier,
               gcr_certificate_extension_subject_key_identifier,
               GCR_TYPE_CERTIFICATE_EXTENSION)

static void
gcr_certificate_extension_subject_key_identifier_finalize (GObject *obj)
{
	GcrCertificateExtensionSubjectKeyIdentifier *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER (obj);

	G_OBJECT_CLASS (gcr_certificate_extension_subject_key_identifier_parent_class)->finalize (obj);

	g_bytes_unref (self->keyid);
}

static void
gcr_certificate_extension_subject_key_identifier_class_init (GcrCertificateExtensionSubjectKeyIdentifierClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_certificate_extension_subject_key_identifier_finalize;
}

static void
gcr_certificate_extension_subject_key_identifier_init (GcrCertificateExtensionSubjectKeyIdentifier *self)
{
	self->keyid = NULL;
}

/**
 * gcr_certificate_extension_subject_key_identifier_get_key_id:
 *
 * Returns the raw bytes containing the subject key identifier.
 *
 * Returns: (transfer none): The subject key identifier.
 */
GBytes *
gcr_certificate_extension_subject_key_identifier_get_key_id (GcrCertificateExtensionSubjectKeyIdentifier *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER (self), NULL);

	return self->keyid;
}

GcrCertificateExtension *
_gcr_certificate_extension_subject_key_identifier_parse (GQuark oid,
                                                         gboolean critical,
                                                         GBytes *value,
                                                         GError **error)
{
	GcrCertificateExtensionSubjectKeyIdentifier *ret = NULL;
	GNode *asn = NULL;
	void *data = NULL;
	size_t data_len;

	g_return_val_if_fail (value != NULL, NULL);

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SubjectKeyIdentifier", value);
	if (asn == NULL) {
		set_general_parse_error (error,
		                         "Couldn't decode SubjectKeyIdentifier");
		goto out;
	}

	data = egg_asn1x_get_string_as_raw (asn, g_realloc, &data_len);

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_SUBJECT_KEY_IDENTIFIER,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	ret->keyid = g_bytes_new_take (data, data_len);

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}

/* Subject Alternative Name */

/**
 * GcrGeneralName:
 *
 * An object describing a name as part of the Subject Alternative Name (SAN)
 * extension.
 */

struct _GcrGeneralName {
	GObject parent_instace;

	GcrGeneralNameType type;
	const char *description;
	char *display;
	GBytes *raw;
};

G_DEFINE_TYPE (GcrGeneralName, gcr_general_name, G_TYPE_OBJECT)

static void
gcr_general_name_finalize (GObject *obj)
{
	GcrGeneralName *self = GCR_GENERAL_NAME (obj);

	G_OBJECT_CLASS (gcr_general_name_parent_class)->finalize (obj);

	g_free (self->display);
	g_bytes_unref (self->raw);
}

static void
gcr_general_name_class_init (GcrGeneralNameClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_general_name_finalize;
}

static void
gcr_general_name_init (GcrGeneralName *self)
{
}

/**
 * gcr_general_name_get_description:
 *
 * Returns a user-friendly string describing the name.
 */
const char *
gcr_general_name_get_description (GcrGeneralName  *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), NULL);

	return self->description;
}

/**
 * gcr_general_name_get_value:
 *
 * Returns the actual value of the name.
 */
const char *
gcr_general_name_get_value (GcrGeneralName  *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), NULL);

	return self->display;
}

/**
 * gcr_general_name_get_value_raw:
 *
 * Returns the raw bytes describing the value of the name.
 */
GBytes *
gcr_general_name_get_value_raw (GcrGeneralName  *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), NULL);
	return self->raw;
}

GcrGeneralNameType
_gcr_general_name_get_name_type (GcrGeneralName  *self)
{
	g_return_val_if_fail (GCR_IS_GENERAL_NAME (self), 0);
	return self->type;
}

static void
general_name_parse_other (GNode *node, GcrGeneralName *general)
{
	GNode *decode = NULL;
	GQuark oid;
	GNode *any;

	general->type = GCR_GENERAL_NAME_OTHER;
	general->description = _("Other Name");
	general->display = NULL;

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (node, "type-id", NULL));
	any = egg_asn1x_node (node, "value", NULL);

	if (any == NULL)
		return;

	if (oid == GCR_OID_ALT_NAME_XMPP_ADDR) {
		general->description = _("XMPP Addr");
		decode = egg_asn1x_get_any_as_string (any, EGG_ASN1X_UTF8_STRING);
		general->display = egg_asn1x_get_string_as_utf8 (decode, g_realloc);
	} else if (oid == GCR_OID_ALT_NAME_DNS_SRV) {
		general->description = _("DNS SRV");
		decode = egg_asn1x_get_any_as_string (any, EGG_ASN1X_IA5_STRING);
		general->display = egg_asn1x_get_string_as_utf8 (decode, g_realloc);
	}

	egg_asn1x_destroy (decode);
}

static void
general_name_parse_rfc822 (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_RFC822;
	general->description = _("Email");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_dns (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_DNS;
	general->description = _("DNS");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_x400 (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_X400;
	general->description = _("X400 Address");
}

static void
general_name_parse_dn (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_DNS;
	general->description = _("Directory Name");
	general->display = egg_dn_read (node);
}

static void
general_name_parse_edi (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_EDI;
	general->description = _("EDI Party Name");
}

static void
general_name_parse_uri (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_URI;
	general->description = _("URI");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_ip (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_IP;
	general->description = _("IP Address");
	general->display = egg_asn1x_get_string_as_utf8 (node, g_realloc);
}

static void
general_name_parse_registered (GNode *node, GcrGeneralName *general)
{
	general->type = GCR_GENERAL_NAME_REGISTERED_ID;
	general->description = _("Registered ID");
	general->display = egg_asn1x_get_oid_as_string (node);
}


/**
 * GcrCertificateExtensionSubjectAltName:
 *
 * A certificate extension describing the Subject Alternative Name (SAN).
 *
 * This kind of extension is used for example to specify multiple domains for
 * the same certificate.
 *
 * The object exposes the different names with the [iface@Gio.ListModel] API.
 */

struct _GcrCertificateExtensionSubjectAltName
{
	GcrCertificateExtension parent_instance;

	GPtrArray *names;
};

static void san_list_model_interface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrCertificateExtensionSubjectAltName,
                         gcr_certificate_extension_subject_alt_name,
                         GCR_TYPE_CERTIFICATE_EXTENSION,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                san_list_model_interface_init))

static GType
gcr_certificate_extension_subject_alt_name_get_item_type (GListModel *model)
{
	return GCR_TYPE_GENERAL_NAME;
}

static unsigned int
gcr_certificate_extension_subject_alt_name_get_n_items (GListModel *model)
{
	GcrCertificateExtensionSubjectAltName *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (model);

	return self->names->len;
}

static void *
gcr_certificate_extension_subject_alt_name_get_item (GListModel   *model,
                                                     unsigned int  position)
{
	GcrCertificateExtensionSubjectAltName *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (model);

	if (position >= self->names->len)
		return NULL;

	return g_ptr_array_index (self->names, position);
}

static void
san_list_model_interface_init (GListModelInterface *iface)
{
	iface->get_item_type = gcr_certificate_extension_subject_alt_name_get_item_type;
	iface->get_n_items = gcr_certificate_extension_subject_alt_name_get_n_items;
	iface->get_item = gcr_certificate_extension_subject_alt_name_get_item;
}

static void
gcr_certificate_extension_subject_alt_name_finalize (GObject *obj)
{
	GcrCertificateExtensionSubjectAltName *self = GCR_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (obj);

	G_OBJECT_CLASS (gcr_certificate_extension_subject_alt_name_parent_class)->finalize (obj);

	g_ptr_array_unref (self->names);
}

static void
gcr_certificate_extension_subject_alt_name_class_init (GcrCertificateExtensionSubjectAltNameClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_certificate_extension_subject_alt_name_finalize;
}

static void
gcr_certificate_extension_subject_alt_name_init (GcrCertificateExtensionSubjectAltName *self)
{
	self->names = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * gcr_certificate_extension_subject_alt_name_get_name:
 *
 * Returns the name at the given position.
 *
 * Returns: (transfer none): The name at position @i
 */
GcrGeneralName *
gcr_certificate_extension_subject_alt_name_get_name (GcrCertificateExtensionSubjectAltName *self,
                                                     unsigned int position)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME (self), NULL);
	g_return_val_if_fail (position < self->names->len, NULL);

	return g_ptr_array_index (self->names, position);
}


GcrCertificateExtension *
_gcr_certificate_extension_subject_alt_name_parse (GQuark oid,
                                                   gboolean critical,
                                                   GBytes *value,
                                                   GError **error)
{
	GcrCertificateExtensionSubjectAltName *ret = NULL;
	GNode *asn = NULL;
	GPtrArray *names;
	unsigned int count;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SubjectAltName", value);
	if (asn == NULL) {
		set_general_parse_error (error,
		                         "Couldn't decode SubjectAltName");
		goto out;
	}

	count = egg_asn1x_count (asn);
	names = g_ptr_array_new_full (count, g_object_unref);

	for (unsigned int i = 0; i < count; i++) {
		GNode *choice;
		const char *node_name;
		GcrGeneralName *general;

		choice = egg_asn1x_get_choice (egg_asn1x_node (asn, i + 1, NULL));
		g_return_val_if_fail (choice, NULL);

		node_name = egg_asn1x_name (choice);
		g_return_val_if_fail (node_name, NULL);

		general = g_object_new (GCR_TYPE_GENERAL_NAME, NULL);

		if (g_str_equal (node_name, "otherName"))
			general_name_parse_other (choice, general);

		else if (g_str_equal (node_name, "rfc822Name"))
			general_name_parse_rfc822 (choice, general);

		else if (g_str_equal (node_name, "dNSName"))
			general_name_parse_dns (choice, general);

		else if (g_str_equal (node_name, "x400Address"))
			general_name_parse_x400 (choice, general);

		else if (g_str_equal (node_name, "directoryName"))
			general_name_parse_dn (choice, general);

		else if (g_str_equal (node_name, "ediPartyName"))
			general_name_parse_edi (choice, general);

		else if (g_str_equal (node_name, "uniformResourceIdentifier"))
			general_name_parse_uri (choice, general);

		else if (g_str_equal (node_name, "iPAddress"))
			general_name_parse_ip (choice, general);

		else if (g_str_equal (node_name, "registeredID"))
			general_name_parse_registered (choice, general);

		general->raw = egg_asn1x_get_element_raw (choice);
		g_ptr_array_add (names, general);
	}

	ret = g_object_new (GCR_TYPE_CERTIFICATE_EXTENSION_SUBJECT_ALT_NAME,
	                    "critical", critical,
	                    "value", value,
	                    NULL);
	_gcr_certificate_extension_set_oid (GCR_CERTIFICATE_EXTENSION (ret), oid);
	g_ptr_array_extend_and_steal (ret->names, g_steal_pointer (&names));

out:
	if (asn != NULL)
		egg_asn1x_destroy (asn);
	return ret ? GCR_CERTIFICATE_EXTENSION (ret) : NULL;
}
