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

#include "gcr-types.h"
#include "gcr-internal.h"
#include "gcr-library.h"
#include "gcr-trust.h"

#include "gck/gck.h"
#include "gck/pkcs11n.h"
#include "gck/pkcs11i.h"
#include "gck/pkcs11x.h"

#include <glib/gi18n-lib.h>

/**
 * GCR_PURPOSE_SERVER_AUTH:
 *
 * The purpose used to verify the server certificate in a TLS connection. This
 * is the most common purpose in use.
 */

/**
 * GCR_PURPOSE_CLIENT_AUTH:
 *
 * The purpose used to verify the client certificate in a TLS connection.
 */

/**
 * GCR_PURPOSE_CODE_SIGNING:
 *
 * The purpose used to verify certificate used for the signature on signed code.
 */

/**
 * GCR_PURPOSE_EMAIL:
 *
 * The purpose used to verify certificates that are used in email communication
 * such as S/MIME.
 */

/* ----------------------------------------------------------------------------------
 * HELPERS
 */

static void
prepare_trust_attrs (GcrCertificate *certificate,
                     CK_X_ASSERTION_TYPE type,
                     GckBuilder *builder)
{
	gconstpointer data;
	gsize n_data;

	gck_builder_add_ulong (builder, CKA_CLASS, CKO_X_TRUST_ASSERTION);
	gck_builder_add_ulong (builder, CKA_X_ASSERTION_TYPE, type);

	data = gcr_certificate_get_der_data (certificate, &n_data);
	g_return_if_fail (data);
	gck_builder_add_data (builder, CKA_X_CERTIFICATE_VALUE, data, n_data);
}

/* ----------------------------------------------------------------------------------
 * GET PINNED CERTIFICATE
 */

static GckAttributes *
prepare_is_certificate_pinned (GcrCertificate *certificate, const gchar *purpose, const gchar *peer)
{
	GckBuilder builder = GCK_BUILDER_INIT;

	prepare_trust_attrs (certificate, CKT_X_PINNED_CERTIFICATE, &builder);

	gck_builder_add_string (&builder, CKA_X_PURPOSE, purpose);
	gck_builder_add_string (&builder, CKA_X_PEER, peer);

	return gck_attributes_ref_sink (gck_builder_end (&builder));
}

static gboolean
perform_is_certificate_pinned (GckAttributes *search,
                               GCancellable *cancellable,
                               GError **error)
{
	GckEnumerator *en;
	GList *slots;
	GckObject *object;

	if (!gcr_pkcs11_initialize (cancellable, error))
		return FALSE;

	slots = gcr_pkcs11_get_trust_lookup_slots ();
	g_debug ("searching for pinned certificate in %d slots",
	         g_list_length (slots));
	en = gck_slots_enumerate_objects (slots, search, 0);
	gck_list_unref_free (slots);

	object = gck_enumerator_next (en, cancellable, error);
	g_object_unref (en);

	if (object)
		g_object_unref (object);

	g_debug ("%s certificate anchor", object ? "found" : "did not find");
	return (object != NULL);
}

/**
 * gcr_trust_is_certificate_pinned:
 * @certificate: a #GcrCertificate to check
 * @purpose: the purpose string
 * @peer: the peer for this pinned
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Check if @certificate is pinned for @purpose to communicate with @peer.
 * A pinned certificate overrides all other certificate verification.
 *
 * This call may block, see gcr_trust_is_certificate_pinned_async() for the
 * non-blocking version.
 *
 * In the case of an error, %FALSE is also returned. Check @error to detect
 * if an error occurred.
 *
 * Returns: %TRUE if the certificate is pinned for the host and purpose
 */
gboolean
gcr_trust_is_certificate_pinned (GcrCertificate *certificate, const gchar *purpose,
                                 const gchar *peer, GCancellable *cancellable, GError **error)
{
	GckAttributes *search;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (certificate), FALSE);
	g_return_val_if_fail (purpose, FALSE);
	g_return_val_if_fail (peer, FALSE);

	search = prepare_is_certificate_pinned (certificate, purpose, peer);
	g_return_val_if_fail (search, FALSE);

	ret = perform_is_certificate_pinned (search, cancellable, error);
	gck_attributes_unref (search);

	return ret;
}

static void
thread_is_certificate_pinned (GTask *task, gpointer object,
                              gpointer task_data, GCancellable *cancellable)
{
	GckAttributes *attrs = task_data;
	GError *error = NULL;
	gboolean found;

	found = perform_is_certificate_pinned (attrs, cancellable, &error);
	if (error == NULL)
		g_task_return_boolean (task, found);
	else
		g_task_return_error (task, g_steal_pointer (&error));
}

