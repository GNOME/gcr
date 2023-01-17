/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "gcr-certificate.h"
#include "gcr-certificate-extensions.h"
#include "gcr-certificate-field.h"
#include "gcr-certificate-field-private.h"
#include "gcr-fingerprint.h"
#include "gcr-internal.h"
#include "gcr-subject-public-key.h"

#include "gcr/gcr-oids.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"
#include "egg/egg-hex.h"
#include "egg/egg-oid.h"

#include <string.h>
#include <glib/gi18n-lib.h>

/**
 * GcrCertificate:
 *
 * An interface that represents an X.509 certificate.
 *
 * Objects can implement this interface to make a certificate usable with the
 * GCR library.
 *
 * Various methods are available to parse out relevant bits of the certificate.
 * However no verification of the validity of a certificate is done here. Use
 * your favorite crypto library to do this.
 *
 * You can use [class@SimpleCertificate] to simply load a certificate for which
 * you already have the raw certificate data.
 *
 * The #GcrCertificate interface has several properties that must be implemented.
 * You can use a mixin to implement these properties if desired. See the
 * gcr_certificate_mixin_class_init() and gcr_certificate_mixin_get_property()
 * functions.
 */

/**
 * GcrCertificateIface:
 * @parent: the parent interface type
 * @get_der_data: a method which returns the RAW der data of the certificate
 *
 * The interface that implementors of #GcrCertificate must implement.
 */

/*
 * The DER data in this structure is owned by the derived class.
 * It is only valid for the duration of the current call stack
 * after we call gcr_certificate_get_der_data(). We shouldn't
 * save it anywhere else.
 *
 * We keep the pointer around and compare it so that if the derived
 * class returns exactly the same pointer and size, then we can
 * keep from parsing things over again.
 */

typedef struct _GcrCertificateInfo {
	gconstpointer der;
	gsize n_der;
	GNode *asn1;
	guint key_size;
} GcrCertificateInfo;

/* Forward declarations */

static GBytes * _gcr_certificate_get_subject_const (GcrCertificate *self);
static GBytes * _gcr_certificate_get_issuer_const (GcrCertificate *self);

enum {
	PROP_FIRST = 0x0007000,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_SUBJECT_NAME,
	PROP_ISSUER_NAME,
	PROP_EXPIRY_DATE
};

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static GQuark CERTIFICATE_INFO = 0;

static void
certificate_info_free (gpointer data)
{
	GcrCertificateInfo *info = data;
	if (info) {
		g_assert (info->asn1);
		egg_asn1x_destroy (info->asn1);
		g_free (info);
	}
}

static GcrCertificateInfo*
certificate_info_load (GcrCertificate *cert)
{
	GcrCertificateInfo *info;
	GBytes *bytes;
	GNode *asn1;
	gconstpointer der;
	gsize n_der;

	g_assert (GCR_IS_CERTIFICATE (cert));

	der = gcr_certificate_get_der_data (cert, &n_der);
	if (der == NULL)
		return NULL;

	info = g_object_get_qdata (G_OBJECT (cert), CERTIFICATE_INFO);
	if (info != NULL) {
		if (n_der == info->n_der && der == info->der)
			return info;
	}

	/* TODO: Once GBytes is public, add to GcrCertificate interface */
	bytes = g_bytes_new_static (der, n_der);

	/* Cache is invalid or non existent */
	asn1 = egg_asn1x_create_and_decode (pkix_asn1_tab, "Certificate", bytes);

	g_bytes_unref (bytes);

	if (asn1 == NULL) {
		g_warning ("a derived class provided an invalid or unparseable X.509 DER certificate data.");
		return NULL;
	}

	info = g_new0 (GcrCertificateInfo, 1);
	info->der = der;
	info->n_der = n_der;
	info->asn1 = asn1;

	g_object_set_qdata_full (G_OBJECT (cert), CERTIFICATE_INFO, info, certificate_info_free);
	return info;
}

static GChecksum*
digest_certificate (GcrCertificate *self, GChecksumType type)
{
	GChecksum *digest;
	gconstpointer der;
	gsize n_der;

	g_assert (GCR_IS_CERTIFICATE (self));

	der = gcr_certificate_get_der_data (self, &n_der);
	if (der == NULL)
		return NULL;

	digest = g_checksum_new (type);
	g_return_val_if_fail (digest, NULL);

	g_checksum_update (digest, der, n_der);
	return digest;
}

/* ---------------------------------------------------------------------------------
 * INTERFACE
 */

