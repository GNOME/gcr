/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-gck-module.c - the GObject PKCS#11 wrapper library

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

#include <errno.h>
#include <glib.h>
#include <string.h>

#include "egg/egg-testing.h"

#include "gck/gck.h"
#include "gck/gck-test.h"

typedef struct {
	GckModule *module;
} Test;

static void
setup (Test *test, gconstpointer unused)
{
	GError *err = NULL;

	/* Successful load */
	test->module = gck_module_initialize (_GCK_TEST_MODULE_PATH, NULL, &err);
	g_assert_no_error (err);
	g_assert_nonnull (test->module);
	g_object_add_weak_pointer (G_OBJECT (test->module), (gpointer *)&test->module);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_object_unref (test->module);
	egg_test_wait_for_gtask_thread (test->module);
	g_assert_null (test->module);
}

static void
fetch_async_result (GObject *source, GAsyncResult *result, gpointer user_data)
{
	*((GAsyncResult**)user_data) = result;
	g_object_ref (result);
	egg_test_wait_stop ();
}

static void
test_initialize_async (void)
{
	GckModule *module;
	GAsyncResult *result;
	GError *error = NULL;

	/* Shouldn't be able to load modules */
	gck_module_initialize_async (_GCK_TEST_MODULE_PATH,
	                             NULL, fetch_async_result, &result);

	egg_test_wait_until (500);
	g_assert_nonnull (result);

	/* Get the result */
	module = gck_module_initialize_finish (result, &error);
	g_assert_no_error (error);
	g_assert_true (GCK_IS_MODULE (module));

	g_object_unref (result);
	g_object_unref (module);
}


static void
test_invalid_modules (Test *test, gconstpointer unused)
{
	GckModule *invalid;
	GError *error = NULL;

	/* Shouldn't be able to load modules */
	invalid = gck_module_initialize ("blah-blah-non-existant", NULL, &error);
	g_assert_error (error, GCK_ERROR, (int)GCK_ERROR_MODULE_PROBLEM);
	g_assert_null (invalid);

	g_clear_error (&error);

	/* Shouldn't be able to load any file successfully */
	invalid = gck_module_initialize ("/usr/lib/libm.so", NULL, &error);
	g_assert_error (error, GCK_ERROR, (int)GCK_ERROR_MODULE_PROBLEM);
	g_assert_null (invalid);

	g_clear_error (&error);
}

static void
test_module_equals_hash (Test *test, gconstpointer unused)
{
	GckModule *other;
	GObject *obj;
	guint hash;

	hash = gck_module_hash (test->module);
	g_assert_cmpuint (hash, !=, 0);

	g_assert_true (gck_module_equal (test->module, test->module));

	other = gck_module_new (gck_module_get_functions (test->module));
	obj = g_object_new (G_TYPE_OBJECT, NULL);

	g_assert_true (gck_module_equal (test->module, other));

	/* TODO: Could do with another test for inequality */
	g_assert_false (gck_module_equal (test->module, (GckModule *) obj));

	g_object_unref (other);
	g_object_unref (obj);
}

static void
test_module_props (Test *test, gconstpointer unused)
{
	gchar *path;

	g_object_get (test->module, "path", &path, NULL);
	g_assert_nonnull (path);
	g_assert_cmpstr (_GCK_TEST_MODULE_PATH, ==, path);
	g_free (path);
}

static void
test_module_info (Test *test, gconstpointer unused)
{
	GckModuleInfo *info;

	info = gck_module_get_info (test->module);
	g_assert_nonnull (info);

	g_assert_cmpuint (info->pkcs11_version_major, ==, CRYPTOKI_VERSION_MAJOR);
	g_assert_cmpuint (info->pkcs11_version_minor, ==, CRYPTOKI_VERSION_MINOR);
	g_assert_cmpstr ("TEST MANUFACTURER", ==, info->manufacturer_id);
	g_assert_cmpstr ("TEST LIBRARY", ==, info->library_description);
	g_assert_cmpuint (0, ==, info->flags);
	g_assert_cmpuint (45, ==, info->library_version_major);
	g_assert_cmpuint (145, ==, info->library_version_minor);

	gck_module_info_free (info);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/gck/module/initialize_async", test_initialize_async);
	g_test_add ("/gck/module/invalid_modules", Test, NULL, setup, test_invalid_modules, teardown);
	g_test_add ("/gck/module/module_equals_hash", Test, NULL, setup, test_module_equals_hash, teardown);
	g_test_add ("/gck/module/module_props", Test, NULL, setup, test_module_props, teardown);
	g_test_add ("/gck/module/module_info", Test, NULL, setup, test_module_info, teardown);

	return egg_tests_run_with_loop ();
}
