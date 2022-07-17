/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-gck-crypto.c - the GObject PKCS#11 wrapper library

   Copyright (C) 2011 Collabora Ltd.

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gck/gck.h"
#include "gck/gck-mock.h"
#include "gck/gck-test.h"

#include "egg/egg-testing.h"
#include "egg/mock-interaction.h"

#include <glib.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	GckModule *module;
	GckSession *session;
	GckSession *session_with_auth;
} Test;

static gboolean
on_discard_handle_ignore (GckSession *self, CK_OBJECT_HANDLE handle, gpointer unused)
{
	/* Don't close the handle for this session, since it's a duplicate */
	return TRUE;
}

static void
setup (Test *test, gconstpointer unused)
{
	GError *err = NULL;
	GList *slots;
	GckSlot *slot;
	GTlsInteraction *interaction;

	/* Successful load */
	test->module = gck_module_initialize (_GCK_TEST_MODULE_PATH, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_MODULE (test->module));
	g_object_add_weak_pointer (G_OBJECT (test->module), (gpointer *)&test->module);

	slots = gck_module_get_slots (test->module, TRUE);
	g_assert_nonnull (slots);

	test->session = gck_slot_open_session (slots->data, 0, NULL, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (test->session));
	g_object_add_weak_pointer (G_OBJECT (test->session), (gpointer *)&test->session);

	slot = gck_session_get_slot (test->session);
	g_assert_nonnull (slot);

	test->session_with_auth = gck_session_from_handle (slot, gck_session_get_handle (test->session), GCK_SESSION_AUTHENTICATE);
	g_signal_connect (test->session_with_auth, "discard-handle", G_CALLBACK (on_discard_handle_ignore), NULL);
	interaction = mock_interaction_new ("booo");
	gck_session_set_interaction (test->session_with_auth, interaction);
	g_object_unref (interaction);

	g_assert_nonnull (test->session_with_auth);
	g_object_add_weak_pointer (G_OBJECT (test->session_with_auth), (gpointer *)&test->session_with_auth);

	g_object_unref (slot);
	g_clear_list (&slots, g_object_unref);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_object_unref (test->session);
	g_object_unref (test->module);
	g_object_unref (test->session_with_auth);

	egg_test_wait_for_gtask_thread (test->session || test->session_with_auth || test->module);

	g_assert_null (test->session);
	g_assert_null (test->session_with_auth);
	g_assert_null (test->module);
}

static void
fetch_async_result (GObject *source, GAsyncResult *result, gpointer user_data)
{
	*((GAsyncResult**)user_data) = result;
	g_object_ref (result);
	egg_test_wait_stop ();
}

static GckObject*
find_key (GckSession *session, CK_ATTRIBUTE_TYPE method, CK_MECHANISM_TYPE mech)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GList *objects, *l;
	GckObject *object = NULL;
	GckAttributes *attributes;
	CK_MECHANISM_TYPE_PTR mechs;
	gboolean match;
	gsize n_mechs;

	gck_builder_add_boolean (&builder, method, TRUE);
	attributes = gck_builder_end (&builder);
	objects = gck_session_find_objects (session, attributes, NULL, NULL);
	gck_attributes_unref (attributes);
	g_assert_nonnull (objects);

	for (l = objects; l; l = g_list_next (l)) {
		if (mech) {
			mechs = (gulong *)gck_object_get_data (l->data, CKA_ALLOWED_MECHANISMS,
			                                       NULL, &n_mechs, NULL);
			g_assert_nonnull (mechs);
			g_assert_cmpuint (n_mechs, ==, sizeof (CK_MECHANISM_TYPE));

			/* We know all of them only have one allowed mech */
			match = (*mechs != mech);
			g_free (mechs);

			if (match)
				continue;
		}
		object = l->data;
		g_object_ref (object);
		break;
	}

	g_clear_list (&objects, g_object_unref);
	return object;
}

static GckObject*
find_key_with_value (GckSession *session, const gchar *value)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attributes;
	GList *objects;
	GckObject *object;

	gck_builder_add_string (&builder, CKA_VALUE, value);
	attributes = gck_builder_end (&builder);
	objects = gck_session_find_objects (session, attributes, NULL, NULL);
	gck_attributes_unref (attributes);
	g_assert_nonnull (objects);

	object = g_object_ref (objects->data);
	g_clear_list (&objects, g_object_unref);
	return object;
}