static void
gcr_certificate_default_init (GcrCertificateIface *iface)
{
	static size_t initialized = 0;

	if (g_once_init_enter (&initialized)) {
		CERTIFICATE_INFO = g_quark_from_static_string ("_gcr_certificate_certificate_info");

		/**
		 * GcrCertificate:label:
		 *
		 * A readable label for this certificate.
		 */
		g_object_interface_install_property (iface,
		         g_param_spec_string ("label", "Label", "Certificate label",
		                              "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrCertificate:description:
		 *
		 * A readable description for this certificate
		 */
		g_object_interface_install_property (iface,
		         g_param_spec_string ("description", "Description", "Description of object being rendered",
		                              "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrCertificate:subject:
		 *
		 * Common name part of the certificate subject
		 */
		g_object_interface_install_property (iface,
		           g_param_spec_string ("subject-name", "Subject name", "Common name of subject",
		                                "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrCertificate:issuer-name:
		 *
		 * Common name part of the certificate issuer
		 */
		g_object_interface_install_property (iface,
		           g_param_spec_string ("issuer-name", "Issuer name", "Common name of issuer",
		                                "", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrCertificate:expiry-date:
		 *
		 * The expiry date of the certificate
		 */
		g_object_interface_install_property (iface,
		           g_param_spec_boxed ("expiry-date", "Expiry date", "Certificate expiry date",
		                               G_TYPE_DATE_TIME, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		g_once_init_leave (&initialized, 1);
	}
}

typedef GcrCertificateIface GcrCertificateInterface;

G_DEFINE_INTERFACE (GcrCertificate, gcr_certificate, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * PUBLIC
 */


/**
 * gcr_certificate_get_der_data: (virtual get_der_data)
 * @self: a #GcrCertificate
 * @n_data: a pointer to a location to store the size of the resulting DER data.
 *
 * Gets the raw DER data for an X.509 certificate.
 *
 * Returns: (transfer none) (array length=n_data): raw DER data of the X.509 certificate
 **/
const guint8 *
gcr_certificate_get_der_data (GcrCertificate *self,
                              gsize *n_data)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (n_data != NULL, NULL);
	g_return_val_if_fail (GCR_CERTIFICATE_GET_INTERFACE (self)->get_der_data, NULL);
	return GCR_CERTIFICATE_GET_INTERFACE (self)->get_der_data (self, n_data);
}

/**
 * gcr_certificate_get_issuer_name:
 * @self: a #GcrCertificate
 *
 * Get a name to represent the issuer of this certificate.
 *
 * This will try to lookup the common name, orianizational unit,
 * organization in that order.
 *
 * Returns: (nullable): the allocated issuer name, or %NULL if no issuer name
 */
gchar *
gcr_certificate_get_issuer_name (GcrCertificate *self)
{
	gchar *name;

	name = gcr_certificate_get_issuer_part (self, "cn");
	if (name == NULL)
		name = gcr_certificate_get_issuer_part (self, "ou");
	if (name == NULL)
		name = gcr_certificate_get_issuer_part (self, "o");

	return name;
}

/**
 * gcr_certificate_get_issuer_cn:
 * @self: a #GcrCertificate
 *
 * Get the common name of the issuer of this certificate.
 *
 * The string returned should be freed by the caller when no longer
 * required.
 *
 * Returns: (nullable): The allocated issuer CN, or %NULL if no issuer CN present.
 */
gchar*
gcr_certificate_get_issuer_cn (GcrCertificate *self)
{
	return gcr_certificate_get_issuer_part (self, "cn");
}

/**
 * gcr_certificate_get_issuer_part:
 * @self: a #GcrCertificate
 * @part: a DN type string or OID.
 *
 * Get a part of the DN of the issuer of this certificate.
 *
 * Examples of a @part might be the 'OU' (organizational unit)
 * or the 'CN' (common name). Only the value of that part
 * of the DN is returned.
 *
 * The string returned should be freed by the caller when no longer
 * required.
 *
 * Returns: (nullable): the allocated part of the issuer DN, or %NULL if no
 *          such part is present
 */
gchar *
gcr_certificate_get_issuer_part (GcrCertificate *self, const char *part)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (part != NULL, NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_dn_read_part (egg_asn1x_node (info->asn1, "tbsCertificate", "issuer", "rdnSequence", NULL), part);
}

static GBytes *
_gcr_certificate_get_issuer_const (GcrCertificate *self)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_asn1x_get_element_raw (egg_asn1x_node (info->asn1, "tbsCertificate", "issuer", NULL));
}

/**
 * gcr_certificate_get_issuer_raw:
 * @self: a #GcrCertificate
 * @n_data: (out): The length of the returned data.
 *
 * Get the raw DER data for the issuer DN of the certificate.
 *
 * The data should be freed by using g_free() when no longer required.
 *
 * Returns: (transfer full) (array length=n_data) (nullable): allocated memory
 *          containing the raw issuer
 */
guchar *
gcr_certificate_get_issuer_raw (GcrCertificate *self,
                                gsize *n_data)
{
	GBytes *bytes;
	guchar *result;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (n_data != NULL, NULL);

	bytes = _gcr_certificate_get_issuer_const (self);
	if (bytes == NULL) {
		*n_data = 0;
		return NULL;
	}

	*n_data = g_bytes_get_size (bytes);
	result = g_memdup2 (g_bytes_get_data (bytes, NULL), *n_data);
	g_bytes_unref (bytes);

	return result;
}

/**
 * gcr_certificate_is_issuer:
 * @self: a #GcrCertificate
 * @issuer: a possible issuer #GcrCertificate
 *
 * Check if @issuer could be the issuer of this certificate. This is done by
 * comparing the relevant subject and issuer fields. No signature check is
 * done. Proper verification of certificates must be done via a crypto
 * library.
 *
 * Returns: whether @issuer could be the issuer of the certificate.
 */
gboolean
gcr_certificate_is_issuer (GcrCertificate *self, GcrCertificate *issuer)
{
	GBytes *subject_dn;
	GBytes *issuer_dn;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), FALSE);
	g_return_val_if_fail (GCR_IS_CERTIFICATE (issuer), FALSE);

	subject_dn = _gcr_certificate_get_subject_const (issuer);
	if (subject_dn == NULL)
		return FALSE;

	issuer_dn = _gcr_certificate_get_issuer_const (self);
	if (issuer_dn == NULL)
		return FALSE;

	ret = g_bytes_equal (subject_dn, issuer_dn);

	g_bytes_unref (subject_dn);
	g_bytes_unref (issuer_dn);

	return ret;
}

/**
 * gcr_certificate_get_issuer_dn:
 * @self: a #GcrCertificate
 *
 * Get the full issuer DN of the certificate as a (mostly)
 * readable string.
 *
 * The string returned should be freed by the caller when no longer
 * required.
 *
 * Returns: (nullable): The allocated issuer DN of the certificate.
 */
gchar*
gcr_certificate_get_issuer_dn (GcrCertificate *self)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_dn_read (egg_asn1x_node (info->asn1, "tbsCertificate", "issuer", "rdnSequence", NULL));
}

/**
 * gcr_certificate_get_subject_cn:
 * @self: a #GcrCertificate
 *
 * Get the common name of the subject of this certificate.
 *
 * The string returned should be freed by the caller when no longer
 * required.
 *
 * Returns: (nullable): The allocated subject CN, or %NULL if no subject CN present.
 */
gchar*
gcr_certificate_get_subject_cn (GcrCertificate *self)
{
	return gcr_certificate_get_subject_part (self, "cn");
}

/**
 * gcr_certificate_get_subject_name:
 * @self: a #GcrCertificate
 *
 * Get a name to represent the subject of this certificate.
 *
 * This will try to lookup the common name, orianizational unit,
 * organization in that order.
 *
 * Returns: (nullable): the allocated subject name, or %NULL if no subject name
 */
gchar *
gcr_certificate_get_subject_name (GcrCertificate *self)
{
	gchar *name;

	name = gcr_certificate_get_subject_part (self, "cn");
	if (name == NULL)
		name = gcr_certificate_get_subject_part (self, "ou");
	if (name == NULL)
		name = gcr_certificate_get_subject_part (self, "o");

	return name;
}

/**
 * gcr_certificate_get_subject_part:
 * @self: a #GcrCertificate
 * @part: a DN type string or OID.
 *
 * Get a part of the DN of the subject of this certificate.
 *
 * Examples of a @part might be the 'OU' (organizational unit)
 * or the 'CN' (common name). Only the value of that part
 * of the DN is returned.
 *
 * The string returned should be freed by the caller when no longer
 * required.
 *
 * Returns: (nullable): the allocated part of the subject DN, or %NULL if no
 *          such part is present.
 */
gchar*
gcr_certificate_get_subject_part (GcrCertificate *self, const char *part)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (part != NULL, NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_dn_read_part (egg_asn1x_node (info->asn1, "tbsCertificate", "subject", "rdnSequence", NULL), part);
}

/**
 * gcr_certificate_get_subject_dn:
 * @self: a #GcrCertificate
 *
 * Get the full subject DN of the certificate as a (mostly)
 * readable string.
 *
 * The string returned should be freed by the caller when no longer
 * required.
 *
 * Returns: (nullable): The allocated subject DN of the certificate.
 */
gchar*
gcr_certificate_get_subject_dn (GcrCertificate *self)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_dn_read (egg_asn1x_node (info->asn1, "tbsCertificate", "subject", "rdnSequence", NULL));
}

static GBytes *
_gcr_certificate_get_subject_const (GcrCertificate *self)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_asn1x_get_element_raw (egg_asn1x_node (info->asn1, "tbsCertificate", "subject", NULL));
}

/**
 * gcr_certificate_get_subject_raw:
 * @self: a #GcrCertificate
 * @n_data: (out): The length of the returned data.
 *
 * Get the raw DER data for the subject DN of the certificate.
 *
 * The data should be freed by using g_free() when no longer required.
 *
 * Returns: (transfer full) (array length=n_data) (nullable): allocated memory
 *          containing the raw subject
 */
guchar *
gcr_certificate_get_subject_raw (GcrCertificate *self, gsize *n_data)
{
	GBytes *bytes;
	guchar *result;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (n_data != NULL, NULL);

	bytes = _gcr_certificate_get_subject_const (self);
	if (bytes == NULL) {
		*n_data = 0;
		return NULL;
	}

	*n_data = g_bytes_get_size (bytes);
	result = g_memdup2 (g_bytes_get_data (bytes, NULL), *n_data);

	g_bytes_unref (bytes);

	return result;
}

/**
 * gcr_certificate_get_issued_date:
 * @self: a #GcrCertificate
 *
 * Get the issued date of this certificate.
 *
 * Returns: (nullable): A issued date of this certificate.
 */
GDateTime *
gcr_certificate_get_issued_date (GcrCertificate *self)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_asn1x_get_time_as_date_time (egg_asn1x_node (info->asn1, "tbsCertificate", "validity", "notBefore", NULL));
}

/**
 * gcr_certificate_get_expiry_date:
 * @self: a #GcrCertificate
 *
 * Get the expiry date of this certificate.
 *
 * Returns: (nullable): An expiry date of this certificate.
 */
GDateTime *
gcr_certificate_get_expiry_date (GcrCertificate *self)
{
	GcrCertificateInfo *info;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	info = certificate_info_load (self);
	if (info == NULL)
		return NULL;

	return egg_asn1x_get_time_as_date_time (egg_asn1x_node (info->asn1, "tbsCertificate", "validity", "notAfter", NULL));
}

/**
 * gcr_certificate_get_key_size:
 * @self: a #GcrCertificate
 *
 * Get the key size in bits of the public key represented
 * by this certificate.
 *
 * Returns: The key size of the certificate.
 */
guint
gcr_certificate_get_key_size (GcrCertificate *self)
{
	GcrCertificateInfo *info;
	GNode *subject_public_key;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), 0);

	info = certificate_info_load (self);
	if (info == NULL)
		return 0;

	if (!info->key_size) {
		subject_public_key = egg_asn1x_node (info->asn1, "tbsCertificate",
		                                     "subjectPublicKeyInfo", NULL);
		info->key_size = _gcr_subject_public_key_calculate_size (subject_public_key);
	}

	return info->key_size;
}

/**
 * gcr_certificate_get_fingerprint:
 * @self: a #GcrCertificate
 * @type: the type of algorithm for the fingerprint.
 * @n_length: (out): The length of the resulting fingerprint.
 *
 * Calculate the fingerprint for this certificate.
 *
 * The caller should free the returned data using g_free() when
 * it is no longer required.
 *
 * Returns: (array length=n_length) (nullable): the raw binary fingerprint
 **/
guchar *
gcr_certificate_get_fingerprint (GcrCertificate *self, GChecksumType type, gsize *n_length)
{
	GChecksum *sum;
	guchar *digest;
	gssize length;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (n_length != NULL, NULL);

	sum = digest_certificate (self, type);
	if (sum == NULL) {
		*n_length = 0;
		return NULL;
	}

	length = g_checksum_type_get_length (type);
	g_return_val_if_fail (length > 0, NULL);
	digest = g_malloc (length);
	*n_length = length;
	g_checksum_get_digest (sum, digest, n_length);
	g_checksum_free (sum);

	return digest;
}

/**
 * gcr_certificate_get_fingerprint_hex:
 * @self: a #GcrCertificate
 * @type: the type of algorithm for the fingerprint.
 *
 * Calculate the fingerprint for this certificate, and return it
 * as a hex string.
 *
 * The caller should free the returned data using g_free() when
 * it is no longer required.
 *
 * Returns: (nullable): an allocated hex string which contains the fingerprint.
 */
gchar*
gcr_certificate_get_fingerprint_hex (GcrCertificate *self, GChecksumType type)
{
	GChecksum *sum;
	guchar *digest;
	gsize n_digest;
	gssize length;
	gchar *hex;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	sum = digest_certificate (self, type);
	if (sum == NULL)
		return NULL;

	length = g_checksum_type_get_length (type);
	g_return_val_if_fail (length > 0, NULL);
	digest = g_malloc (length);
	n_digest = length;
	g_checksum_get_digest (sum, digest, &n_digest);
	hex = egg_hex_encode_full (digest, n_digest, TRUE, " ", 1);
	g_checksum_free (sum);
	g_free (digest);
	return hex;
}

/**
 * gcr_certificate_get_serial_number:
 * @self: a #GcrCertificate
 * @n_length: (out): the length of the returned data.
 *
 * Get the raw binary serial number of the certificate.
 *
 * The caller should free the returned data using g_free() when
 * it is no longer required.
 *
 * Returns: (array length=n_length) (nullable): the raw binary serial number.
 */
guchar *
gcr_certificate_get_serial_number (GcrCertificate *self, gsize *n_length)
{
	GcrCertificateInfo *info;
	GBytes *bytes;
	guchar *result;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (n_length != NULL, NULL);

	info = certificate_info_load (self);
	if (info == NULL) {
		*n_length = 0;
		return NULL;
	}

	bytes = egg_asn1x_get_integer_as_raw (egg_asn1x_node (info->asn1, "tbsCertificate", "serialNumber", NULL));
	g_return_val_if_fail (bytes != NULL, NULL);

	*n_length = g_bytes_get_size (bytes);
	result = g_memdup2 (g_bytes_get_data (bytes, NULL), *n_length);

	g_bytes_unref (bytes);
	return result;
}

/**
 * gcr_certificate_get_serial_number_hex:
 * @self: a #GcrCertificate
 *
 * Get the serial number of the certificate as a hex string.
 *
 * The caller should free the returned data using g_free() when
 * it is no longer required.
 *
 * Returns: (nullable): an allocated string containing the serial number as hex.
 */
gchar*
gcr_certificate_get_serial_number_hex (GcrCertificate *self)
{
	guchar *serial;
	gsize n_serial;
	gchar *hex;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	serial = gcr_certificate_get_serial_number (self, &n_serial);
	if (serial == NULL)
		return NULL;

	hex = egg_hex_encode (serial, n_serial);
	g_free (serial);
	return hex;
}

/**
 * gcr_certificate_get_basic_constraints:
 * @self: the certificate
 * @is_ca: (out) (optional): location to place a %TRUE if is an authority
 * @path_len: (out) (optional): location to place the max path length
 *
 * Get the basic constraints for the certificate if present. If %FALSE is
 * returned then no basic constraints are present and the @is_ca and
 * @path_len arguments are not changed.
 *
 * Returns: whether basic constraints are present or not
 */
gboolean
gcr_certificate_get_basic_constraints (GcrCertificate *self,
                                       gboolean *is_ca,
                                       gint *path_len)
{
	GcrCertificateInfo *info;
	GBytes *value;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), FALSE);

	info = certificate_info_load (self);
	if (info == NULL)
		return FALSE;

	value = _gcr_certificate_extension_find (info->asn1, GCR_OID_BASIC_CONSTRAINTS, NULL);
	if (!value)
		return FALSE;

	if (!_gcr_certificate_extension_basic_constraints (value, is_ca, path_len))
		g_return_val_if_reached (FALSE);

	g_bytes_unref (value);
	return TRUE;
}

