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

#include "gcr-subject-public-key.h"
#include "gcr-types.h"

#include "gcr/gcr-oids.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-error.h"

#include <glib/gi18n-lib.h>

#include <gcrypt.h>

static gboolean
check_object_basics (GckBuilder *builder,
                     gulong *klass,
                     gulong *type)
{
	g_assert (klass != NULL);
	g_assert (type != NULL);

	if (!gck_builder_find_ulong (builder, CKA_CLASS, klass))
		return FALSE;

	if (*klass == CKO_PUBLIC_KEY || *klass == CKO_PRIVATE_KEY)
		return gck_builder_find_ulong (builder, CKA_KEY_TYPE, type);

	else if (*klass == CKO_CERTIFICATE)
		return gck_builder_find_ulong (builder, CKA_CERTIFICATE_TYPE, type);

	*type = GCK_INVALID;
	return FALSE;
}

static gboolean
load_object_basics (GckObject *object,
                    GckBuilder *builder,
                    GCancellable *cancellable,
                    gulong *klass,
                    gulong *type,
                    GError **lerror)
{
	gulong attr_types[] = { CKA_CLASS, CKA_KEY_TYPE, CKA_CERTIFICATE_TYPE };
	GckAttributes *attrs;
	GError *error = NULL;

	g_assert (klass != NULL);
	g_assert (type != NULL);

	if (check_object_basics (builder, klass, type)) {
		g_debug ("already loaded: class = %lu, type = %lu", *klass, *type);
		return TRUE;
	}

	attrs = gck_object_cache_lookup (object, attr_types, G_N_ELEMENTS (attr_types),
	                                 cancellable, &error);
	if (error != NULL) {
		g_debug ("couldn't load: %s", error->message);
		g_propagate_error (lerror, error);
		return FALSE;
	}

	gck_builder_set_all (builder, attrs);
	gck_attributes_unref (attrs);

	if (!check_object_basics (builder, klass, type))
		return FALSE;

	g_debug ("loaded: class = %lu, type = %lu", *klass, *type);
	return TRUE;
}

static gboolean
check_x509_attributes (GckBuilder *builder)
{
	const GckAttribute *value = gck_builder_find (builder, CKA_VALUE);
	return (value && !gck_attribute_is_invalid (value));
}

static gboolean
load_x509_attributes (GckObject *object,
                      GckBuilder *builder,
                      GCancellable *cancellable,
                      GError **lerror)
{
	gulong attr_types[] = { CKA_VALUE };
	GckAttributes *attrs;
	GError *error = NULL;

	if (check_x509_attributes (builder)) {
		g_debug ("already loaded");
		return TRUE;
	}

	attrs = gck_object_cache_lookup (object, attr_types, G_N_ELEMENTS (attr_types),
	                                 cancellable, &error);
	if (error != NULL) {
		g_debug ("couldn't load: %s", error->message);
		g_propagate_error (lerror, error);
		return FALSE;
	}

	gck_builder_set_all (builder, attrs);
	gck_attributes_unref (attrs);

	return check_x509_attributes (builder);
}

static gboolean
check_rsa_attributes (GckBuilder *builder)
{
	const GckAttribute *modulus;
	const GckAttribute *exponent;

	modulus = gck_builder_find (builder, CKA_MODULUS);
	exponent = gck_builder_find (builder, CKA_PUBLIC_EXPONENT);

	return (modulus && !gck_attribute_is_invalid (modulus) &&
	        exponent && !gck_attribute_is_invalid (exponent));
}

static gboolean
load_rsa_attributes (GckObject *object,
                     GckBuilder *builder,
                     GCancellable *cancellable,
                     GError **lerror)
{
	gulong attr_types[] = { CKA_MODULUS, CKA_PUBLIC_EXPONENT };
	GckAttributes *attrs;
	GError *error = NULL;

	if (check_rsa_attributes (builder)) {
		g_debug ("rsa attributes already loaded");
		return TRUE;
	}

	attrs = gck_object_cache_lookup (object, attr_types, G_N_ELEMENTS (attr_types),
	                                 cancellable, &error);
	if (error != NULL) {
		g_debug ("couldn't load rsa attributes: %s", error->message);
		g_propagate_error (lerror, error);
		return FALSE;
	}

	gck_builder_set_all (builder, attrs);
	gck_attributes_unref (attrs);

	return check_rsa_attributes (builder);
}