/**
 * gcr_trust_is_certificate_pinned_async:
 * @certificate: a #GcrCertificate to check
 * @purpose: the purpose string
 * @peer: the peer for this pinned
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback to call when the operation completes
 * @user_data: the data to pass to callback function
 *
 * Check if @certificate is pinned for @purpose to communicate with @peer. A
 * pinned certificate overrides all other certificate verification.
 *
 * When the operation is finished, callback will be called. You can then call
 * [func@Gcr.trust_is_certificate_pinned_finish] to get the result of the
 * operation.
 */
void
gcr_trust_is_certificate_pinned_async (GcrCertificate *certificate, const gchar *purpose,
                                       const gchar *peer, GCancellable *cancellable,
                                       GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;
	GckAttributes *attrs;

	g_return_if_fail (GCR_IS_CERTIFICATE (certificate));
	g_return_if_fail (purpose);
	g_return_if_fail (peer);

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_trust_is_certificate_pinned_async);

	attrs = prepare_is_certificate_pinned (certificate, purpose, peer);
	g_return_if_fail (attrs);
	g_task_set_task_data (task, attrs, gck_attributes_unref);

	g_task_run_in_thread (task, thread_is_certificate_pinned);

	g_clear_object (&task);
}

/**
 * gcr_trust_is_certificate_pinned_finish:
 * @result: the #GAsyncResult passed to the callback
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous operation started by
 * gcr_trust_is_certificate_pinned_async().
 *
 * In the case of an error, %FALSE is also returned. Check @error to detect
 * if an error occurred.
 *
 * Returns: %TRUE if the certificate is pinned.
 */
gboolean
gcr_trust_is_certificate_pinned_finish (GAsyncResult *result, GError **error)
{
	g_return_val_if_fail (!error || !*error, FALSE);
	g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* ----------------------------------------------------------------------------------
 * ADD PINNED CERTIFICATE
 */

static GckAttributes *
prepare_add_pinned_certificate (GcrCertificate *certificate, const gchar *purpose, const gchar *peer)
{
	GckBuilder builder = GCK_BUILDER_INIT;

	prepare_trust_attrs (certificate, CKT_X_PINNED_CERTIFICATE, &builder);

	gck_builder_add_string (&builder, CKA_X_PURPOSE, purpose);
	gck_builder_add_string (&builder, CKA_X_PEER, peer);
	gck_builder_add_boolean (&builder, CKA_TOKEN, TRUE);

	return gck_attributes_ref_sink (gck_builder_end (&builder));
}

static gboolean
perform_add_pinned_certificate (GckAttributes *search,
                                GCancellable *cancellable,
                                GError **error)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	gboolean ret = FALSE;
	GError *lerr = NULL;
	GckObject *object;
	GckSession *session;
	GckSlot *slot;
	GckEnumerator *en;
	GList *slots;

	if (!gcr_pkcs11_initialize (cancellable, error))
		return FALSE;

	slots = gcr_pkcs11_get_trust_lookup_slots ();
	en = gck_slots_enumerate_objects (slots, search, CKF_RW_SESSION);
	gck_list_unref_free (slots);

	object = gck_enumerator_next (en, cancellable, &lerr);
	g_object_unref (en);

	if (lerr != NULL) {
		g_propagate_error (error, lerr);
		return FALSE;
	}

	/* It already exists */
	if (object) {
		g_object_unref (object);
		return TRUE;
	}

	gck_builder_add_all (&builder, search);

	/* TODO: Add relevant label */

	/* Find an appropriate token */
	slot = gcr_pkcs11_get_trust_store_slot ();
	if (slot == NULL) {
		g_set_error (&lerr, GCK_ERROR, CKR_FUNCTION_FAILED,
		             /* Translators: A pinned certificate is an exception which
		                trusts a given certificate explicitly for a purpose and
		                communication with a certain peer. */
		             _("Couldn’t find a place to store the pinned certificate"));
		ret = FALSE;
	} else {
		session = gck_slot_open_session (slot, CKF_RW_SESSION, NULL, &lerr);
		if (session != NULL) {
			object = gck_session_create_object (session, gck_builder_end (&builder),
			                                    cancellable, &lerr);
			if (object != NULL) {
				g_object_unref (object);
				ret = TRUE;
			}

			g_object_unref (session);
		}

		g_object_unref (slot);
	}

	gck_builder_clear (&builder);

	if (!ret)
		g_propagate_error (error, lerr);

	return ret;
}

