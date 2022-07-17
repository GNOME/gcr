/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-gck-session.c - the GObject PKCS#11 wrapper library

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
	GckSlot *slot;
	GckSession *session;
} Test;

static void
setup (Test *test, gconstpointer unused)
{
	GError *err = NULL;
	GList *slots;

	/* Successful load */
	test->module = gck_module_initialize (_GCK_TEST_MODULE_PATH, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_MODULE (test->module));
	g_object_add_weak_pointer (G_OBJECT (test->module), (gpointer *)&test->module);

	slots = gck_module_get_slots (test->module, TRUE);
	g_assert_nonnull (slots);

	test->slot = GCK_SLOT (slots->data);
	g_object_ref (test->slot);
	g_clear_list (&slots, g_object_unref);
	g_object_add_weak_pointer (G_OBJECT (test->slot), (gpointer *)&test->slot);

	test->session = gck_slot_open_session (test->slot, 0, NULL, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (test->session));
	g_object_add_weak_pointer (G_OBJECT (test->session), (gpointer *)&test->session);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_object_unref (test->session);
	g_object_unref (test->slot);
	g_object_unref (test->module);

	egg_test_wait_for_gtask_thread (test->session || test->slot || test->module);

	g_assert_null (test->session);
	g_assert_null (test->slot);
	g_assert_null (test->module);
}

static void
test_session_props (Test *test, gconstpointer unused)
{
	GckModule *mod;
	GckSlot *sl;
	gulong handle;

	g_object_get (test->session, "module", &mod, "handle", &handle, "slot", &sl, NULL);
	g_assert_true (mod == test->module);
	g_assert_true (sl == test->slot);
	g_object_unref (mod);
	g_object_unref (sl);

	g_assert_cmpuint (handle, !=, 0);
	g_assert_cmpuint (gck_session_get_handle (test->session), ==, handle);
}

static void
test_session_info (Test *test, gconstpointer unused)
{
	GckSessionInfo *info;

	info = gck_session_get_info (test->session);
	g_assert_nonnull (info);

	g_assert_cmpuint (info->slot_id, ==, gck_slot_get_handle (test->slot));
	g_assert_cmphex ((info->flags & CKF_SERIAL_SESSION), ==, CKF_SERIAL_SESSION);
	g_assert_cmpuint (info->device_error, ==, 1414);
	gck_session_info_free (info);
}

static void
fetch_async_result (GObject *source, GAsyncResult *result, gpointer user_data)
{
	*((GAsyncResult**)user_data) = result;
	g_object_ref (result);
	egg_test_wait_stop ();
}

static void
test_open_close_session (Test *test, gconstpointer unused)
{
	GckSession *sess;
	GAsyncResult *result = NULL;
	GError *err = NULL;

	sess = gck_slot_open_session (test->slot, 0, NULL, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (sess));

	g_object_unref (sess);

	/* Test opening async */
	gck_slot_open_session_async (test->slot, 0, NULL, NULL, fetch_async_result, &result);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	g_object_add_weak_pointer (G_OBJECT (result), (gpointer *)&result);
	sess = gck_slot_open_session_finish (test->slot, result, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (sess));
	g_object_add_weak_pointer (G_OBJECT (sess), (gpointer *)&sess);

	g_object_unref (result);
	g_assert_null (result);

	g_object_unref (sess);
	egg_test_wait_for_gtask_thread (sess);
	g_assert_null (sess);
}

static void
test_session_initable (Test *test,
                       gconstpointer unused)
{
	GckSession *sess;
	GAsyncResult *result = NULL;
	GError *err = NULL;

	sess = gck_session_open (test->slot, 0, NULL, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (sess));

	g_object_unref (sess);

	/* Test opening async */
	gck_session_open_async (test->slot, 0, NULL, NULL, fetch_async_result, &result);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	sess = gck_session_open_finish (result, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (sess));

	g_object_unref (result);
	g_object_unref (sess);
}