static void
append_subject_public_key (GcrCertificate        *self,
                           GcrCertificateSection *section,
                           GNode                 *subject_public_key)
{
	guint key_nbits;
	const gchar *text;
	gchar *display;
	GBytes *value;
	guchar *raw;
	gsize n_raw;
	GQuark oid;
	guint bits;

	key_nbits = gcr_certificate_get_key_size (self);

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (subject_public_key,
	                                                  "algorithm", "algorithm", NULL));
	text = egg_oid_get_description (oid);
	_gcr_certificate_section_new_field (section, _("Key Algorithm"), text);

	value = egg_asn1x_get_element_raw (egg_asn1x_node (subject_public_key,
	                                                   "algorithm", "parameters", NULL));
	if (value) {
		_gcr_certificate_section_new_field_take_bytes (section,
		                                               _("Key Parameters"),
		                                               g_steal_pointer (&value));
	}

	if (key_nbits > 0) {
		display = g_strdup_printf ("%u", key_nbits);
		_gcr_certificate_section_new_field_take_value (section,
		                                               _("Key Size"),
		                                               g_steal_pointer (&display));
	}

	value = egg_asn1x_get_element_raw (subject_public_key);
	raw = gcr_fingerprint_from_subject_public_key_info (g_bytes_get_data (value, NULL),
	                                                    g_bytes_get_size (value),
	                                                    G_CHECKSUM_SHA1, &n_raw);
	g_clear_pointer (&value, g_bytes_unref);
	_gcr_certificate_section_new_field_take_bytes (section,
	                                               _("Key SHA1 Fingerprint"),
	                                               g_bytes_new_take (raw, n_raw));

	value = egg_asn1x_get_bits_as_raw (egg_asn1x_node (subject_public_key, "subjectPublicKey", NULL), &bits);
	_gcr_certificate_section_new_field_take_bytes (section, _("Public Key"), g_steal_pointer (&value));
}