static GckObject *
lookup_public_key (GckObject *object,
                   GCancellable *cancellable,
                   GError **lerror)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	gulong attr_types[] = { CKA_ID };
	GckAttributes *attrs;
	GError *error = NULL;
	GckSession *session;
	GckObject *result;
	const GckAttribute *id;
	GList *objects;

	attrs = gck_object_cache_lookup (object, attr_types, G_N_ELEMENTS (attr_types),
	                                 cancellable, &error);
	if (error != NULL) {
		g_debug ("couldn't load private key id: %s", error->message);
		g_propagate_error (lerror, error);
		return NULL;
	}

	id = gck_attributes_find (attrs, CKA_ID);
	if (id == NULL || gck_attribute_is_invalid (id)) {
		gck_attributes_unref (attrs);
		g_debug ("couldn't load private key id");
		g_set_error_literal (lerror, GCK_ERROR, CKR_ATTRIBUTE_TYPE_INVALID,
		                     gck_message_from_rv (CKR_ATTRIBUTE_TYPE_INVALID));
		return NULL;
	}

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_PUBLIC_KEY);
	gck_builder_add_attribute (&builder, id);
	gck_attributes_unref (attrs);

	session = gck_object_get_session (object);
	attrs = gck_builder_end (&builder);
	objects = gck_session_find_objects (session, attrs, cancellable, &error);
	gck_attributes_unref (attrs);
	g_object_unref (session);

	if (error != NULL) {
		g_debug ("couldn't lookup public key: %s", error->message);
		g_propagate_error (lerror, error);
		return NULL;
	}

	if (!objects)
		return NULL;

	result = g_object_ref (objects->data);
	g_clear_list (&objects, g_object_unref);

	return result;
}

static gboolean
check_dsa_attributes (GckBuilder *builder)
{
	const GckAttribute *prime;
	const GckAttribute *subprime;
	const GckAttribute *base;
	const GckAttribute *value;

	prime = gck_builder_find (builder, CKA_PRIME);
	subprime = gck_builder_find (builder, CKA_SUBPRIME);
	base = gck_builder_find (builder, CKA_BASE);
	value = gck_builder_find (builder, CKA_VALUE);

	return (prime && !gck_attribute_is_invalid (prime) &&
	        subprime && !gck_attribute_is_invalid (subprime) &&
	        base && !gck_attribute_is_invalid (base) &&
	        value && !gck_attribute_is_invalid (value));
}

static gboolean
load_dsa_attributes (GckObject *object,
                     GckBuilder *builder,
                     GCancellable *cancellable,
                     GError **lerror)
{
	gulong attr_types[] = { CKA_PRIME, CKA_SUBPRIME, CKA_BASE, CKA_VALUE };
	GError *error = NULL;
	GckAttributes *loaded;
	GckObject *publi;
	gulong klass;

	if (check_dsa_attributes (builder))
		return TRUE;

	if (!gck_builder_find_ulong (builder, CKA_CLASS, &klass))
		g_return_val_if_reached (FALSE);

	/* If it's a private key, find the public one */
	if (klass == CKO_PRIVATE_KEY)
		publi = lookup_public_key (object, cancellable, lerror);

	else
		publi = g_object_ref (object);

	if (!publi)
		return FALSE;

	loaded = gck_object_cache_lookup (publi, attr_types, G_N_ELEMENTS (attr_types),
	                                  cancellable, &error);
	g_object_unref (publi);

	if (error != NULL) {
		g_debug ("couldn't load rsa attributes: %s", error->message);
		g_propagate_error (lerror, error);
		return FALSE;
	}

	/* We've made sure to load info from the public key, so change class */
	gck_builder_set_ulong (builder, CKA_CLASS, CKO_PUBLIC_KEY);

	gck_builder_set_all (builder, loaded);
	gck_attributes_unref (loaded);

	return check_dsa_attributes (builder);
}

static gboolean
check_ec_attributes (GckBuilder *builder)
{
	const GckAttribute *ec_params;
	const GckAttribute *ec_point;

	ec_params = gck_builder_find (builder, CKA_EC_PARAMS);
	ec_point = gck_builder_find (builder, CKA_EC_POINT);

	return (ec_params && !gck_attribute_is_invalid (ec_params) &&
	        ec_point && !gck_attribute_is_invalid (ec_point));
}


