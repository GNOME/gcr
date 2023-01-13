/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Collabora Ltd
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

#include "gcr-pkcs11-certificate.h"

#include "gcr-certificate.h"
#include "gcr-internal.h"
#include "gcr-library.h"

#include <gck/gck.h>

#include <string.h>

/**
 * GcrPkcs11Certificate:
 *
 * A certificate loaded from a PKCS#11 storage.
 * It is also a valid [class@Gck.Object] and can be used as such.
 *
 * Use gcr_pkcs11_certificate_lookup_issuer() to lookup the issuer of a given
 * certificate in the PKCS#11 store.
 *
 * Various common PKCS#11 certificate attributes are automatically loaded and
 * are available via gcr_pkcs11_certificate_get_attributes().
 */

enum {
	PROP_0,
	PROP_ATTRIBUTES
};

struct _GcrPkcs11CertificatePrivate {
	GckAttributes *attrs;
};

static void gcr_certificate_iface (GcrCertificateIface *iface);
G_DEFINE_TYPE_WITH_CODE (GcrPkcs11Certificate, gcr_pkcs11_certificate, GCK_TYPE_OBJECT,
	G_ADD_PRIVATE (GcrPkcs11Certificate);
	G_IMPLEMENT_INTERFACE (GCR_TYPE_CERTIFICATE, gcr_certificate_iface);
);

static GckAttributes *
prepare_lookup_certificate_issuer (GcrCertificate *cert)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	gpointer data;
	gsize n_data;

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_CERTIFICATE);
	gck_builder_add_ulong (&builder, CKA_CERTIFICATE_TYPE, CKC_X_509);

	data = gcr_certificate_get_issuer_raw (cert, &n_data);
	gck_builder_add_data (&builder, CKA_SUBJECT, data, n_data);
	g_free (data);

	return gck_builder_end (&builder);
}

static GcrCertificate*
perform_lookup_certificate (GckAttributes *search,
                            GCancellable *cancellable,
                            GError **error)
{
	GcrCertificate *cert;
	GckObject *object;
	GckAttributes *attrs;
	GckModule *module;
	GckSession *session;
	GckEnumerator *en;
	GList *modules;

	if (!gcr_pkcs11_initialize (cancellable, error))
		return NULL;

	modules = gcr_pkcs11_get_modules ();
	en = gck_modules_enumerate_objects (modules, search, 0);
	g_clear_list (&modules, g_object_unref);

	object = gck_enumerator_next (en, cancellable, error);
	g_object_unref (en);

	if (object == NULL)
		return NULL;

	/*
	 * Only the CKA_VALUE, CKA_CLASS and CKA_CERTIFICATE_TYPE
	 * is strictly necessary here, but we get more attrs.
	 */
	attrs = gck_object_get (object, cancellable, error,
	                        CKA_VALUE, CKA_LABEL,
	                        CKA_ID, CKA_CLASS,
	                        CKA_CERTIFICATE_TYPE,
	                        CKA_ISSUER,
	                        CKA_SERIAL_NUMBER,
	                        GCK_INVALID);

	if (attrs == NULL) {
		g_object_unref (object);
		return NULL;
	}

	module = gck_object_get_module (object);
	session = gck_object_get_session (object);

	cert = g_object_new (GCR_TYPE_PKCS11_CERTIFICATE,
	                     "module", module,
	                     "handle", gck_object_get_handle (object),
	                     "session", session,
	                     "attributes", attrs,
	                     NULL);

	g_object_unref (module);
	g_object_unref (session);
	g_object_unref (object);

	gck_attributes_unref (attrs);

	return cert;
}

static void
thread_lookup_certificate (GTask *task, gpointer src_object, gpointer task_data,
                           GCancellable *cancellable)
{
	GckAttributes *search = (GckAttributes *) task_data;
	GcrCertificate *result;
	GError *error = NULL;

	result = perform_lookup_certificate (search, cancellable, &error);
	if (error != NULL) {
		g_task_return_error (task, g_steal_pointer (&error));
		g_clear_error (&error);
	} else{
		g_task_return_pointer (task, result, g_object_unref);
	}
}

/* ----------------------------------------------------------------------------
 * OBJECT
 */

static GObject*
gcr_pkcs11_certificate_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	gpointer obj = G_OBJECT_CLASS (gcr_pkcs11_certificate_parent_class)->constructor (type, n_props, props);
	GckAttributes *attrs;
	const GckAttribute *attr;
	gulong value;

	attrs = gcr_pkcs11_certificate_get_attributes (obj);
	g_return_val_if_fail (attrs, NULL);

	if (!gck_attributes_find_ulong (attrs, CKA_CLASS, &value) ||
	    value != CKO_CERTIFICATE) {
		g_warning ("attributes don't contain a certificate with: %s",
		           "CKA_CLASS == CKO_CERTIFICATE");
		return NULL;
	}

	if (!gck_attributes_find_ulong (attrs, CKA_CERTIFICATE_TYPE, &value) ||
	    value != CKC_X_509) {
		g_warning ("attributes don't contain a certificate with: %s",
		           "CKA_CERTIFICATE_TYPE == CKC_X_509");
		return NULL;
	}

	attr = gck_attributes_find (attrs, CKA_VALUE);
	if (!attr || !attr->value || attr->length == 0 || attr->length == G_MAXULONG) {
		g_warning ("attributes don't contain a valid: CKA_VALUE");
		return NULL;
	}

	return obj;
}