static GcrCertificateSection *
append_extension_basic_constraints (GBytes *data)
{
	GcrCertificateSection *section;
	gboolean is_ca = FALSE;
	gint path_len = -1;
	gchar *number;

	if (!_gcr_certificate_extension_basic_constraints (data, &is_ca, &path_len))
		return NULL;

	section = _gcr_certificate_section_new (_("Basic Constraints"), FALSE);
	_gcr_certificate_section_new_field (section, _("Certificate Authority"), is_ca ? _("Yes") : _("No"));

	if (path_len < 0)
		number = g_strdup (_("Unlimited"));
	else
		number = g_strdup_printf ("%d", path_len);

	_gcr_certificate_section_new_field_take_value (section, _("Max Path Length"), g_steal_pointer (&number));

	return section;
}

static GcrCertificateSection *
append_extension_extended_key_usage (GBytes *data)
{
	GcrCertificateSection *section;
	GQuark *oids;
	GStrvBuilder *text;
	guint i;

	oids = _gcr_certificate_extension_extended_key_usage (data);
	if (!oids)
		return NULL;

	text = g_strv_builder_new ();
	for (i = 0; oids[i] != 0; i++) {
		g_strv_builder_add (text, egg_oid_get_description (oids[i]));
	}

	g_free (oids);

	section = _gcr_certificate_section_new (_("Extended Key Usage"), FALSE);
	_gcr_certificate_section_new_field_take_values (section, _("Allowed Purposes"), g_strv_builder_end (text));
	g_strv_builder_unref (text);

	return section;
}