static gboolean
load_ec_attributes (GckObject *object,
                    GckBuilder *builder,
                    GCancellable *cancellable,
                    GError **lerror)
{
	gulong attr_types[] = { CKA_EC_PARAMS, CKA_EC_POINT };
	GckAttributes *attrs;
	GError *error = NULL;
	GckObject *publi;
	gulong klass;

	if (check_ec_attributes (builder)) {
		g_debug ("ec attributes already loaded");
		return TRUE;
	}

	if (!gck_builder_find_ulong (builder, CKA_CLASS, &klass))
		g_return_val_if_reached (FALSE);

	/* If it's a private key, find the public one */
	if (klass == CKO_PRIVATE_KEY)
		publi = lookup_public_key (object, cancellable, lerror);

	else
		publi = g_object_ref (object);

	if (!publi)
		return FALSE;

	attrs = gck_object_cache_lookup (publi, attr_types, G_N_ELEMENTS (attr_types),
	                                 cancellable, &error);
	g_object_unref (publi);

	if (error != NULL) {
		g_debug ("couldn't load ec attributes: %s", error->message);
		g_propagate_error (lerror, error);
		return FALSE;
	}

	gck_builder_set_all (builder, attrs);
	gck_attributes_unref (attrs);

	return check_ec_attributes (builder);
}

static gboolean
load_attributes (GckObject *object,
                 GckBuilder *builder,
                 GCancellable *cancellable,
                 GError **lerror)
{
	gboolean ret = FALSE;
	gulong klass;
	gulong type;

	if (!load_object_basics (object, builder, cancellable,
	                         &klass, &type, lerror))
		return FALSE;

	switch (klass) {

	case CKO_CERTIFICATE:
		switch (type) {
		case CKC_X_509:
			ret = load_x509_attributes (object, builder, cancellable, lerror);
			break;
		default:
			g_debug ("unsupported certificate type: %lu", type);
			break;
		}
		break;

	case CKO_PUBLIC_KEY:
	case CKO_PRIVATE_KEY:
		switch (type) {
		case CKK_RSA:
			ret = load_rsa_attributes (object, builder, cancellable, lerror);
			break;
		case CKK_DSA:
			ret = load_dsa_attributes (object, builder, cancellable, lerror);
			break;
		case CKK_EC:
			ret = load_ec_attributes (object, builder, cancellable, lerror);
			break;
		default:
			g_debug ("unsupported key type: %lu", type);
			break;
		}
		break;

	default:
		g_debug ("unsupported class: %lu", type);
		break;
	}

	if (ret == FALSE && lerror != NULL && *lerror == NULL) {
		g_set_error_literal (lerror, GCR_DATA_ERROR, GCR_ERROR_UNRECOGNIZED,
		                     _("Unrecognized or unavailable attributes for key"));
	}

	return ret;
}

static gboolean
check_attributes (GckBuilder *builder)
{
	gulong klass;
	gulong type;

	if (!check_object_basics (builder, &klass, &type))
		return FALSE;

	switch (klass) {

	case CKO_CERTIFICATE:
		switch (type) {
		case CKC_X_509:
			return check_x509_attributes (builder);
		default:
			return FALSE;
		}

	case CKO_PUBLIC_KEY:
	case CKO_PRIVATE_KEY:
		switch (type) {
		case CKK_RSA:
			return check_rsa_attributes (builder);
		case CKK_DSA:
			return check_dsa_attributes (builder);
		case CKK_EC:
			return check_ec_attributes (builder);
		default:
			return FALSE;
		}

	default:
		return FALSE;
	}
}

static void
lookup_attributes (GckObject *object,
                   GckBuilder *builder)
{
	GckObjectCache *oakey;
	GckAttributes *attrs;

	if (GCK_IS_OBJECT_CACHE (object)) {
		oakey = GCK_OBJECT_CACHE (object);
		attrs = gck_object_cache_get_attributes (oakey);
		if (attrs != NULL) {
			gck_builder_add_all (builder, attrs);
			gck_attributes_unref (attrs);
		}
	}
}

GNode *
_gcr_subject_public_key_load (GckObject *key,
                              GCancellable *cancellable,
                              GError **error)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attributes;
	GNode *asn;

	g_return_val_if_fail (GCK_IS_OBJECT (key), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	lookup_attributes (key, &builder);

	if (!check_attributes (&builder)) {
		if (!load_attributes (key, &builder, cancellable, error)) {
			gck_builder_clear (&builder);
			return NULL;
		}
	}

	attributes = gck_builder_end (&builder);
	asn = _gcr_subject_public_key_for_attributes (attributes);
	if (asn == NULL) {
		g_set_error_literal (error, GCK_ERROR, CKR_TEMPLATE_INCONSISTENT,
		                     _("Couldn’t build public key"));
	}

	gck_attributes_unref (attributes);
	return asn;
}

typedef struct {
	GckObject *object;
	GckBuilder builder;
} LoadClosure;

static void
load_closure_free (gpointer data)
{
	LoadClosure *closure = data;
	g_object_unref (closure->object);
	gck_builder_clear (&closure->builder);
	g_free (closure);
}