static void
check_key_with_value (GckSession *session, GckObject *key, CK_OBJECT_CLASS klass, const gchar *value)
{
	GckAttributes *attrs;
	const GckAttribute *attr;
	gulong check;

	attrs = gck_object_get (key, NULL, NULL, CKA_CLASS, CKA_VALUE, GCK_INVALID);
	g_assert_nonnull (attrs);

	if (!gck_attributes_find_ulong (attrs, CKA_CLASS, &check))
		g_assert_not_reached ();
	g_assert_cmpuint (check, ==, klass);

	attr = gck_attributes_find (attrs, CKA_VALUE);
	g_assert_nonnull (attr);
	g_assert_false (gck_attribute_is_invalid (attr));
	g_assert_cmpmem (attr->value, attr->length, value, strlen (value));

	gck_attributes_unref (attrs);
}

static void
test_encrypt (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_CAPITALIZE, NULL, 0 };
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GckObject *key;
	guchar *output;
	gsize n_output;

	/* Find the right key */
	key = find_key (test->session, CKA_ENCRYPT, CKM_MOCK_CAPITALIZE);
	g_assert_nonnull (key);

	/* Simple one */
	output = gck_session_encrypt (test->session, key, CKM_MOCK_CAPITALIZE, (const guchar*)"blah blah", 10, &n_output, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
	g_assert_cmpuint (n_output, ==, 10);
	g_assert_cmpstr ((gchar*)output, ==, "BLAH BLAH");
	g_free (output);

	/* Asynchronous one */
	gck_session_encrypt_async (test->session, key, &mech, (const guchar*)"second chance", 14, NULL, fetch_async_result, &result);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	output = gck_session_encrypt_finish (test->session, result, &n_output, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
	g_assert_cmpuint (n_output, ==, 14);
	g_assert_cmpstr ((gchar*)output, ==, "SECOND CHANCE");
	g_free (output);

	g_object_unref (result);
	g_object_unref (key);
}

static void
test_decrypt (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_CAPITALIZE, NULL, 0 };
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GckObject *key;
	guchar *output;
	gsize n_output;

	/* Find the right key */
	key = find_key (test->session, CKA_DECRYPT, CKM_MOCK_CAPITALIZE);
	g_assert_nonnull (key);

	/* Simple one */
	output = gck_session_decrypt (test->session, key, CKM_MOCK_CAPITALIZE, (const guchar*)"FRY???", 7, &n_output, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
	g_assert_cmpuint (n_output, ==, 7);
	g_assert_cmpstr ((gchar*)output, ==, "fry???");
	g_free (output);

	/* Asynchronous one */
	gck_session_decrypt_async (test->session, key, &mech, (const guchar*)"FAT CHANCE", 11, NULL, fetch_async_result, &result);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	output = gck_session_decrypt_finish (test->session, result, &n_output, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
	g_assert_cmpuint (n_output, ==, 11);
	g_assert_cmpstr ((gchar*)output, ==, "fat chance");
	g_free (output);

	g_object_unref (result);
	g_object_unref (key);
}

static void
test_login_context_specific (Test *test, gconstpointer unused)
{
	/* The test module won't let us sign without doing a login, check that */

	GError *error = NULL;
	GckObject *key;
	guchar *output;
	gsize n_output;

	/* Find the right key */
	key = find_key (test->session, CKA_SIGN, CKM_MOCK_PREFIX);
	g_assert_true (GCK_IS_OBJECT (key));
	g_object_add_weak_pointer (G_OBJECT (key), (gpointer *)&key);

	/* Simple one */
	output = gck_session_sign (test->session, key, CKM_MOCK_PREFIX, (const guchar*)"TV Monster", 11, &n_output, NULL, &error);
	g_assert_error (error, GCK_ERROR, CKR_USER_NOT_LOGGED_IN);
	g_assert_null (output);
	g_error_free (error);

	g_object_unref (key);
	egg_test_wait_for_gtask_thread (key);
	g_assert_null (key);
}

static void
test_sign (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_PREFIX, (guchar *)"my-prefix:", 10 };
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GckObject *key;
	guchar *output;
	gsize n_output;

	/* Find the right key */
	key = find_key (test->session_with_auth, CKA_SIGN, CKM_MOCK_PREFIX);
	g_assert_nonnull (key);

	/* Simple one */
	output = gck_session_sign (test->session_with_auth, key, CKM_MOCK_PREFIX, (const guchar*)"Labarbara", 10, &n_output, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
	g_assert_cmpuint (n_output, ==, 24);
	g_assert_cmpstr ((gchar*)output, ==, "signed-prefix:Labarbara");
	g_free (output);

	/* Asynchronous one */
	gck_session_sign_async (test->session_with_auth, key, &mech, (const guchar*)"Conrad", 7, NULL, fetch_async_result, &result);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	output = gck_session_sign_finish (test->session_with_auth, result, &n_output, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
	g_assert_cmpuint (n_output, ==, 17);
	g_assert_cmpstr ((gchar*)output, ==, "my-prefix:Conrad");
	g_free (output);

	g_object_unref (result);
	g_object_unref (key);
}

static void
test_verify (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_PREFIX, (guchar *)"my-prefix:", 10 };
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GTlsInteraction *interaction;
	GckObject *key;
	gboolean ret;

	/* Enable auto-login on this session, shouldn't be needed */
	interaction = mock_interaction_new ("booo");
	gck_session_set_interaction (test->session, interaction);
	g_object_unref (interaction);

	/* Find the right key */
	key = find_key (test->session, CKA_VERIFY, CKM_MOCK_PREFIX);
	g_assert_true (GCK_IS_OBJECT (key));
	g_object_add_weak_pointer (G_OBJECT (key), (gpointer *)&key);

	/* Simple one */
	ret = gck_session_verify (test->session, key, CKM_MOCK_PREFIX, (const guchar*)"Labarbara", 10,
	                           (const guchar*)"signed-prefix:Labarbara", 24, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* Failure one */
	ret = gck_session_verify_full (test->session, key, &mech, (const guchar*)"Labarbara", 10,
	                                (const guchar*)"my-prefix:Loborboro", 20, NULL, &error);
	g_assert_nonnull (error);
	g_assert_false (ret);
	g_clear_error (&error);

	/* Asynchronous one */
	gck_session_verify_async (test->session, key, &mech, (const guchar*)"Labarbara", 10,
	                           (const guchar*)"my-prefix:Labarbara", 20, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	ret = gck_session_verify_finish (test->session, result, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_object_unref (result);

	/* Asynchronous failure */
	result = NULL;
	gck_session_verify_async (test->session, key, &mech, (const guchar*)"Labarbara", 10,
	                           (const guchar*)"my-prefix:Labarxoro", 20, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	ret = gck_session_verify_finish (test->session, result, &error);
	g_assert_nonnull (error);
	g_assert_false (ret);
	g_clear_error (&error);
	g_object_unref (result);

	g_object_unref (key);
	egg_test_wait_for_gtask_thread (key);
	g_assert_null (key);
}

static void
test_generate_key_pair (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_GENERATE, (guchar *)"generate", 9 };
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *pub_attrs, *prv_attrs;
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GckObject *pub_key, *prv_key;
	gboolean ret;

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_PUBLIC_KEY);
	pub_attrs = gck_builder_end (&builder);
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_PRIVATE_KEY);
	prv_attrs = gck_builder_end (&builder);

	/* Full One*/
	ret = gck_session_generate_key_pair_full (test->session, &mech, pub_attrs, prv_attrs,
	                                           &pub_key, &prv_key, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_object_unref (pub_key);
	g_object_unref (prv_key);

	/* Failure one */
	mech.type = 0;
	pub_key = prv_key = NULL;
	ret = gck_session_generate_key_pair_full (test->session, &mech, pub_attrs, prv_attrs,
	                                           &pub_key, &prv_key, NULL, &error);
	g_assert_nonnull (error);
	g_assert_false (ret);
	g_clear_error (&error);
	g_assert_null (pub_key);
	g_assert_null (prv_key);

	/* Asynchronous one */
	mech.type = CKM_MOCK_GENERATE;
	gck_session_generate_key_pair_async (test->session, &mech, pub_attrs, prv_attrs, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	ret = gck_session_generate_key_pair_finish (test->session, result, &pub_key, &prv_key, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_object_unref (result);
	g_object_unref (pub_key);
	g_object_unref (prv_key);

	/* Asynchronous failure */
	result = NULL;
	mech.type = 0;
	pub_key = prv_key = NULL;
	gck_session_generate_key_pair_async (test->session, &mech, pub_attrs, prv_attrs, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	ret = gck_session_generate_key_pair_finish (test->session, result, &pub_key, &prv_key, &error);
	g_assert_nonnull (error);
	g_assert_false (ret);
	g_clear_error (&error);
	g_object_unref (result);
	g_assert_null (pub_key);
	g_assert_null (prv_key);

	gck_attributes_unref (pub_attrs);
	gck_attributes_unref (prv_attrs);
}

static void
test_wrap_key (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_WRAP, (guchar *)"wrap", 4 };
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GckObject *wrapper, *wrapped;
	gpointer output;
	gsize n_output;

	wrapper = find_key (test->session, CKA_WRAP, 0);
	wrapped = find_key_with_value (test->session, "value");

	/* Simple One */
	output = gck_session_wrap_key (test->session, wrapper, CKM_MOCK_WRAP, wrapped, &n_output, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
    g_assert_cmpmem (output, n_output, "value", 5);
	g_free (output);

	/* Full One*/
	output = gck_session_wrap_key_full (test->session, wrapper, &mech, wrapped, &n_output, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
    g_assert_cmpmem (output, n_output, "value", 5);
	g_free (output);

	/* Failure one */
	mech.type = 0;
	n_output = 0;
	output = gck_session_wrap_key_full (test->session, wrapper, &mech, wrapped, &n_output, NULL, &error);
	g_assert_nonnull (error);
	g_assert_null (output);
	g_clear_error (&error);
	egg_assert_cmpsize (n_output, ==, 0);

	/* Asynchronous one */
	mech.type = CKM_MOCK_WRAP;
	gck_session_wrap_key_async (test->session, wrapper, &mech, wrapped, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	output = gck_session_wrap_key_finish (test->session, result, &n_output, &error);
	g_assert_no_error (error);
	g_assert_nonnull (output);
    g_assert_cmpmem (output, n_output, "value", 5);
	g_object_unref (result);
	g_free (output);

	/* Asynchronous failure */
	result = NULL;
	mech.type = 0;
	n_output = 0;
	gck_session_wrap_key_async (test->session, wrapper, &mech, wrapped, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	output = gck_session_wrap_key_finish (test->session, result, &n_output, &error);
	g_assert_nonnull (error);
	g_assert_null (output);
	g_clear_error (&error);
	egg_assert_cmpsize (n_output, ==, 0);
	g_object_unref (result);

	g_object_unref (wrapper);
	g_object_unref (wrapped);
}

static void
test_unwrap_key (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_WRAP, (guchar *)"wrap", 4 };
	GckBuilder builder = GCK_BUILDER_INIT;
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GckObject *wrapper, *unwrapped;
	GckAttributes *attrs;

	wrapper = find_key (test->session, CKA_UNWRAP, 0);
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_SECRET_KEY);
	attrs = gck_builder_end (&builder);

	/* Full One*/
	unwrapped = gck_session_unwrap_key_full (test->session, wrapper, &mech, (const guchar *)"special", 7, attrs, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GCK_IS_OBJECT (unwrapped));
	check_key_with_value (test->session, unwrapped, CKO_SECRET_KEY, "special");
	g_object_unref (unwrapped);

	/* Failure one */
	mech.type = 0;
	unwrapped = gck_session_unwrap_key_full (test->session, wrapper, &mech, (const guchar *)"special", 7, attrs, NULL, &error);
	g_assert_nonnull (error);
	g_assert_null (unwrapped);
	g_clear_error (&error);

	/* Asynchronous one */
	mech.type = CKM_MOCK_WRAP;
	gck_session_unwrap_key_async (test->session, wrapper, &mech, (const guchar *)"special", 7, attrs, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	unwrapped = gck_session_unwrap_key_finish (test->session, result, &error);
	g_assert_no_error (error);
	g_assert_true (GCK_IS_OBJECT (unwrapped));
	check_key_with_value (test->session, unwrapped, CKO_SECRET_KEY, "special");
	g_object_unref (unwrapped);
	g_object_unref (result);

	/* Asynchronous failure */
	result = NULL;
	mech.type = 0;
	gck_session_unwrap_key_async (test->session, wrapper, &mech, (const guchar *)"special", 6, attrs, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	unwrapped = gck_session_unwrap_key_finish (test->session, result, &error);
	g_assert_nonnull (error);
	g_assert_null (unwrapped);
	g_clear_error (&error);
	g_object_unref (result);

	g_object_unref (wrapper);
	gck_attributes_unref (attrs);
}

static void
test_derive_key (Test *test, gconstpointer unused)
{
	GckMechanism mech = { CKM_MOCK_DERIVE, (guchar *)"derive", 6 };
	GckBuilder builder = GCK_BUILDER_INIT;
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GckObject *wrapper, *derived;
	GckAttributes *attrs;

	wrapper = find_key (test->session, CKA_DERIVE, 0);
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_SECRET_KEY);
	attrs = gck_builder_end (&builder);

	/* Full One*/
	derived = gck_session_derive_key_full (test->session, wrapper, &mech, attrs, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GCK_IS_OBJECT (derived));
	check_key_with_value (test->session, derived, CKO_SECRET_KEY, "derived");
	g_object_unref (derived);

	/* Failure one */
	mech.type = 0;
	derived = gck_session_derive_key_full (test->session, wrapper, &mech, attrs, NULL, &error);
	g_assert_nonnull (error);
	g_assert_null (derived);
	g_clear_error (&error);

	/* Asynchronous one */
	mech.type = CKM_MOCK_DERIVE;
	gck_session_derive_key_async (test->session, wrapper, &mech, attrs, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	derived = gck_session_derive_key_finish (test->session, result, &error);
	g_assert_no_error (error);
	g_assert_true (GCK_IS_OBJECT (derived));
	check_key_with_value (test->session, derived, CKO_SECRET_KEY, "derived");
	g_object_unref (derived);
	g_object_unref (result);

	/* Asynchronous failure */
	result = NULL;
	mech.type = 0;
	gck_session_derive_key_async (test->session, wrapper, &mech, attrs, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	derived = gck_session_derive_key_finish (test->session, result, &error);
	g_assert_nonnull (error);
	g_assert_null (derived);
	g_clear_error (&error);
	g_object_unref (result);

	g_object_unref (wrapper);
	gck_attributes_unref (attrs);
}

static void
null_log_handler (const gchar *log_domain, GLogLevelFlags log_level,
                  const gchar *message, gpointer user_data)
{

}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_set_prgname ("test-gck-crypto");

	/* Suppress these messages in tests */
	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG,
	                   null_log_handler, NULL);

	g_test_add ("/gck/crypto/encrypt", Test, NULL, setup, test_encrypt, teardown);
	g_test_add ("/gck/crypto/decrypt", Test, NULL, setup, test_decrypt, teardown);
	g_test_add ("/gck/crypto/login_context_specific", Test, NULL, setup, test_login_context_specific, teardown);
	g_test_add ("/gck/crypto/sign", Test, NULL, setup, test_sign, teardown);
	g_test_add ("/gck/crypto/verify", Test, NULL, setup, test_verify, teardown);
	g_test_add ("/gck/crypto/generate_key_pair", Test, NULL, setup, test_generate_key_pair, teardown);
	g_test_add ("/gck/crypto/wrap_key", Test, NULL, setup, test_wrap_key, teardown);
	g_test_add ("/gck/crypto/unwrap_key", Test, NULL, setup, test_unwrap_key, teardown);
	g_test_add ("/gck/crypto/derive_key", Test, NULL, setup, test_derive_key, teardown);

	return egg_tests_run_with_loop ();
}