static GcrCertificateSection *
append_extension_subject_key_identifier (GBytes *data)
{
	GcrCertificateSection *section;
	gpointer keyid;
	gsize n_keyid;

	keyid = _gcr_certificate_extension_subject_key_identifier (data, &n_keyid);
	if (!keyid)
		return NULL;

	section = _gcr_certificate_section_new (_("Subject Key Identifier"), FALSE);
	gchar *display = egg_hex_encode_full (keyid, n_keyid, TRUE, " ", 1);
	g_free (keyid);
	_gcr_certificate_section_new_field_take_value (section, _("Key Identifier"), g_steal_pointer (&display));

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

static GcrCertificateSection *
append_extension_key_usage (GBytes *data)
{
	GcrCertificateSection *section;
	gulong key_usage;
	GStrvBuilder *values;
	guint i;

	if (!_gcr_certificate_extension_key_usage (data, &key_usage))
		return NULL;

	values = g_strv_builder_new ();
	for (i = 0; i < G_N_ELEMENTS (usage_descriptions); i++) {
		if (key_usage & usage_descriptions[i].usage) {
			g_strv_builder_add (values, _(usage_descriptions[i].description));
		}
	}

	section = _gcr_certificate_section_new (_("Key Usage"), FALSE);
	_gcr_certificate_section_new_field_take_values (section, _("Usages"), g_strv_builder_end (values));
	g_strv_builder_unref (values);

	return section;
}

static GcrCertificateSection *
append_extension_subject_alt_name (GBytes *data)
{
	GcrCertificateSection *section;
	GArray *general_names;
	GcrGeneralName *general;
	guint i;

	general_names = _gcr_certificate_extension_subject_alt_name (data);
	if (general_names == NULL)
		return FALSE;

	section = _gcr_certificate_section_new (_("Subject Alternative Names"), FALSE);

	for (i = 0; i < general_names->len; i++) {
		general = &g_array_index (general_names, GcrGeneralName, i);
		if (general->display == NULL) {
			_gcr_certificate_section_new_field_take_bytes (section, general->description, g_bytes_ref (general->raw));
		} else
			_gcr_certificate_section_new_field (section, general->description, general->display);
	}

	_gcr_general_names_free (general_names);

	return section;
}

static GcrCertificateSection *
append_extension_hex (GQuark oid,
                      GBytes *value)
{
	GcrCertificateSection *section;
	const gchar *text;

	section = _gcr_certificate_section_new (_("Extension"), FALSE);

	/* Extension type */
	text = egg_oid_get_description (oid);
	_gcr_certificate_section_new_field (section, _("Identifier"), text);
	_gcr_certificate_section_new_field_take_bytes (section, _("Value"), g_steal_pointer (&value));

	return section;
}

static GcrCertificateSection *
append_extension (GcrCertificate *self,
                  GNode *node)
{
	GQuark oid;
	GBytes *value;
	gboolean critical;
	GcrCertificateSection *section = NULL;

	/* Dig out the OID */
	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (node, "extnID", NULL));
	g_return_val_if_fail (oid, NULL);

	/* Extension value */
	value = egg_asn1x_get_string_as_bytes (egg_asn1x_node (node, "extnValue", NULL));

	/* The custom parsers */
	if (oid == GCR_OID_BASIC_CONSTRAINTS)
		section = append_extension_basic_constraints (value);
	else if (oid == GCR_OID_EXTENDED_KEY_USAGE)
		section = append_extension_extended_key_usage (value);
	else if (oid == GCR_OID_SUBJECT_KEY_IDENTIFIER)
		section = append_extension_subject_key_identifier (value);
	else if (oid == GCR_OID_KEY_USAGE)
		section = append_extension_key_usage (value);
	else if (oid == GCR_OID_SUBJECT_ALT_NAME)
		section = append_extension_subject_alt_name (value);

	/* Otherwise the default raw display */
	if (!section) {
		section = append_extension_hex (oid, g_steal_pointer (&value));
	}

	/* Critical */
	if (section && egg_asn1x_get_boolean (egg_asn1x_node (node, "critical", NULL), &critical)) {
		_gcr_certificate_section_new_field (section, _("Critical"), critical ? _("Yes") : _("No"));
	}

	g_clear_pointer (&value, g_bytes_unref);
	return section;
}