static void
thread_key_attributes (GTask *task, gpointer src_object, gpointer task_data,
                       GCancellable *cancellable)
{
	LoadClosure *closure = task_data;
	GError *error = NULL;

	if (load_attributes (closure->object, &closure->builder, cancellable, &error))
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, g_steal_pointer (&error));
}

void
_gcr_subject_public_key_load_async (GckObject *key,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	GTask *task;
	LoadClosure *closure;

	g_return_if_fail (GCK_IS_OBJECT (key));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gcr_subject_public_key_load_async);

	closure = g_new0 (LoadClosure, 1);
	closure->object = g_object_ref (key);
	lookup_attributes (key, &closure->builder);
	g_task_set_task_data (task, closure, load_closure_free);

	if (check_attributes (&closure->builder)) {
		g_task_return_boolean (task, TRUE);
		g_clear_object (&task);
		return;
	}

	g_task_run_in_thread (task, thread_key_attributes);
	g_clear_object (&task);
}

GNode *
_gcr_subject_public_key_load_finish (GAsyncResult *result,
                                     GError **error)
{
	GckAttributes *attributes;
	LoadClosure *closure;
	GNode *asn;

	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);

	if (!g_task_propagate_boolean (G_TASK (result), error))
		return NULL;

	closure = g_task_get_task_data (G_TASK (result));
	attributes = gck_builder_end (&closure->builder);
	asn = _gcr_subject_public_key_for_attributes (attributes);
	if (asn == NULL) {
		g_set_error_literal (error, GCK_ERROR, CKR_TEMPLATE_INCONSISTENT,
		                     _("Couldn’t build public key"));
	}

	gck_attributes_unref (attributes);
	return asn;
}

static gboolean
rsa_subject_public_key_from_attributes (GckAttributes *attrs,
                                        GNode *info_asn)
{
	const GckAttribute *modulus;
	const GckAttribute *exponent;
	GNode *key_asn;
	GNode *params_asn;
	GBytes *key;
	GBytes *usg;

	modulus = gck_attributes_find (attrs, CKA_MODULUS);
	exponent = gck_attributes_find (attrs, CKA_PUBLIC_EXPONENT);
	if (modulus == NULL || gck_attribute_is_invalid (modulus) ||
	    exponent == NULL || gck_attribute_is_invalid (exponent))
		return FALSE;

	key_asn = egg_asn1x_create (pk_asn1_tab, "RSAPublicKey");
	g_return_val_if_fail (key_asn, FALSE);

	params_asn = egg_asn1x_create (pk_asn1_tab, "RSAParameters");
	g_return_val_if_fail (params_asn, FALSE);

	usg = g_bytes_new_with_free_func (modulus->value, modulus->length,
	                                    gck_attributes_unref,
	                                    gck_attributes_ref (attrs));
	egg_asn1x_set_integer_as_usg (egg_asn1x_node (key_asn, "modulus", NULL), usg);
	g_bytes_unref (usg);

	usg = g_bytes_new_with_free_func (exponent->value, exponent->length,
	                                    gck_attributes_unref,
	                                    gck_attributes_ref (attrs));
	egg_asn1x_set_integer_as_usg (egg_asn1x_node (key_asn, "publicExponent", NULL), usg);
	g_bytes_unref (usg);

	key = egg_asn1x_encode (key_asn, NULL);
	egg_asn1x_destroy (key_asn);

	egg_asn1x_set_null (params_asn);

	egg_asn1x_set_bits_as_raw (egg_asn1x_node (info_asn, "subjectPublicKey", NULL),
	                           key, g_bytes_get_size (key) * 8);

	egg_asn1x_set_oid_as_quark (egg_asn1x_node (info_asn, "algorithm", "algorithm", NULL), GCR_OID_PKIX1_RSA);
	egg_asn1x_set_any_from (egg_asn1x_node (info_asn, "algorithm", "parameters", NULL), params_asn);

	egg_asn1x_destroy (params_asn);
	g_bytes_unref (key);
	return TRUE;
}