static void
test_session_already (Test *test,
                      gconstpointer unused)
{
	CK_FUNCTION_LIST_PTR funcs;
	gulong handle;
	GckSession *session;
	GAsyncResult *result = NULL;
	GError *error = NULL;
	GObject *source;
	CK_RV rv;

	funcs = gck_module_get_functions (test->module);
	g_assert_nonnull (funcs);

	rv = funcs->C_OpenSession (gck_slot_get_handle (test->slot), CKF_SERIAL_SESSION,
	                           NULL, NULL, &handle);
	gck_assert_cmprv (rv, ==, CKR_OK);
	g_assert_cmpuint (handle, !=, 0);

	session = g_initable_new (GCK_TYPE_SESSION, NULL, &error,
	                          "slot", test->slot,
	                          "handle", handle,
	                          NULL);

	g_assert_no_error (error);
	g_assert_true (GCK_IS_SESSION (session));
	gck_assert_cmpulong (handle, ==, gck_session_get_handle (session));
	g_object_unref (session);

	rv = funcs->C_OpenSession (gck_slot_get_handle (test->slot), CKF_SERIAL_SESSION,
	                           NULL, NULL, &handle);
	gck_assert_cmprv (rv, ==, CKR_OK);
	g_assert_cmpuint (handle, !=, 0);

	/* Test opening async */
	g_async_initable_new_async (GCK_TYPE_SESSION, G_PRIORITY_DEFAULT, NULL,
	                            fetch_async_result, &result,
	                            "slot", test->slot,
	                            "handle", handle,
	                            NULL);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	source = g_async_result_get_source_object (result);
	session = GCK_SESSION (g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, &error));
	g_object_unref (source);
	g_assert_no_error (error);
	g_assert_true (GCK_IS_SESSION (session));
	gck_assert_cmpulong (handle, ==, gck_session_get_handle (session));

	g_object_unref (result);
	g_object_unref (session);
}

static void
test_open_interaction (Test *test,
                       gconstpointer unused)
{
	GckSession *sess;
	GAsyncResult *result = NULL;
	GError *err = NULL;
	GTlsInteraction *interaction;

	interaction = mock_interaction_new ("booo");

	sess = gck_session_open (test->slot, GCK_SESSION_LOGIN_USER, interaction, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (sess));

	g_object_unref (sess);

	/* Test opening async */
	gck_session_open_async (test->slot, GCK_SESSION_LOGIN_USER, interaction, NULL, fetch_async_result, &result);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	sess = gck_session_open_finish (result, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (sess));

	g_object_unref (interaction);
	g_object_unref (result);
	g_object_unref (sess);

}