static void
on_parsed_dn_part (guint index,
                   GQuark oid,
                   GNode *value,
                   gpointer user_data)
{
	GcrCertificateSection *section = user_data;
	const gchar *attr;
	const gchar *desc;
	gchar *label, *display;

	attr = egg_oid_get_name (oid);
	desc = egg_oid_get_description (oid);

	/* Combine them into something sane */
	if (attr && desc) {
		if (strcmp (attr, desc) == 0)
			label = g_strdup (attr);
		else
			label = g_strdup_printf ("%s (%s)", attr, desc);
	} else if (!attr && !desc) {
		label = g_strdup ("");
	} else if (attr) {
		label = g_strdup (attr);
	} else if (desc) {
		label = g_strdup (desc);
	} else {
		g_assert_not_reached ();
	}

	display = egg_dn_print_value (oid, value);
	if (!display)
		display = g_strdup ("");

	_gcr_certificate_section_new_field_take_value (section, label, g_steal_pointer (&display));
	g_clear_pointer (&label, g_free);
}

/**
 * gcr_certificate_get_interface_elements:
 * @self: the #GcrCertificate
 *
 * Get the list of sections from the certificate that can be shown to the user
 * interface.
 *
 * Returns: (element-type GcrCertificateSection) (transfer full): A #GList of
 * #GcrCertificateSection
 */