static gboolean
dsa_subject_public_key_from_private (GNode *key_asn,
                                     const GckAttribute *ap,
                                     const GckAttribute *aq,
                                     const GckAttribute *ag,
                                     const GckAttribute *ax)
{
	gcry_mpi_t mp, mq, mg, mx, my;
	size_t n_buffer;
	gcry_error_t gcry;
	unsigned char *buffer;

	gcry = gcry_mpi_scan (&mp, GCRYMPI_FMT_USG, ap->value, ap->length, NULL);
	g_return_val_if_fail (gcry == 0, FALSE);

	gcry = gcry_mpi_scan (&mq, GCRYMPI_FMT_USG, aq->value, aq->length, NULL);
	g_return_val_if_fail (gcry == 0, FALSE);

	gcry = gcry_mpi_scan (&mg, GCRYMPI_FMT_USG, ag->value, ag->length, NULL);
	g_return_val_if_fail (gcry == 0, FALSE);

	gcry = gcry_mpi_scan (&mx, GCRYMPI_FMT_USG, ax->value, ax->length, NULL);
	g_return_val_if_fail (gcry == 0, FALSE);

	/* Calculate the public part from the private */
	my = gcry_mpi_snew (gcry_mpi_get_nbits (mx));
	g_return_val_if_fail (my, FALSE);
	gcry_mpi_powm (my, mg, mx, mp);

	gcry = gcry_mpi_aprint (GCRYMPI_FMT_STD, &buffer, &n_buffer, my);
	g_return_val_if_fail (gcry == 0, FALSE);
	egg_asn1x_take_integer_as_raw (key_asn, g_bytes_new_with_free_func (buffer, n_buffer,
	                                                                      gcry_free, buffer));

	gcry_mpi_release (mp);
	gcry_mpi_release (mq);
	gcry_mpi_release (mg);
	gcry_mpi_release (mx);
	gcry_mpi_release (my);

	return TRUE;
}

static gboolean
dsa_subject_public_key_from_attributes (GckAttributes *attrs,
                                        gulong klass,
                                        GNode *info_asn)
{
	const GckAttribute *value, *g, *q, *p;
	GNode *key_asn, *params_asn;
	GBytes *key;

	p = gck_attributes_find (attrs, CKA_PRIME);
	q = gck_attributes_find (attrs, CKA_SUBPRIME);
	g = gck_attributes_find (attrs, CKA_BASE);
	value = gck_attributes_find (attrs, CKA_VALUE);

	if (p == NULL || gck_attribute_is_invalid (p) ||
	    q == NULL || gck_attribute_is_invalid (q) ||
	    g == NULL || gck_attribute_is_invalid (g) ||
	    value == NULL || gck_attribute_is_invalid (value))
		return FALSE;

	key_asn = egg_asn1x_create (pk_asn1_tab, "DSAPublicPart");
	g_return_val_if_fail (key_asn, FALSE);

	params_asn = egg_asn1x_create (pk_asn1_tab, "DSAParameters");
	g_return_val_if_fail (params_asn, FALSE);

	egg_asn1x_take_integer_as_usg (egg_asn1x_node (params_asn, "p", NULL),
	                               g_bytes_new_with_free_func (p->value, p->length,
	                                                             gck_attributes_unref,
	                                                             gck_attributes_ref (attrs)));
	egg_asn1x_take_integer_as_usg (egg_asn1x_node (params_asn, "q", NULL),
	                               g_bytes_new_with_free_func (q->value, q->length,
	                                                             gck_attributes_unref,
	                                                             gck_attributes_ref (attrs)));
	egg_asn1x_take_integer_as_usg (egg_asn1x_node (params_asn, "g", NULL),
	                               g_bytes_new_with_free_func (g->value, g->length,
	                                                             gck_attributes_unref,
	                                                             gck_attributes_ref (attrs)));

	/* Are these attributes for a public or private key? */
	if (klass == CKO_PRIVATE_KEY) {

		/* We need to calculate the public from the private key */
		if (!dsa_subject_public_key_from_private (key_asn, p, q, g, value))
			g_return_val_if_reached (FALSE);

	} else if (klass == CKO_PUBLIC_KEY) {
		egg_asn1x_take_integer_as_usg (key_asn,
		                               g_bytes_new_with_free_func (value->value, value->length,
		                                                             gck_attributes_unref,
		                                                             gck_attributes_ref (attrs)));
	} else {
		g_assert_not_reached ();
	}

	key = egg_asn1x_encode (key_asn, NULL);
	egg_asn1x_destroy (key_asn);

	egg_asn1x_set_bits_as_raw (egg_asn1x_node (info_asn, "subjectPublicKey", NULL),
	                           key, g_bytes_get_size (key) * 8);
	egg_asn1x_set_any_from (egg_asn1x_node (info_asn, "algorithm", "parameters", NULL), params_asn);

	egg_asn1x_set_oid_as_quark (egg_asn1x_node (info_asn, "algorithm", "algorithm", NULL), GCR_OID_PKIX1_DSA);

	g_bytes_unref (key);
	egg_asn1x_destroy (params_asn);
	return TRUE;
}