/**
 * gcr_trust_add_pinned_certificate:
 * @certificate: a #GcrCertificate
 * @purpose: the purpose string
 * @peer: the peer for this pinned certificate
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Add a pinned @certificate for connections to @peer for @purpose. A pinned
 * certificate overrides all other certificate verification and should be
 * used with care.
 *
 * If the same pinned certificate already exists, then this operation
 * does not add another, and succeeds without error.
 *
 * This call may block, see gcr_trust_add_pinned_certificate_async() for the
 * non-blocking version.
 *
 * Returns: %TRUE if the pinned certificate is recorded successfully
 */
gboolean
gcr_trust_add_pinned_certificate (GcrCertificate *certificate, const gchar *purpose, const gchar *peer,
                                  GCancellable *cancellable, GError **error)
{
	GckAttributes *search;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (certificate), FALSE);
	g_return_val_if_fail (purpose, FALSE);
	g_return_val_if_fail (peer, FALSE);

	search = prepare_add_pinned_certificate (certificate, purpose, peer);
	g_return_val_if_fail (search, FALSE);

	ret = perform_add_pinned_certificate (search, cancellable, error);
	gck_attributes_unref (search);

	return ret;
}

static void
thread_add_pinned_certificate (GTask *task, gpointer object,
                               gpointer task_data, GCancellable *cancellable)
{
	GckAttributes *attrs = task_data;
	GError *error = NULL;

	perform_add_pinned_certificate (attrs, cancellable, &error);
	if (error == NULL)
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, g_steal_pointer (&error));
}

/**
 * gcr_trust_add_pinned_certificate_async:
 * @certificate: a #GcrCertificate
 * @purpose: the purpose string
 * @peer: the peer for this pinned certificate
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback to call when the operation completes
 * @user_data: the data to pass to callback function
 *
 * Add a pinned certificate for communication with @peer for @purpose. A pinned
 * certificate overrides all other certificate verification and should be used
 * with care.
 *
 * If the same pinned certificate already exists, then this operation
 * does not add another, and succeeds without error.
 *
 * When the operation is finished, callback will be called. You can then call
 * [func@Gcr.trust_add_pinned_certificate_finish] to get the result of the
 * operation.
 */
void
gcr_trust_add_pinned_certificate_async (GcrCertificate *certificate, const gchar *purpose,
                                        const gchar *peer, GCancellable *cancellable,
                                        GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;
	GckAttributes *attrs;

	g_return_if_fail (GCR_IS_CERTIFICATE (certificate));
	g_return_if_fail (purpose);
	g_return_if_fail (peer);

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_trust_add_pinned_certificate_async);

	attrs = prepare_add_pinned_certificate (certificate, purpose, peer);
	g_return_if_fail (attrs);
	g_task_set_task_data (task, attrs, gck_attributes_unref);

	g_task_run_in_thread (task, thread_add_pinned_certificate);

	g_clear_object (&task);
}

/**
 * gcr_trust_add_pinned_certificate_finish:
 * @result: the #GAsyncResult passed to the callback
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous operation started by
 * gcr_trust_add_pinned_certificate_async().
 *
 * Returns: %TRUE if the pinned certificate is recorded successfully
 */