GList *
gcr_certificate_get_interface_elements (GcrCertificate *self)
{
	GcrCertificateSection *section;
	GcrCertificateInfo *info;
	GList *list = NULL;
	gchar *display;
	GBytes *bytes, *number;
	GNode *subject_public_key;
	GQuark oid;
	GDateTime *datetime;
	gulong version;
	guint bits;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);

	info = certificate_info_load (self);
	g_return_val_if_fail (info != NULL, NULL);

	display = gcr_certificate_get_subject_name (self);
	if (!display)
		display = g_strdup (_("Certificate"));

	section = _gcr_certificate_section_new (display, TRUE);
	g_clear_pointer (&display, g_free);

	display = gcr_certificate_get_subject_cn (self);
	if (display == NULL)
		display = g_strdup (_("Unknown"));
	_gcr_certificate_section_new_field_take_value (section, _("Identity"), g_steal_pointer (&display));

	display = gcr_certificate_get_issuer_cn (self);
	if (display == NULL)
		display = g_strdup (_("Unknown"));
	_gcr_certificate_section_new_field_take_value (section, _("Verified by"), g_steal_pointer (&display));

	datetime = gcr_certificate_get_expiry_date (self);
	if (datetime) {
		display = g_date_time_format (datetime, "%F");
		if (display)
			_gcr_certificate_section_new_field_take_value (section, _("Expires"), g_steal_pointer (&display));

		g_clear_pointer (&datetime, g_date_time_unref);
	}

	list = g_list_prepend (list, g_steal_pointer (&section));

	/* The subject */
	section = _gcr_certificate_section_new (_("Subject Name"), FALSE);
	egg_dn_parse (egg_asn1x_node (info->asn1, "tbsCertificate", "subject", "rdnSequence", NULL), on_parsed_dn_part, section);

	list = g_list_prepend (list, g_steal_pointer (&section));

	/* The Issuer */
	section = _gcr_certificate_section_new (_("Issuer Name"), FALSE);
	egg_dn_parse (egg_asn1x_node (info->asn1, "tbsCertificate", "issuer", "rdnSequence", NULL), on_parsed_dn_part, section);

	list = g_list_prepend (list, g_steal_pointer (&section));

	/* The Issued Parameters */
	section = _gcr_certificate_section_new (_("Issued Certificate"), FALSE);

	if (!egg_asn1x_get_integer_as_ulong (egg_asn1x_node (info->asn1, "tbsCertificate", "version", NULL), &version)) {
		g_critical ("Unable to parse certificate version");
	} else {
		display = g_strdup_printf ("%lu", version + 1);
		_gcr_certificate_section_new_field_take_value (section, _("Version"), g_steal_pointer (&display));
	}

	number = egg_asn1x_get_integer_as_raw (egg_asn1x_node (info->asn1, "tbsCertificate", "serialNumber", NULL));
	if (!number) {
		g_critical ("Unable to parse certificate serial number");
	} else {
		_gcr_certificate_section_new_field_take_bytes (section, _("Serial Number"), g_steal_pointer (&number));
	}

	datetime = gcr_certificate_get_issued_date (self);
	if (datetime) {
		display = g_date_time_format (datetime, "%F");
		if (display)
			_gcr_certificate_section_new_field_take_value (section, _("Not Valid Before"), g_steal_pointer (&display));

		g_clear_pointer (&datetime, g_date_time_unref);
	}

	datetime = gcr_certificate_get_expiry_date (self);
	if (datetime) {
		display = g_date_time_format (datetime, "%F");
		if (display)
			_gcr_certificate_section_new_field_take_value (section, _("Not Valid After"), g_steal_pointer (&display));

		g_clear_pointer (&datetime, g_date_time_unref);
	}

	list = g_list_prepend (list, g_steal_pointer (&section));

	/* Fingerprints */
	bytes = g_bytes_new_static (info->der, info->n_der);
	section = _gcr_certificate_section_new (_("Certificate Fingerprints"), FALSE);
	display = g_compute_checksum_for_bytes (G_CHECKSUM_SHA1, bytes);
	_gcr_certificate_section_new_field_take_value (section, "SHA1", g_steal_pointer (&display));
	display = g_compute_checksum_for_bytes (G_CHECKSUM_MD5, bytes);
	_gcr_certificate_section_new_field_take_value (section, "MD5", g_steal_pointer (&display));
	g_clear_pointer (&bytes, g_bytes_unref);

	list = g_list_prepend (list, g_steal_pointer (&section));

	/* Public Key Info */
	section = _gcr_certificate_section_new (_("Public Key Info"), FALSE);
	subject_public_key = egg_asn1x_node (info->asn1, "tbsCertificate", "subjectPublicKeyInfo", NULL);
	append_subject_public_key (self, section, subject_public_key);

	list = g_list_prepend (list, g_steal_pointer (&section));

	/* Extensions */
	for (guint extension_num = 1; TRUE; ++extension_num) {
		GNode *extension = egg_asn1x_node (info->asn1, "tbsCertificate", "extensions", extension_num, NULL);
		if (extension == NULL)
			break;
		section = append_extension (self, extension);
		if (section)
			list = g_list_prepend (list, g_steal_pointer (&section));
	}

	/* Signature */
	section = _gcr_certificate_section_new (_("Signature"), FALSE);

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (info->asn1, "signatureAlgorithm", "algorithm", NULL));
	_gcr_certificate_section_new_field (section, _("Signature Algorithm"), egg_oid_get_description (oid));

	bytes = egg_asn1x_get_element_raw (egg_asn1x_node (info->asn1, "signatureAlgorithm", "parameters", NULL));
	if (bytes) {
		_gcr_certificate_section_new_field_take_bytes (section, _("Signature Parameters"), g_steal_pointer (&bytes));
	}

	bytes = egg_asn1x_get_bits_as_raw (egg_asn1x_node (info->asn1, "signature", NULL), &bits);
	_gcr_certificate_section_new_field_take_bytes (section, _("Signature"), g_steal_pointer (&bytes));

	list = g_list_prepend (list, g_steal_pointer (&section));

	return g_list_reverse (list);
}

