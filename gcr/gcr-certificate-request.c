/*
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

#include "gcr-certificate-request.h"
#include "gcr-key-mechanisms.h"
#include "gcr-subject-public-key.h"

#include "gcr/gcr-enum-types.h"
#include "gcr/gcr-oids.h"

#include <egg/egg-armor.h>
#include <egg/egg-asn1x.h>
#include <egg/egg-asn1-defs.h>
#include <egg/egg-dn.h>

#include <glib/gi18n-lib.h>

/**
 * GcrCertificateRequest:
 *
 * An object that allows creation of certificate requests. A certificate
 * request is sent to a certificate authority to request an X.509 certificate.
 *
 * Use [func@CertificateRequest.prepare] to create a blank certificate
 * request for a given private key. Set the common name on the certificate
 * request with [method@CertificateRequest.set_cn], and then sign the request
 * with [method@CertificateRequest.complete_async].
 */

/**
 * GcrCertificateRequestFormat:
 * @GCR_CERTIFICATE_REQUEST_PKCS10: certificate request is in PKCS#10 format
 *
 * The format of a certificate request. Currently only PKCS#10 is supported.
 */

struct _GcrCertificateRequest {
	GObject parent;

	GckObject *private_key;
	GNode *asn;
	gulong *mechanisms;
	gulong n_mechanisms;
};

enum {
	PROP_0,
	PROP_FORMAT,
	PROP_PRIVATE_KEY
};

/* Forward declarations */
G_DEFINE_TYPE (GcrCertificateRequest, gcr_certificate_request, G_TYPE_OBJECT);

/* When updating here, update prepare_to_be_signed() */
static const gulong RSA_MECHANISMS[] = {
	CKM_SHA1_RSA_PKCS,
	CKM_RSA_PKCS,
};

/* When updating here, update prepare_to_be_signed() */
static const gulong DSA_MECHANISMS[] = {
	CKM_DSA_SHA1,
	CKM_DSA,
};

static const gulong ALL_MECHANISMS[] = {
	CKM_SHA1_RSA_PKCS,
	CKM_DSA_SHA1,
	CKM_RSA_PKCS,
	CKM_DSA,
};

G_STATIC_ASSERT (sizeof (ALL_MECHANISMS) == sizeof (RSA_MECHANISMS) + sizeof (DSA_MECHANISMS));

static void
gcr_certificate_request_init (GcrCertificateRequest *self)
{

}

static void
gcr_certificate_request_constructed (GObject *obj)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);
	GNode *version;

	G_OBJECT_CLASS (gcr_certificate_request_parent_class)->constructed (obj);

	self->asn = egg_asn1x_create (pkix_asn1_tab, "pkcs-10-CertificationRequest");
	g_return_if_fail (self->asn != NULL);

	/* Setup the version */
	version = egg_asn1x_node (self->asn, "certificationRequestInfo", "version", NULL);
	egg_asn1x_set_integer_as_ulong (version, 0);
}

static void
gcr_certificate_request_finalize (GObject *obj)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);

	egg_asn1x_destroy (self->asn);
	g_free (self->mechanisms);

	G_OBJECT_CLASS (gcr_certificate_request_parent_class)->finalize (obj);
}