gboolean
gcr_trust_add_pinned_certificate_finish (GAsyncResult *result, GError **error)
{
	g_return_val_if_fail (!error || !*error, FALSE);
	g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* -----------------------------------------------------------------------
 * REMOVE PINNED CERTIFICATE
 */

static GckAttributes *
prepare_remove_pinned_certificate (GcrCertificate *certificate, const gchar *purpose,
                                   const gchar *peer)
{
	GckBuilder builder = GCK_BUILDER_INIT;

	prepare_trust_attrs (certificate, CKT_X_PINNED_CERTIFICATE, &builder);
	gck_builder_add_string (&builder, CKA_X_PURPOSE, purpose);
	gck_builder_add_string (&builder, CKA_X_PEER, peer);

	return gck_attributes_ref_sink (gck_builder_end (&builder));
}

static gboolean
perform_remove_pinned_certificate (GckAttributes *attrs,
                                   GCancellable *cancellable,
                                   GError **error)
{
	GList *objects, *l;
	GError *lerr = NULL;
	GckEnumerator *en;
	GList *slots;

	if (!gcr_pkcs11_initialize (cancellable, error))
		return FALSE;

	slots = gcr_pkcs11_get_trust_lookup_slots ();
	en = gck_slots_enumerate_objects (slots, attrs, CKF_RW_SESSION);
	gck_list_unref_free (slots);

	/* We need an error below */
	if (error && !*error)
		*error = lerr;

	objects = gck_enumerator_next_n (en, -1, cancellable, error);
	g_object_unref (en);

	if (*error)
		return FALSE;

	for (l = objects; l; l = g_list_next (l)) {
		if (!gck_object_destroy (l->data, cancellable, error)) {

			/* In case there's a race condition */
			if (g_error_matches (*error, GCK_ERROR, CKR_OBJECT_HANDLE_INVALID)) {
				g_clear_error (error);
				continue;
			}

			gck_list_unref_free (objects);
			return FALSE;
		}
	}

	gck_list_unref_free (objects);
	return TRUE;
}

/**
 * gcr_trust_remove_pinned_certificate:
 * @certificate: a #GcrCertificate
 * @purpose: the purpose string
 * @peer: the peer for this pinned certificate
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Remove a pinned certificate for communication with @peer for @purpose.
 *
 * If the same pinned certificate does not exist, or was already removed,
 * then this operation succeeds without error.
 *
 * This call may block, see gcr_trust_remove_pinned_certificate_async() for the
 * non-blocking version.
 *
 * Returns: %TRUE if the pinned certificate no longer exists
 */
gboolean
gcr_trust_remove_pinned_certificate (GcrCertificate *certificate, const gchar *purpose, const gchar *peer,
                                     GCancellable *cancellable, GError **error)
{
	GckAttributes *search;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (certificate), FALSE);
	g_return_val_if_fail (purpose, FALSE);
	g_return_val_if_fail (peer, FALSE);

	search = prepare_remove_pinned_certificate (certificate, purpose, peer);
	g_return_val_if_fail (search, FALSE);

	ret = perform_remove_pinned_certificate (search, cancellable, error);
	gck_attributes_unref (search);

	return ret;
}

static void
thread_remove_pinned_certificate (GTask *task, gpointer object,
                                  gpointer task_data, GCancellable *cancellable)
{
	GckAttributes *attrs = task_data;
	GError *error = NULL;

	perform_remove_pinned_certificate (attrs, cancellable, &error);

	if (error == NULL)
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, g_steal_pointer (&error));
}

/**
 * gcr_trust_remove_pinned_certificate_async:
 * @certificate: a #GcrCertificate
 * @purpose: the purpose string
 * @peer: the peer for this pinned certificate
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback to call when the operation completes
 * @user_data: the data to pass to callback function
 *
 * Remove a pinned certificate for communication with @peer for @purpose.
 *
 * If the same pinned certificate does not exist, or was already removed,
 * then this operation succeeds without error.
 *
 * When the operation is finished, callback will be called. You can then call
 * gcr_trust_remove_pinned_certificate_finish() to get the result of the
 * operation.
 */
void
gcr_trust_remove_pinned_certificate_async (GcrCertificate *certificate,
                                           const gchar *purpose,
                                           const gchar *peer,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
	GTask *task;
	GckAttributes *attrs;

	g_return_if_fail (GCR_IS_CERTIFICATE (certificate));
	g_return_if_fail (purpose);
	g_return_if_fail (peer);

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_trust_remove_pinned_certificate_async);

	attrs = prepare_remove_pinned_certificate (certificate, purpose, peer);
	g_return_if_fail (attrs);
	g_task_set_task_data (task, attrs, gck_attributes_unref);

	g_task_run_in_thread (task, thread_remove_pinned_certificate);

	g_clear_object (&task);
}

/**
 * gcr_trust_remove_pinned_certificate_finish:
 * @result: the #GAsyncResult passed to the callback
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous operation started by
 * gcr_trust_remove_pinned_certificate_async().
 *
 * Returns: %TRUE if the pinned certificate no longer exists
 */