static gboolean
ec_subject_public_key_from_attributes (GckAttributes *attrs,
                                       gulong klass,
                                       GNode *info_asn)
{
	const GckAttribute *ec_params, *ec_point;
	GNode *params_asn, *point_asn;
	GBytes *bytes, *key_bytes;

	ec_params = gck_attributes_find (attrs, CKA_EC_PARAMS);
	ec_point = gck_attributes_find (attrs, CKA_EC_POINT);

	if (ec_params == NULL || gck_attribute_is_invalid (ec_params) ||
	    ec_point == NULL || gck_attribute_is_invalid (ec_point))
		return FALSE;

	bytes = g_bytes_new_with_free_func (ec_params->value, ec_params->length,
	                                    gck_attributes_unref, gck_attributes_ref (attrs));
	params_asn = egg_asn1x_create_and_decode (pk_asn1_tab, "ECParameters", bytes);
	g_bytes_unref (bytes);

	if (params_asn == NULL)
		return FALSE;

	bytes = g_bytes_new_with_free_func (ec_point->value, ec_point->length,
	                                    gck_attributes_unref, gck_attributes_ref (attrs));
	point_asn = egg_asn1x_create_and_decode (pk_asn1_tab, "ECPoint", bytes);
	g_bytes_unref (bytes);

	if (point_asn == NULL) {
		egg_asn1x_destroy (params_asn);
		return FALSE;
	}
	key_bytes = egg_asn1x_get_string_as_bytes (point_asn);
	egg_asn1x_destroy (point_asn);
	if (key_bytes == NULL) {
		egg_asn1x_destroy (params_asn);
		return FALSE;
	}

	egg_asn1x_set_bits_as_raw (egg_asn1x_node (info_asn, "subjectPublicKey", NULL),
	                           key_bytes, g_bytes_get_size (key_bytes) * 8);
	egg_asn1x_set_any_from (egg_asn1x_node (info_asn, "algorithm", "parameters", NULL), params_asn);

	egg_asn1x_set_oid_as_quark (egg_asn1x_node (info_asn, "algorithm", "algorithm", NULL), GCR_OID_PKIX1_EC);

	g_bytes_unref (key_bytes);
	egg_asn1x_destroy (params_asn);
	return TRUE;
}

static GNode *
cert_subject_public_key_from_attributes (GckAttributes *attributes)
{
	const GckAttribute *attr;
	GBytes *bytes;
	GNode *cert;
	GNode *asn;

	attr = gck_attributes_find (attributes, CKA_VALUE);
	if (attr == NULL || gck_attribute_is_invalid (attr)) {
		g_debug ("no value attribute for certificate");
		return NULL;
	}

	bytes = g_bytes_new_with_free_func (attr->value, attr->length,
	                                      gck_attributes_unref,
	                                      gck_attributes_ref (attributes));
	cert = egg_asn1x_create_and_decode (pkix_asn1_tab, "Certificate", bytes);
	g_bytes_unref (bytes);

	if (cert == NULL) {
		g_debug ("couldn't parse certificate value");
		return NULL;
	}

	asn = egg_asn1x_node (cert, "tbsCertificate", "subjectPublicKeyInfo", NULL);
	g_return_val_if_fail (asn != NULL, NULL);

	/* Remove the subject public key out of the certificate */
	g_node_unlink (asn);
	egg_asn1x_destroy (cert);

	return asn;
}

GNode *
_gcr_subject_public_key_for_attributes (GckAttributes *attributes)
{
	gboolean ret = FALSE;
	gulong key_type;
	gulong klass;
	GNode *asn = NULL;

	if (!gck_attributes_find_ulong (attributes, CKA_CLASS, &klass)) {
		g_debug ("no class in attributes");
		return NULL;
	}

	if (klass == CKO_CERTIFICATE) {
		return cert_subject_public_key_from_attributes (attributes);

	} else if (klass == CKO_PUBLIC_KEY || klass == CKO_PRIVATE_KEY) {
		if (!gck_attributes_find_ulong (attributes, CKA_KEY_TYPE, &key_type)) {
			g_debug ("no key type in attributes");
			return NULL;
		}

		asn = egg_asn1x_create (pkix_asn1_tab, "SubjectPublicKeyInfo");
		g_return_val_if_fail (asn, NULL);

		if (key_type == CKK_RSA) {
			ret = rsa_subject_public_key_from_attributes (attributes, asn);

		} else if (key_type == CKK_DSA) {
			ret = dsa_subject_public_key_from_attributes (attributes, klass, asn);

		} else if (key_type == CKK_ECDSA) {
			ret = ec_subject_public_key_from_attributes (attributes, klass, asn);

		} else {
			g_debug ("unsupported key type: %lu", key_type);
			ret = FALSE;
		}


		if (ret == FALSE) {
			egg_asn1x_destroy (asn);
			asn = NULL;
		}
	}

	return asn;
}