static void
gcr_certificate_request_set_property (GObject *obj,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);
	GcrCertificateRequestFormat format;

	switch (prop_id) {
	case PROP_PRIVATE_KEY:
		g_return_if_fail (self->private_key == NULL);
		self->private_key = g_value_dup_object (value);
		g_return_if_fail (GCK_IS_OBJECT (self->private_key));
		break;
	case PROP_FORMAT:
		format = g_value_get_enum (value);
		g_return_if_fail (format == GCR_CERTIFICATE_REQUEST_PKCS10);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_certificate_request_get_property (GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);

	switch (prop_id) {
	case PROP_PRIVATE_KEY:
		g_value_set_object (value, gcr_certificate_request_get_private_key (self));
		break;
	case PROP_FORMAT:
		g_value_set_enum (value, gcr_certificate_request_get_format (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_certificate_request_class_init (GcrCertificateRequestClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = gcr_certificate_request_constructed;
	gobject_class->finalize = gcr_certificate_request_finalize;
	gobject_class->set_property = gcr_certificate_request_set_property;
	gobject_class->get_property = gcr_certificate_request_get_property;

	/**
	 * GcrCertificateRequest:private-key:
	 *
	 * The private key that this certificate request is for.
	 */
	g_object_class_install_property (gobject_class, PROP_PRIVATE_KEY,
		g_param_spec_object ("private-key", "Private key", "Private key for request",
		                     GCK_TYPE_OBJECT,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GcrCertificateRequest:format:
	 *
	 * The format of the certificate request.
	 */
	g_object_class_install_property (gobject_class, PROP_FORMAT,
		g_param_spec_enum ("format", "Format", "Format of certificate request",
		                   GCR_TYPE_CERTIFICATE_REQUEST_FORMAT, GCR_CERTIFICATE_REQUEST_PKCS10,
		                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

/**
 * gcr_certificate_request_prepare:
 * @format: the format for the certificate request
 * @private_key: the private key the the certificate is being requested for
 *
 * Create a new certificate request, in the given format for the private key.
 *
 * Returns: (transfer full): a new #GcrCertificate request
 */
GcrCertificateRequest *
gcr_certificate_request_prepare (GcrCertificateRequestFormat format,
                                 GckObject *private_key)
{
	g_return_val_if_fail (format == GCR_CERTIFICATE_REQUEST_PKCS10, NULL);
	g_return_val_if_fail (GCK_IS_OBJECT (private_key), NULL);

	return g_object_new (GCR_TYPE_CERTIFICATE_REQUEST,
	                     "format", format,
	                     "private-key", private_key,
	                     NULL);
}

/**
 * gcr_certificate_request_get_private_key:
 * @self: the certificate request
 *
 * Get the private key this certificate request is for.
 *
 * Returns: (transfer none): the private key,
 */
GckObject *
gcr_certificate_request_get_private_key (GcrCertificateRequest *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), NULL);
	return self->private_key;
}

/**
 * gcr_certificate_request_get_format:
 * @self: the certificate request
 *
 * Get the format of this certificate request.
 *
 * Returns: the format
 */
GcrCertificateRequestFormat
gcr_certificate_request_get_format (GcrCertificateRequest *self)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), 0);
	return GCR_CERTIFICATE_REQUEST_PKCS10;
}

/**
 * gcr_certificate_request_set_cn:
 * @self: the certificate request
 * @cn: common name to set on the request
 *
 * Set the common name encoded in the certificate request.
 */
void
gcr_certificate_request_set_cn (GcrCertificateRequest *self,
                                const gchar *cn)
{
	GNode *subject;
	GNode *dn;

	g_return_if_fail (GCR_IS_CERTIFICATE_REQUEST (self));
	g_return_if_fail (cn != NULL);

	subject = egg_asn1x_node (self->asn, "certificationRequestInfo", "subject", NULL);
	dn = egg_asn1x_node (subject, "rdnSequence", NULL);

	/* TODO: we shouldn't really be clearing this, but replacing CN */
	egg_asn1x_set_choice (subject, dn);
	egg_asn1x_clear (dn);
	egg_dn_add_string_part (dn, GCR_OID_NAME_CN, cn);
}

static GBytes *
hash_sha1_pkcs1 (GBytes *data)
{
	const guchar SHA1_ASN[15] = /* Object ID is 1.3.14.3.2.26 */
		{ 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03,
		  0x02, 0x1a, 0x05, 0x00, 0x04, 0x14 };

	GChecksum *checksum;
	guchar *hash;
	gsize n_hash;
	gsize n_digest;

	n_digest = g_checksum_type_get_length (G_CHECKSUM_SHA1);
	n_hash = n_digest + sizeof (SHA1_ASN);
	hash = g_malloc (n_hash);
	memcpy (hash, SHA1_ASN, sizeof (SHA1_ASN));

	checksum = g_checksum_new (G_CHECKSUM_SHA1);
	g_checksum_update (checksum, g_bytes_get_data (data, NULL), g_bytes_get_size (data));
	g_checksum_get_digest (checksum, hash + sizeof (SHA1_ASN), &n_digest);
	g_checksum_free (checksum);

	return g_bytes_new_take (hash, n_hash);
}

static GBytes *
hash_sha1 (GBytes *data)
{
	GChecksum *checksum;
	guchar *hash;
	gsize n_hash;

	n_hash = g_checksum_type_get_length (G_CHECKSUM_SHA1);
	hash = g_malloc (n_hash);

	checksum = g_checksum_new (G_CHECKSUM_SHA1);
	g_checksum_update (checksum, g_bytes_get_data (data, NULL), g_bytes_get_size (data));
	g_checksum_get_digest (checksum, hash, &n_hash);
	g_checksum_free (checksum);

	return g_bytes_new_take (hash, n_hash);
}

static GBytes *
prepare_to_be_signed (GcrCertificateRequest *self,
                      GckMechanism *mechanism)
{
	GNode *node;
	GBytes *data;
	GBytes *hash;

	g_assert (mechanism != NULL);

	node = egg_asn1x_node (self->asn, "certificationRequestInfo", NULL);
	data = egg_asn1x_encode (node, NULL);

	mechanism->parameter = NULL;
	mechanism->n_parameter = 0;

	switch (mechanism->type) {
	case CKM_SHA1_RSA_PKCS:
	case CKM_DSA_SHA1:
		return data;

	case CKM_RSA_PKCS:
		hash = hash_sha1_pkcs1 (data);
		g_bytes_unref (data);
		return hash;

	case CKM_DSA:
		hash = hash_sha1 (data);
		g_bytes_unref (data);
		return hash;

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static gboolean
prepare_subject_public_key_and_mechanisms (GcrCertificateRequest *self,
                                           GNode *subject_public_key,
                                           GQuark *algorithm,
                                           const gulong **mechanisms,
                                           gsize *n_mechanisms,
                                           GError **error)
{
	GBytes *encoded;
	GNode *node;
	GQuark oid;

	g_assert (algorithm != NULL);
	g_assert (mechanisms != NULL);
	g_assert (n_mechanisms != NULL);

	encoded = egg_asn1x_encode (subject_public_key, NULL);
	g_return_val_if_fail (encoded != NULL, FALSE);

	node = egg_asn1x_node (subject_public_key, "algorithm", "algorithm", NULL);
	oid = egg_asn1x_get_oid_as_quark (node);

	if (oid == GCR_OID_PKIX1_RSA) {
		*mechanisms = RSA_MECHANISMS;
		*n_mechanisms = G_N_ELEMENTS (RSA_MECHANISMS);
		*algorithm = GCR_OID_PKIX1_SHA1_WITH_RSA;

	} else if (oid == GCR_OID_PKIX1_DSA) {
		*mechanisms = DSA_MECHANISMS;
		*n_mechanisms = G_N_ELEMENTS (DSA_MECHANISMS);
		*algorithm = GCR_OID_PKIX1_SHA1_WITH_DSA;

	} else {
		g_bytes_unref (encoded);
		g_set_error (error, GCR_DATA_ERROR, GCR_ERROR_UNRECOGNIZED,
		             _("Unsupported key type for certificate request"));
		return FALSE;
	}

	node = egg_asn1x_node (self->asn, "certificationRequestInfo", "subjectPKInfo", NULL);
	if (!egg_asn1x_decode (node, encoded))
		g_return_val_if_reached (FALSE);

	g_bytes_unref (encoded);
	return TRUE;
}

static void
encode_take_signature_into_request (GcrCertificateRequest *self,
                                    GQuark algorithm,
                                    GNode *subject_public_key,
                                    guchar *result,
                                    gsize n_result)
{
	GNode *params;
	GNode *node;

	node = egg_asn1x_node (self->asn, "signature", NULL);
	egg_asn1x_take_bits_as_raw (node, g_bytes_new_take (result, n_result), n_result * 8);

	node = egg_asn1x_node (self->asn, "signatureAlgorithm", "algorithm", NULL);
	egg_asn1x_set_oid_as_quark (node, algorithm);

	node = egg_asn1x_node (self->asn, "signatureAlgorithm", "parameters", NULL);
	params = egg_asn1x_node (subject_public_key, "algorithm", "parameters", NULL);
	egg_asn1x_set_any_from (node, params);
}

/**
 * gcr_certificate_request_complete:
 * @self: a certificate request
 * @cancellable: a cancellation object
 * @error: location to place an error on failure
 *
 * Complete and sign a certificate request, so that it can be encoded
 * and sent to a certificate authority.
 *
 * This call may block as it signs the request using the private key.
 *
 * Returns: whether certificate request was successfully completed or not
 */
gboolean
gcr_certificate_request_complete (GcrCertificateRequest *self,
                                  GCancellable *cancellable,
                                  GError **error)
{
	GNode *subject_public_key;
	const gulong *mechanisms;
	gsize n_mechanisms;
	GckMechanism mechanism = { 0, };
	GQuark algorithm = 0;
	GBytes *tbs;
	GckSession *session;
	guchar *signature;
	gsize n_signature;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	subject_public_key = _gcr_subject_public_key_load (self->private_key,
	                                                   cancellable, error);
	if (subject_public_key == NULL)
		return FALSE;

	ret = prepare_subject_public_key_and_mechanisms (self, subject_public_key,
	                                                 &algorithm, &mechanisms,
	                                                 &n_mechanisms, error);

	if (!ret) {
		egg_asn1x_destroy (subject_public_key);
		return FALSE;
	}

	/* Figure out which mechanism to use */
	mechanism.type = _gcr_key_mechanisms_check (self->private_key, mechanisms,
	                                            n_mechanisms, CKA_SIGN,
	                                            cancellable, NULL);
	if (mechanism.type == GCK_INVALID) {
		egg_asn1x_destroy (subject_public_key);
		g_set_error (error, GCK_ERROR, CKR_KEY_TYPE_INCONSISTENT,
		             _("The key cannot be used to sign the request"));
		return FALSE;
	}

	tbs = prepare_to_be_signed (self, &mechanism);
	session = gck_object_get_session (self->private_key);
	signature = gck_session_sign_full (session, self->private_key, &mechanism,
	                                   g_bytes_get_data (tbs, NULL),
	                                   g_bytes_get_size (tbs),
	                                   &n_signature, cancellable, error);
	g_object_unref (session);
	g_bytes_unref (tbs);

	if (!signature) {
		egg_asn1x_destroy (subject_public_key);
		return FALSE;
	}

	encode_take_signature_into_request (self, algorithm, subject_public_key,
	                                    signature, n_signature);
	egg_asn1x_destroy (subject_public_key);
	return TRUE;
}

typedef struct {
	GcrCertificateRequest *request;
	GQuark algorithm;
	GNode *subject_public_key;
	GckMechanism mechanism;
	GckSession *session;
	GBytes *tbs;
} CompleteClosure;

static void
complete_closure_free (gpointer data)
{
	CompleteClosure *closure = data;
	egg_asn1x_destroy (closure->subject_public_key);
	g_clear_object (&closure->request);
	g_clear_object (&closure->session);
	if (closure->tbs)
		g_bytes_unref (closure->tbs);
	g_free (closure);
}

static void
on_certificate_request_signed (GObject *source,
                               GAsyncResult *result,
                               gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CompleteClosure *closure = g_task_get_task_data (task);
	GError *error = NULL;
	guchar *signature;
	gsize n_signature;

	signature = gck_session_sign_finish (closure->session, result, &n_signature, &error);
	if (error == NULL) {
		encode_take_signature_into_request (closure->request,
		                                    closure->algorithm,
		                                    closure->subject_public_key,
		                                    signature, n_signature);

		g_task_return_boolean (task, TRUE);
	} else {
		g_task_return_error (task, g_steal_pointer (&error));
	}

	g_clear_object (&task);
}

static void
on_mechanism_check (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CompleteClosure *closure = g_task_get_task_data (task);
	GCancellable *cancellable = g_task_get_cancellable (task);

	closure->mechanism.type =  _gcr_key_mechanisms_check_finish (closure->request->private_key,
	                                                             result, NULL);
	if (closure->mechanism.type == GCK_INVALID) {
		g_task_return_new_error (task, GCK_ERROR, CKR_KEY_TYPE_INCONSISTENT,
		                         _("The key cannot be used to sign the request"));

	} else {
		closure->tbs = prepare_to_be_signed (closure->request, &closure->mechanism);
		gck_session_sign_async (closure->session,
		                        closure->request->private_key,
		                        &closure->mechanism,
		                        g_bytes_get_data (closure->tbs, NULL),
		                        g_bytes_get_size (closure->tbs),
		                        cancellable,
		                        on_certificate_request_signed,
		                        g_steal_pointer (&task));
	}

	g_clear_object (&task);
}

static void
on_subject_public_key_loaded (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CompleteClosure *closure = g_task_get_task_data (task);
	GCancellable *cancellable = g_task_get_cancellable (task);
	const gulong *mechanisms;
	gsize n_mechanisms;
	GError *error = NULL;

	closure->subject_public_key = _gcr_subject_public_key_load_finish (result, &error);
	if (error == NULL) {
		prepare_subject_public_key_and_mechanisms (closure->request,
		                                           closure->subject_public_key,
		                                           &closure->algorithm,
		                                           &mechanisms,
		                                           &n_mechanisms,
		                                           &error);
	}

	if (error != NULL) {
		g_task_return_error (task, g_steal_pointer (&error));
		g_clear_object (&task);
		return;
	}

	_gcr_key_mechanisms_check_async (closure->request->private_key,
	                                 mechanisms, n_mechanisms, CKA_SIGN,
	                                 cancellable, on_mechanism_check,
	                                 g_steal_pointer (&task));
}

/**
 * gcr_certificate_request_complete_async:
 * @self: a certificate request
 * @cancellable: (nullable): a cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Asynchronously complete and sign a certificate request, so that it can
 * be encoded and sent to a certificate authority.
 *
 * This call will return immediately and complete later.
 */
void
gcr_certificate_request_complete_async (GcrCertificateRequest *self,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
	GTask *task;
	CompleteClosure *closure;

	g_return_if_fail (GCR_IS_CERTIFICATE_REQUEST (self));
	g_return_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable));

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_certificate_request_complete_async);
	closure = g_new0 (CompleteClosure, 1);
	closure->session = gck_object_get_session (self->private_key);
	closure->request = g_object_ref (self);
	g_task_set_task_data (task, closure, complete_closure_free);

	_gcr_subject_public_key_load_async (self->private_key, cancellable,
	                                    on_subject_public_key_loaded,
	                                    g_steal_pointer (&task));

	g_clear_object (&task);
}

/**
 * gcr_certificate_request_complete_finish:
 * @self: a certificate request
 * @result: result of the asynchronous operation
 * @error: location to place an error on failure
 *
 * Finish an asynchronous operation to complete and sign a certificate
 * request.
 *
 * Returns: whether certificate request was successfully completed or not
 */
gboolean
gcr_certificate_request_complete_finish (GcrCertificateRequest *self,
                                         GAsyncResult *result,
                                         GError **error)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * gcr_certificate_request_encode:
 * @self: a certificate request
 * @textual: whether to encode output as text
 * @length: (out): location to place length of returned data
 *
 * Encode the certificate request. It must have been completed with
 * [method@CertificateRequest.complete] or
 * [method@CertificateRequest.complete_async].
 *
 * If @textual is %FALSE, the output is a DER encoded certificate request.
 *
 * If @textual is %TRUE, the output is encoded as text. For PKCS#10 requests
 * this is done using the OpenSSL style PEM encoding.
 *
 * Returns: (transfer full) (array length=length): the encoded certificate request
 */
guchar *
gcr_certificate_request_encode (GcrCertificateRequest *self,
                                gboolean textual,
                                gsize *length)
{
	GBytes *bytes;
	gpointer encoded;
	gpointer data;
	gsize size;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), NULL);
	g_return_val_if_fail (length != NULL, NULL);

	bytes = egg_asn1x_encode (self->asn, NULL);
	if (bytes == NULL) {
		g_warning ("couldn't encode certificate request: %s",
		           egg_asn1x_message (self->asn));
		return NULL;
	}

	encoded = g_bytes_unref_to_data (bytes, &size);
	if (textual) {
		data = egg_armor_write (encoded, size,
		                        g_quark_from_static_string ("CERTIFICATE REQUEST"),
		                        NULL, length);
		g_free (encoded);
		encoded = data;

	} else {
		*length = size;
	}

	return encoded;
}

/**
 * gcr_certificate_request_capable:
 * @private_key: a private key
 * @cancellable: (nullable): cancellation object
 * @error: location to place an error
 *
 * Check whether [class@CertificateRequest] is capable of creating a request
 * for the given @private_key.
 *
 * Returns: whether a request can be created
 */
gboolean
gcr_certificate_request_capable (GckObject *private_key,
                                 GCancellable *cancellable,
                                 GError **error)
{
	g_return_val_if_fail (GCK_IS_OBJECT (private_key), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return _gcr_key_mechanisms_check (private_key, ALL_MECHANISMS,
	                                  G_N_ELEMENTS (ALL_MECHANISMS),
	                                 CKA_SIGN, cancellable, error);
}

/**
 * gcr_certificate_request_capable_async:
 * @private_key: a private key
 * @cancellable: (nullable): cancellation object
 * @callback: will be called when the operation completes
 * @user_data: data to be passed to callback
 *
 * Asynchronously check whether [class@CertificateRequest] is capable of
 * creating a request for the given @private_key.
 */
void
gcr_certificate_request_capable_async (GckObject *private_key,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
	g_return_if_fail (GCK_IS_OBJECT (private_key));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	_gcr_key_mechanisms_check_async (private_key, ALL_MECHANISMS,
	                                 G_N_ELEMENTS (ALL_MECHANISMS),
	                                 CKA_SIGN, cancellable,
	                                 callback, user_data);
}

/**
 * gcr_certificate_request_capable_finish:
 * @result: asynchronous result
 * @error: location to place an error
 *
 * Get the result for asynchronously check whether [class@CertificateRequest] is
 * capable of creating a request for the given @private_key.
 *
 * Returns: whether a request can be created
 */
gboolean
gcr_certificate_request_capable_finish (GAsyncResult *result,
                                        GError **error)
{
	GObject *source;
	gulong mech;

	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	source = g_async_result_get_source_object (result);
	mech = _gcr_key_mechanisms_check_finish (GCK_OBJECT (source), result, error);
	g_object_unref (source);

	return mech != GCK_INVALID;
}