static void
test_init_set_pin (Test *test, gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GError *err = NULL;
	gboolean ret;

	/* init pin */
	ret = gck_session_init_pin (test->session, (guchar*)"booo", 4, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	/* set pin */
	ret = gck_session_set_pin (test->session, (guchar*)"booo", 4, (guchar*)"tooo", 4, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	/* init pin async */
	gck_session_init_pin_async (test->session, (guchar*)"booo", 4, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	ret = gck_session_init_pin_finish (test->session, result, &err);
	g_assert_no_error (err);
	g_assert_true (ret);
	g_object_unref (result);
	result = NULL;

	/* set pin async */
	gck_session_set_pin_async (test->session, (guchar*)"booo", 4, (guchar*)"tooo", 4, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	ret = gck_session_set_pin_finish (test->session, result, &err);
	g_assert_no_error (err);
	g_assert_true (ret);
	g_object_unref (result);
	result = NULL;
}

static void
test_login_logout (Test *test, gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GError *err = NULL;
	gboolean ret;
	gulong state;

	/* login/logout */
	ret = gck_session_login (test->session, CKU_USER, (guchar*)"booo", 4, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_USER_FUNCTIONS);

	ret = gck_session_logout (test->session, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_PUBLIC_SESSION);

	/* login async */
	gck_session_login_async (test->session, CKU_USER, (guchar*)"booo", 4, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);

	ret = gck_session_login_finish (test->session, result, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	g_object_unref (result);
	result = NULL;

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_USER_FUNCTIONS);

	/* logout async */
	gck_session_logout_async (test->session, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);

	ret = gck_session_logout_finish (test->session, result, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_PUBLIC_SESSION);

	g_object_unref (result);
	result = NULL;
}

static void
test_login_interactive (Test *test,
                        gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GError *error = NULL;
	gboolean ret;
	gulong state;
	GTlsInteraction *interaction;

	interaction = mock_interaction_new ("booo");

	/* login/logout */
	ret = gck_session_login_interactive (test->session, CKU_USER, interaction, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_USER_FUNCTIONS);

	ret = gck_session_logout (test->session, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_PUBLIC_SESSION);

	/* login async */
	gck_session_login_interactive_async (test->session, CKU_USER, interaction, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);

	ret = gck_session_login_interactive_finish (test->session, result, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	g_object_unref (result);
	result = NULL;

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_USER_FUNCTIONS);

	ret = gck_session_logout (test->session, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	state = gck_session_get_state (test->session);
	gck_assert_cmpulong (state, ==, CKS_RO_PUBLIC_SESSION);

	g_object_unref (interaction);
}

static void
test_auto_login (Test *test, gconstpointer unused)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckObject *object;
	GckSession *new_session;
	GAsyncResult *result = NULL;
	GError *err = NULL;
	GckAttributes *attrs;
	GTlsInteraction *test_interaction;
	gboolean ret;

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_DATA);
	gck_builder_add_string (&builder, CKA_LABEL, "TEST OBJECT");
	gck_builder_add_boolean (&builder, CKA_PRIVATE, CK_TRUE);
	attrs = gck_builder_end (&builder);

	/* Try to do something that requires a login */
	object = gck_session_create_object (test->session, attrs, NULL, &err);
	g_assert_null (object);
	g_assert_true (err && err->code == CKR_USER_NOT_LOGGED_IN);
	g_clear_error (&err);

	/* Setup for auto login */
	test_interaction = mock_interaction_new ("booo");
	new_session = gck_slot_open_session (test->slot, GCK_SESSION_READ_WRITE | GCK_SESSION_LOGIN_USER, test_interaction, NULL, &err);
	g_object_unref (test_interaction);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (new_session));

	/* Try again to do something that requires a login */
	object = gck_session_create_object (new_session, attrs, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_OBJECT (object));
	g_object_unref (object);

	/* We should now be logged in, try to log out */
	ret = gck_session_logout (new_session, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	g_object_unref (new_session);

	/* Now try the same thing, but asyncronously */
	test_interaction = mock_interaction_new ("booo");
	gck_slot_open_session_async (test->slot, GCK_SESSION_READ_WRITE | GCK_SESSION_LOGIN_USER, test_interaction, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	new_session = gck_slot_open_session_finish (test->slot, result, &err);
	g_object_unref (test_interaction);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_SESSION (new_session));
	g_object_unref (result);

	result = NULL;
	gck_session_create_object_async (new_session, attrs, NULL, fetch_async_result, &result);
	egg_test_wait_until (500);
	g_assert_nonnull (result);
	object = gck_session_create_object_finish (new_session, result, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_OBJECT (object));
	g_object_unref (result);
	g_object_unref (object);

	/* We should now be logged in, try to log out */
	ret = gck_session_logout (new_session, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (ret);

	gck_attributes_unref (attrs);
	g_object_unref (new_session);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_set_prgname ("test-gck-session");

	g_test_add ("/gck/session/session_props", Test, NULL, setup, test_session_props, teardown);
	g_test_add ("/gck/session/session_info", Test, NULL, setup, test_session_info, teardown);
	g_test_add ("/gck/session/open_close_session", Test, NULL, setup, test_open_close_session, teardown);
	g_test_add ("/gck/session/open_initable", Test, NULL, setup, test_session_initable, teardown);
	g_test_add ("/gck/session/open_already", Test, NULL, setup, test_session_already, teardown);
	g_test_add ("/gck/session/open_interaction", Test, NULL, setup, test_open_interaction, teardown);
	g_test_add ("/gck/session/init_set_pin", Test, NULL, setup, test_init_set_pin, teardown);
	g_test_add ("/gck/session/login_logout", Test, NULL, setup, test_login_logout, teardown);
	g_test_add ("/gck/session/login_interactive", Test, NULL, setup, test_login_interactive, teardown);
	g_test_add ("/gck/session/auto_login", Test, NULL, setup, test_auto_login, teardown);

	return egg_tests_run_with_loop ();
}
