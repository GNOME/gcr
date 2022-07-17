/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-gck-modules.c - the GObject PKCS#11 wrapper library

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
#include "gck/gck-private.h"
#include "gck/gck-test.h"

#include "egg/egg-testing.h"

#include <glib.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	GList *modules;
} Test;

static void
setup (Test *test, gconstpointer unused)
{
	GckModule *module;
	GError *err = NULL;

	/* Successful load */
	module = gck_module_initialize (_GCK_TEST_MODULE_PATH, NULL, &err);
	g_assert_no_error (err);
	g_assert_true (GCK_IS_MODULE (module));

	test->modules = g_list_append (NULL, module);
}

static void
teardown (Test *test, gconstpointer unused)
{
	g_clear_list (&test->modules, g_object_unref);
}

static void
test_enumerate_objects (Test *test, gconstpointer unused)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GError *error = NULL;
	GckEnumerator *en;
	GckAttributes *attributes;
	GList *objects;

	gck_builder_add_string (&builder, CKA_LABEL, "Private Capitalize Key");
	attributes = gck_builder_end (&builder);
	en = gck_modules_enumerate_objects (test->modules, attributes, 0);
	gck_attributes_unref (attributes);
	g_assert_true (GCK_IS_ENUMERATOR (en));

	objects = gck_enumerator_next_n (en, -1, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (g_list_length (objects), ==, 1);
	g_assert_true (GCK_IS_OBJECT (objects->data));

	g_clear_list (&objects, g_object_unref);
	g_object_unref (en);
}


static void
test_token_for_uri (Test *test, gconstpointer unused)
{
	GckSlot *slot;
	GError *error = NULL;

	slot = gck_modules_token_for_uri (test->modules, "pkcs11:token=TEST%20LABEL", &error);
	g_assert_true (GCK_IS_SLOT (slot));

	g_object_unref (slot);
}

static void
test_token_for_uri_not_found (Test *test, gconstpointer unused)
{
	GckSlot *slot;
	GError *error = NULL;

	slot = gck_modules_token_for_uri (test->modules, "pkcs11:token=UNKNOWN", &error);
	g_assert_null (slot);
	g_assert_no_error (error);
}

static void
test_token_for_uri_error (Test *test, gconstpointer unused)
{
	GckSlot *slot;
	GError *error = NULL;

	slot = gck_modules_token_for_uri (test->modules, "http://invalid.uri", &error);
	g_assert_null (slot);
	g_assert_error (error, GCK_URI_ERROR, GCK_URI_BAD_SCHEME);
	g_error_free (error);
}

static void
test_object_for_uri (Test *test, gconstpointer unused)
{
	GckObject *object;
	GError *error = NULL;

	object = gck_modules_object_for_uri (test->modules, "pkcs11:object=Public%20Capitalize%20Key;objecttype=public", 0, &error);
	g_assert_true (GCK_IS_OBJECT (object));
	g_object_unref (object);
}

static void
test_object_for_uri_not_found (Test *test, gconstpointer unused)
{
	GckObject *object;
	GError *error = NULL;

	object = gck_modules_object_for_uri (test->modules, "pkcs11:object=Unknown%20Label", 0, &error);
	g_assert_null (object);
	g_assert_null (error);
}

static void
test_object_for_uri_error (Test *test, gconstpointer unused)
{
	GckObject *object;
	GError *error = NULL;

	object = gck_modules_object_for_uri (test->modules, "http://invalid.uri", 0, &error);
	g_assert_null (object);
	g_assert_error (error, GCK_URI_ERROR, GCK_URI_BAD_SCHEME);
	g_error_free (error);
}

static void
test_objects_for_uri (Test *test, gconstpointer unused)
{
	GList *objects;
	GError *error = NULL;

	objects = gck_modules_objects_for_uri (test->modules, "pkcs11:token=TEST%20LABEL", 0, &error);
	g_assert_nonnull (objects);
	g_assert_no_error (error);
	g_assert_cmpint (g_list_length (objects), ==, 5);

	g_clear_list (&objects, g_object_unref);
}

static void
test_enumerate_uri (Test *test, gconstpointer unused)
{
	GckEnumerator *en;
	GList *objects;
	GError *error = NULL;

	en = gck_modules_enumerate_uri (test->modules, "pkcs11:token=TEST%20LABEL", 0, &error);
	g_assert_true (GCK_IS_ENUMERATOR (en));
	g_assert_no_error (error);

	objects = gck_enumerator_next_n (en, -1, NULL, &error);
	g_assert_cmpint (g_list_length (objects), ==, 5);
	g_assert_no_error (error);

	g_clear_object (&en);
	g_clear_list (&objects, g_object_unref);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_add ("/gck/modules/enumerate_objects", Test, NULL, setup, test_enumerate_objects, teardown);
	g_test_add ("/gck/modules/token_for_uri", Test, NULL, setup, test_token_for_uri, teardown);
	g_test_add ("/gck/modules/token_for_uri_not_found", Test, NULL, setup, test_token_for_uri_not_found, teardown);
	g_test_add ("/gck/modules/token_for_uri_error", Test, NULL, setup, test_token_for_uri_error, teardown);
	g_test_add ("/gck/modules/object_for_uri", Test, NULL, setup, test_object_for_uri, teardown);
	g_test_add ("/gck/modules/object_for_uri_not_found", Test, NULL, setup, test_object_for_uri_not_found, teardown);
	g_test_add ("/gck/modules/object_for_uri_error", Test, NULL, setup, test_object_for_uri_error, teardown);
	g_test_add ("/gck/modules/objects_for_uri", Test, NULL, setup, test_objects_for_uri, teardown);
	g_test_add ("/gck/modules/enumerate_uri", Test, NULL, setup, test_enumerate_uri, teardown);

	return egg_tests_run_with_loop ();
}