static guint
calculate_rsa_key_size (GBytes *data)
{
	GNode *asn;
	GBytes *content;
	guint key_size;

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "RSAPublicKey", data);
	g_return_val_if_fail (asn, 0);

	content = egg_asn1x_get_value_raw (egg_asn1x_node (asn, "modulus", NULL));
	if (!content)
		g_return_val_if_reached (0);

	egg_asn1x_destroy (asn);

	/* Removes the complement */
	key_size = (g_bytes_get_size (content) / 2) * 2 * 8;

	g_bytes_unref (content);
	return key_size;
}

static guint
attributes_rsa_key_size (GckAttributes *attrs)
{
	const GckAttribute *attr;
	gulong bits;

	attr = gck_attributes_find (attrs, CKA_MODULUS);

	/* Calculate the bit length, and remove the complement */
	if (attr != NULL)
		return (attr->length / 2) * 2 * 8;

	if (gck_attributes_find_ulong (attrs, CKA_MODULUS_BITS, &bits))
		return (gint)bits;

	return 0;
}

static guint
calculate_dsa_params_size (GNode *params)
{
	GNode *asn;
	GBytes *content;
	guint key_size;

	asn = egg_asn1x_get_any_as (params, pk_asn1_tab, "DSAParameters");
	g_return_val_if_fail (asn, 0);

	content = egg_asn1x_get_value_raw (egg_asn1x_node (asn, "p", NULL));
	if (!content)
		g_return_val_if_reached (0);

	egg_asn1x_destroy (asn);

	/* Removes the complement */
	key_size = (g_bytes_get_size (content) / 2) * 2 * 8;

	g_bytes_unref (content);
	return key_size;
}

static guint
attributes_dsa_key_size (GckAttributes *attrs)
{
	const GckAttribute *attr;
	gulong bits;

	attr = gck_attributes_find (attrs, CKA_PRIME);

	/* Calculate the bit length, and remove the complement */
	if (attr != NULL)
		return (attr->length / 2) * 2 * 8;

	if (gck_attributes_find_ulong (attrs, CKA_PRIME_BITS, &bits))
		return (gint)bits;

	return 0;
}

static guint
named_curve_size (GNode *params)
{
	GQuark oid;
	guint size;

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (params, "namedCurve", NULL));
	if (oid == GCR_OID_EC_SECP192R1)
		size = 192;
	else if (oid == GCR_OID_EC_SECT163K1)
		size = 163;
	else if (oid == GCR_OID_EC_SECT163R2)
		size = 163;
	else if (oid == GCR_OID_EC_SECP224R1)
		size = 224;
	else if (oid == GCR_OID_EC_SECT233K1)
		size = 233;
	else if (oid == GCR_OID_EC_SECT233R1)
		size = 233;
	else if (oid == GCR_OID_EC_SECP256R1)
		size = 256;
	else if (oid == GCR_OID_EC_SECT283K1)
		size = 283;
	else if (oid == GCR_OID_EC_SECT283R1)
		size = 283;
	else if (oid == GCR_OID_EC_SECP384R1)
		size = 384;
	else if (oid == GCR_OID_EC_SECT409K1)
		size = 409;
	else if (oid == GCR_OID_EC_SECT409R1)
		size = 409;
	else if (oid == GCR_OID_EC_SECP521R1)
		size = 521;
	else if (oid == GCR_OID_EC_SECP571K1)
		size = 571;
	else if (oid == GCR_OID_EC_SECT571R1)
		size = 571;
	else
		size = 0;
	return size;

}

static guint
calculate_ec_params_size (GNode *params)
{
	GNode *asn;
	guint size;

	asn = egg_asn1x_get_any_as (params, pk_asn1_tab, "ECParameters");
	g_return_val_if_fail (asn, 0);

	size = named_curve_size (asn);
	egg_asn1x_destroy (asn);

	return size;
}