static void
gcr_pkcs11_certificate_init (GcrPkcs11Certificate *self)
{
	self->pv = gcr_pkcs11_certificate_get_instance_private (self);
}

static void
gcr_pkcs11_certificate_set_property (GObject *obj, guint prop_id, const GValue *value,
                                     GParamSpec *pspec)
{
	GcrPkcs11Certificate *self = GCR_PKCS11_CERTIFICATE (obj);

	switch (prop_id) {
	case PROP_ATTRIBUTES:
		g_return_if_fail (self->pv->attrs == NULL);
		self->pv->attrs = g_value_dup_boxed (value);
		g_return_if_fail (self->pv->attrs != NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_pkcs11_certificate_get_property (GObject *obj, guint prop_id, GValue *value,
                                     GParamSpec *pspec)
{
	GcrPkcs11Certificate *self = GCR_PKCS11_CERTIFICATE (obj);

	switch (prop_id) {
	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, gcr_pkcs11_certificate_get_attributes (self));
		break;
	default:
		gcr_certificate_mixin_get_property (obj, prop_id, value, pspec);
		break;
	}
}

static void
gcr_pkcs11_certificate_finalize (GObject *obj)
{
	GcrPkcs11Certificate *self = GCR_PKCS11_CERTIFICATE (obj);

	gck_attributes_unref (self->pv->attrs);

	G_OBJECT_CLASS (gcr_pkcs11_certificate_parent_class)->finalize (obj);
}

static void
gcr_pkcs11_certificate_class_init (GcrPkcs11CertificateClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructor = gcr_pkcs11_certificate_constructor;
	gobject_class->get_property = gcr_pkcs11_certificate_get_property;
	gobject_class->set_property = gcr_pkcs11_certificate_set_property;
	gobject_class->finalize = gcr_pkcs11_certificate_finalize;

	/**
	 * GcrPkcs11Certificate:attributes:
	 *
	 * Automatically loaded attributes for this certificate.
	 */
	g_object_class_install_property (gobject_class, PROP_ATTRIBUTES,
	         g_param_spec_boxed ("attributes", "Attributes", "The data displayed in the renderer",
	                             GCK_TYPE_ATTRIBUTES,
	                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	gcr_certificate_mixin_class_init (gobject_class);
	_gcr_initialize_library ();
}

static const guchar *
gcr_pkcs11_certificate_get_der_data (GcrCertificate *cert,
                                     gsize *n_data)
{
	GcrPkcs11Certificate *self = GCR_PKCS11_CERTIFICATE (cert);
	const GckAttribute *attr;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (n_data, NULL);
	g_return_val_if_fail (self->pv->attrs, NULL);

	attr = gck_attributes_find (self->pv->attrs, CKA_VALUE);
	g_return_val_if_fail (attr && attr->length != 0 && attr->length != G_MAXULONG, NULL);
	*n_data = attr->length;
	return attr->value;
}

static void
gcr_certificate_iface (GcrCertificateIface *iface)
{
	iface->get_der_data = gcr_pkcs11_certificate_get_der_data;
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gcr_pkcs11_certificate_get_attributes:
 * @self: A #GcrPkcs11Certificate
 *
 * Access the automatically loaded attributes for this certificate.
 *
 * Returns: (transfer none): the certificate attributes
 */
GckAttributes *
gcr_pkcs11_certificate_get_attributes (GcrPkcs11Certificate *self)
{
	g_return_val_if_fail (GCR_IS_PKCS11_CERTIFICATE (self), NULL);
	return self->pv->attrs;
}

/**
 * gcr_pkcs11_certificate_lookup_issuer:
 * @certificate: a #GcrCertificate
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Lookup a the issuer of a @certificate in the PKCS#11 storage. The
 * lookup is done using the issuer DN of the certificate. No certificate chain
 * verification is done. Use a crypto library to make trust decisions.
 *
 * This call may block, see gcr_pkcs11_certificate_lookup_issuer() for the
 * non-blocking version.
 *
 * Will return %NULL if no issuer certificate is found. Use @error to determine
 * if an error occurred.
 *
 * Returns: (transfer full): a new #GcrPkcs11Certificate, or %NULL
 */
GcrCertificate *
gcr_pkcs11_certificate_lookup_issuer (GcrCertificate *certificate, GCancellable *cancellable,
                                      GError **error)
{
	GckAttributes *search;
	GcrCertificate *issuer;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (certificate), NULL);

	if (!gcr_pkcs11_initialize (cancellable, error))
		return NULL;

	search = prepare_lookup_certificate_issuer (certificate);
	g_return_val_if_fail (search, FALSE);

	issuer = perform_lookup_certificate (search, cancellable, error);
	gck_attributes_unref (search);

	return issuer;
}

/**
 * gcr_pkcs11_certificate_lookup_issuer_async:
 * @certificate: a #GcrCertificate
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback to call when the operation completes
 * @user_data: the data to pass to callback function
 *
 * Lookup a the issuer of a @certificate in the PKCS#11 storage. The
 * lookup is done using the issuer DN of the certificate. No certificate chain
 * verification is done. Use a crypto library to make trust decisions.
 *
 * When the operation is finished, callback will be called. You can then call
 * gcr_pkcs11_certificate_lookup_issuer_finish() to get the result of the
 * operation.
 */
void
gcr_pkcs11_certificate_lookup_issuer_async (GcrCertificate *certificate, GCancellable *cancellable,
                                            GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;
	GckAttributes *search;

	g_return_if_fail (GCR_IS_CERTIFICATE (certificate));

	task = g_task_new (certificate, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_pkcs11_certificate_lookup_issuer_async);

	search = prepare_lookup_certificate_issuer (certificate);
	g_return_if_fail (search);
	g_task_set_task_data (task, search, gck_attributes_unref);

	g_task_run_in_thread (task, thread_lookup_certificate);

	g_object_unref (task);
}

/**
 * gcr_pkcs11_certificate_lookup_issuer_finish:
 * @result: the #GAsyncResult passed to the callback
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous operation started by
 * gcr_pkcs11_certificate_lookup_issuer_async().
 *
 * Will return %NULL if no issuer certificate is found. Use @error to determine
 * if an error occurred.
 *
 * Returns: (transfer full): a new #GcrPkcs11Certificate, or %NULL
 */
GcrCertificate *
gcr_pkcs11_certificate_lookup_issuer_finish (GAsyncResult *result, GError **error)
{
	GObject *source;

	g_return_val_if_fail (G_IS_TASK (result), NULL);

	source = g_task_get_source_object (G_TASK (result));
	g_return_val_if_fail (g_task_is_valid (result, source), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * gcr_pkcs11_certificate_new_from_uri:
 * @pkcs11_uri: a A PKCS \#11 URI
 * @cancellable: (nullable): a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Lookup a certificate in the PKCS#11 storage by the given URI.
 *
 * This call may block, see gcr_pkcs11_certificate_new_from_uri_async() for the
 * non-blocking version.
 *
 * Will return %NULL if no certificate is found. Use @error to determine
 * if an error occurred.
 *
 * Returns: (transfer full) (nullable): a new #GcrCertificate, or %NULL
 */
GcrCertificate *
gcr_pkcs11_certificate_new_from_uri (const gchar *pkcs11_uri,
				     GCancellable *cancellable,
				     GError **error)
{
	GckUriData *data;
	GcrCertificate *cert;

	g_return_val_if_fail (pkcs11_uri != NULL, NULL);
	g_return_val_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	data = gck_uri_data_parse (pkcs11_uri,  GCK_URI_FOR_OBJECT, error);
	g_return_val_if_fail (data != NULL, NULL);

	cert = perform_lookup_certificate (data->attributes, cancellable, error);
	gck_uri_data_free (data);

	return cert;
}

/**
 * gcr_pkcs11_certificate_new_from_uri_async:
 * @pkcs11_uri: a A PKCS \#11 URI
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to call when the operation completes
 * @user_data: the data to pass to callback function
 *
 * Lookup a certificate in the PKCS#11 storage by the given URI.
 *
 * When the operation is finished, callback will be called. You can then call
 * gcr_pkcs11_certificate_new_from_uri_finish() to get the result of the
 * operation.
 */
void
gcr_pkcs11_certificate_new_from_uri_async (const gchar *pkcs11_uri,
					   GCancellable *cancellable,
					   GAsyncReadyCallback callback,
					   gpointer user_data)
{
	GTask *task;
	GckUriData *data;
	GError *error = NULL;

	g_return_if_fail (pkcs11_uri != NULL);
	g_return_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable));

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_pkcs11_certificate_new_from_uri_async);

	data = gck_uri_data_parse (pkcs11_uri,  GCK_URI_FOR_OBJECT, &error);
	if (data == NULL) {
		g_task_return_error (task, error);
		return;
	}

	g_task_set_task_data (task, g_steal_pointer (&data->attributes), gck_attributes_unref);
	gck_uri_data_free (data);

	g_task_run_in_thread (task, thread_lookup_certificate);

	g_object_unref (task);
}

/**
 * gcr_pkcs11_certificate_new_from_uri_finish:
 * @result: the #GAsyncResult passed to the callback
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous operation started by
 * gcr_pkcs11_certificate_new_from_uri_async().
 *
 * Will return %NULL if no certificate is found. Use @error to determine
 * if an error occurred.
 *
 * Returns: (transfer full) (nullable): a new #GcrCertificate, or %NULL
 */
GcrCertificate *
gcr_pkcs11_certificate_new_from_uri_finish (GAsyncResult *result,
					    GError **error)
{
	GObject *source;

	g_return_val_if_fail (G_IS_TASK (result), NULL);

	source = g_task_get_source_object (G_TASK (result));
	g_return_val_if_fail (g_task_is_valid (result, source), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}