gboolean
gcr_trust_remove_pinned_certificate_finish (GAsyncResult *result, GError **error)
{
	g_return_val_if_fail (!error || !*error, FALSE);
	g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* ----------------------------------------------------------------------------------
 * CERTIFICATE ROOT
 */

static GckAttributes *
prepare_is_certificate_anchored (GcrCertificate *certificate, const gchar *purpose)
{
	GckBuilder builder = GCK_BUILDER_INIT;

	prepare_trust_attrs (certificate, CKT_X_ANCHORED_CERTIFICATE, &builder);
	gck_builder_add_string (&builder, CKA_X_PURPOSE, purpose);

	return gck_attributes_ref_sink (gck_builder_end (&builder));
}

static gboolean
perform_is_certificate_anchored (GckAttributes *attrs,
                                 GCancellable *cancellable,
                                 GError **error)
{
	GckEnumerator *en;
	GList *slots;
	GckObject *object;

	if (!gcr_pkcs11_initialize (cancellable, error))
		return FALSE;

	slots = gcr_pkcs11_get_trust_lookup_slots ();
	g_debug ("searching for certificate anchor in %d slots",
	         g_list_length (slots));
	en = gck_slots_enumerate_objects (slots, attrs, 0);
	gck_list_unref_free (slots);

	object = gck_enumerator_next (en, cancellable, error);
	g_object_unref (en);

	if (object != NULL)
		g_object_unref (object);

	g_debug ("%s certificate anchor", object ? "found" : "did not find");
	return (object != NULL);
}

/**
 * gcr_trust_is_certificate_anchored:
 * @certificate: a #GcrCertificate to check
 * @purpose: the purpose string
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Check if the @certificate is a trust anchor for the given @purpose. A trust
 * anchor is used to verify the signatures on other certificates when verifying
 * a certificate chain. Also known as a trusted certificate authority.
 *
 * This call may block, see [func@Gcr.trust_is_certificate_anchored_async] for
 * the non-blocking version.
 *
 * In the case of an error, %FALSE is also returned. Check @error to detect
 * if an error occurred.
 *
 * Returns: %TRUE if the certificate is a trust anchor
 */
gboolean
gcr_trust_is_certificate_anchored (GcrCertificate *certificate, const gchar *purpose,
                                   GCancellable *cancellable, GError **error)
{
	GckAttributes *search;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_CERTIFICATE (certificate), FALSE);
	g_return_val_if_fail (purpose, FALSE);

	search = prepare_is_certificate_anchored (certificate, purpose);
	g_return_val_if_fail (search, FALSE);

	ret = perform_is_certificate_anchored (search, cancellable, error);
	gck_attributes_unref (search);

	return ret;
}

static void
thread_is_certificate_anchored (GTask *task, gpointer object,
                                gpointer task_data, GCancellable *cancellable)
{
	GckAttributes *attrs = task_data;
	GError *error = NULL;
	gboolean found;

	found = perform_is_certificate_anchored (attrs, cancellable, &error);
	if (error == NULL)
		g_task_return_boolean (task, found);
	else
		g_task_return_error (task, g_steal_pointer (&error));
}

/**
 * gcr_trust_is_certificate_anchored_async:
 * @certificate: a #GcrCertificate to check
 * @purpose: the purpose string
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback to call when the operation completes
 * @user_data: the data to pass to callback function
 *
 * Check if the @certificate is a trust anchor for the given @purpose. A trust
 * anchor is used to verify the signatures on other certificates when verifying
 * a certificate chain. Also known as a trusted certificate authority.
 *
 * When the operation is finished, callback will be called. You can then call
 * gcr_trust_is_certificate_anchored_finish() to get the result of the operation.
 */
void
gcr_trust_is_certificate_anchored_async (GcrCertificate *certificate, const gchar *purpose,
                                         GCancellable *cancellable, GAsyncReadyCallback callback,
                                         gpointer user_data)
{
	GTask *task;
	GckAttributes *attrs;

	g_return_if_fail (GCR_IS_CERTIFICATE (certificate));
	g_return_if_fail (purpose);

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, gcr_trust_is_certificate_anchored_async);

	attrs = prepare_is_certificate_anchored (certificate, purpose);
	g_return_if_fail (attrs);
	g_task_set_task_data (task, attrs, gck_attributes_unref);

	g_task_run_in_thread (task, thread_is_certificate_anchored);

	g_clear_object (&task);
}

/**
 * gcr_trust_is_certificate_anchored_finish:
 * @result: the #GAsyncResult passed to the callback
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous operation started by
 * gcr_trust_is_certificate_anchored_async().
 *
 * In the case of an error, %FALSE is also returned. Check @error to detect
 * if an error occurred.
 *
 * Returns: %TRUE if the certificate is a trust anchor
 */
gboolean
gcr_trust_is_certificate_anchored_finish (GAsyncResult *result, GError **error)
{
	g_return_val_if_fail (!error || !*error, FALSE);
	g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}