/* -----------------------------------------------------------------------------
 * MIXIN
 */

/**
 * gcr_certificate_mixin_emit_notify:
 * @self: the #GcrCertificate
 *
 * Implementers of the #GcrCertificate mixin should call this function to notify
 * when the certificate has changed to emit notifications on the various
 * properties.
 */
void
gcr_certificate_mixin_emit_notify (GcrCertificate *self)
{
	GObject *obj;

	g_return_if_fail (GCR_IS_CERTIFICATE (self));

	obj = G_OBJECT (self);
	g_object_notify (obj, "label");
	g_object_notify (obj, "subject-name");
	g_object_notify (obj, "issuer-name");
	g_object_notify (obj, "expiry-date");
}

/**
 * gcr_certificate_mixin_class_init: (skip)
 * @object_class: The GObjectClass for this class
 *
 * Initialize the certificate mixin for the class. This mixin implements the
 * various required properties for the certificate.
 *
 * Call this function near the end of your derived class_init function. The
 * derived class must implement the #GcrCertificate interface.
 */
void
gcr_certificate_mixin_class_init (GObjectClass *object_class)
{
	if (!g_object_class_find_property (object_class, "description"))
		g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	if (!g_object_class_find_property (object_class, "label"))
		g_object_class_override_property (object_class, PROP_LABEL, "label");
	if (!g_object_class_find_property (object_class, "subject-name"))
		g_object_class_override_property (object_class, PROP_SUBJECT_NAME, "subject-name");
	if (!g_object_class_find_property (object_class, "issuer-name"))
		g_object_class_override_property (object_class, PROP_ISSUER_NAME, "issuer-name");
	if (!g_object_class_find_property (object_class, "expiry-date"))
		g_object_class_override_property (object_class, PROP_EXPIRY_DATE, "expiry-date");

	_gcr_initialize_library ();
}

/**
 * gcr_certificate_mixin_get_property: (skip)
 * @obj: The object
 * @prop_id: The property id
 * @value: The value to fill in.
 * @pspec: The param specification.
 *
 * Implementation to get various required certificate properties. This should
 * be called from your derived class get_property function, or used as a
 * get_property virtual function.
 *
 * Example of use as called from derived class get_property function:
 *
 * <informalexample><programlisting>
 * static void
 * my_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
 * {
 * 	switch (prop_id) {
 *
 * 	...
 *
 * 	default:
 * 		gcr_certificate_mixin_get_property (obj, prop_id, value, pspec);
 * 		break;
 * 	}
 *}
 * </programlisting></informalexample>
 *
 * Example of use as get_property function:
 *
 * <informalexample><programlisting>
 * static void
 * my_class_init (MyClass *klass)
 * {
 * 	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
 * 	gobject_class->get_property = gcr_certificate_mixin_get_property;
 *
 * 	...
 * }
 * </programlisting></informalexample>

 */
void
gcr_certificate_mixin_get_property (GObject *obj, guint prop_id,
                                    GValue *value, GParamSpec *pspec)
{
	GcrCertificate *cert = GCR_CERTIFICATE (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_take_string (value, gcr_certificate_get_subject_name (cert));
		break;
	case PROP_SUBJECT_NAME:
		g_value_take_string (value, gcr_certificate_get_subject_name (cert));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, _("Certificate"));
		break;
	case PROP_ISSUER_NAME:
		g_value_take_string (value, gcr_certificate_get_issuer_name (cert));
		break;
	case PROP_EXPIRY_DATE:
		g_value_take_boxed (value, gcr_certificate_get_expiry_date (cert));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}