static guint
gost_curve_size (GNode *params)
{
	GQuark oid;
	guint size;

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (params, "publicKeyParamSet", NULL));
	if (oid == GCR_OID_GOSTR3410_TEST ||
	    oid == GCR_OID_GOSTR3410_CRYPTOPRO_A ||
	    oid == GCR_OID_GOSTR3410_CRYPTOPRO_B ||
	    oid == GCR_OID_GOSTR3410_CRYPTOPRO_C ||
	    oid == GCR_OID_GOSTR3410_CRYPTOPRO_XCHA ||
	    oid == GCR_OID_GOSTR3410_CRYPTOPRO_XCHB ||
	    oid == GCR_OID_GOSTR3410_GC256A ||
	    oid == GCR_OID_GOSTR3410_GC256B ||
	    oid == GCR_OID_GOSTR3410_GC256C ||
	    oid == GCR_OID_GOSTR3410_GC256D)
		size = 256;
	else if (oid == GCR_OID_GOSTR3410_512_TEST ||
		 oid == GCR_OID_GOSTR3410_GC512A ||
		 oid == GCR_OID_GOSTR3410_GC512B ||
		 oid == GCR_OID_GOSTR3410_GC512C)
		size = 512;
	else {
		g_message ("unsupported curve: %s", g_quark_to_string (oid));
		size = 0;
	}
	return size;

}

static guint
calculate_gost_params_size (GNode *params, gboolean gost2012)
{
	GNode *asn;
	guint size;

	asn = egg_asn1x_get_any_as (params, pk_asn1_tab,
				    gost2012 ?
				    "GostR3410-2012-PublicKeyParameters" :
				    "GostR3410-2001-PublicKeyParameters");
	g_return_val_if_fail (asn, 0);

	size = gost_curve_size (asn);
	egg_asn1x_destroy (asn);

	return size;
}

static guint
attributes_ec_params_size (GckAttributes *attrs)
{
	GNode *asn;
	const GckAttribute *attr;
	GBytes *bytes;
	guint size = 0;

	attr = gck_attributes_find (attrs, CKA_EC_PARAMS);

	/* Calculate the bit length, and remove the complement */
	if (attr && !gck_attribute_is_invalid (attr)) {
		bytes = g_bytes_new_with_free_func (attr->value, attr->length,
		                                    gck_attributes_unref,
		                                    gck_attributes_ref (attrs));
		asn = egg_asn1x_create_and_decode (pk_asn1_tab, "ECParameters", bytes);
		g_bytes_unref (bytes);

		if (asn)
			size = named_curve_size (asn);
		egg_asn1x_destroy (asn);
	}

	return size;
}

guint
_gcr_subject_public_key_calculate_size (GNode *subject_public_key)
{
	GBytes *key;
	GNode *params;
	guint key_size = 0;
	guint n_bits;
	GQuark oid;

	/* Figure out the algorithm */
	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (subject_public_key,
	                                                  "algorithm", "algorithm", NULL));
	g_return_val_if_fail (oid != 0, 0);

	/* RSA keys are stored in the main subjectPublicKey field */
	if (oid == GCR_OID_PKIX1_RSA) {
		key = egg_asn1x_get_bits_as_raw (egg_asn1x_node (subject_public_key, "subjectPublicKey", NULL), &n_bits);
		g_return_val_if_fail (key != NULL, 0);
		key_size = calculate_rsa_key_size (key);
		g_bytes_unref (key);

	/* The DSA key size is discovered by the prime in params */
	} else if (oid == GCR_OID_PKIX1_DSA) {
		params = egg_asn1x_node (subject_public_key, "algorithm", "parameters", NULL);
		key_size = calculate_dsa_params_size (params);

	} else if (oid == GCR_OID_PKIX1_EC) {
		params = egg_asn1x_node (subject_public_key, "algorithm", "parameters", NULL);
		key_size = calculate_ec_params_size (params);

	} else if (oid == GCR_OID_GOSTR3410_2001) {
		params = egg_asn1x_node (subject_public_key, "algorithm", "parameters", NULL);
		key_size = calculate_gost_params_size (params, FALSE);

	} else if (oid == GCR_OID_GOSTR3410_2012_256 ||
		   oid == GCR_OID_GOSTR3410_2012_512) {
		params = egg_asn1x_node (subject_public_key, "algorithm", "parameters", NULL);
		key_size = calculate_gost_params_size (params, TRUE);

	} else {
		g_message ("unsupported key algorithm: %s", g_quark_to_string (oid));
	}

	return key_size;
}

guint
_gcr_subject_public_key_attributes_size (GckAttributes *attrs)
{
	gulong key_type;

	if (!gck_attributes_find_ulong (attrs, CKA_KEY_TYPE, &key_type))
		return 0;

	switch (key_type) {
	case CKK_RSA:
		return attributes_rsa_key_size (attrs);
	case CKK_DSA:
		return attributes_dsa_key_size (attrs);
	case CKK_EC:
		return attributes_ec_params_size (attrs);
	default:
		g_message ("unsupported key algorithm: %lu", key_type);
		return 0;
	}
}
