/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
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

#include "gcr/gcr-simple-collection.h"
#include "gcr/gcr-filter-collection.h"

#include "egg/egg-testing.h"

#include <glib.h>

#define NUM_OBJECTS 10

typedef struct {
	GcrCollection *underlying;
	GObject *objects[NUM_OBJECTS];
} Test;

static guint
mock_object_value (GObject *object)
{
	return GPOINTER_TO_UINT (g_object_get_data (object, "value"));
}

static void
setup (Test *test,
       gconstpointer unused)
{
	GcrSimpleCollection *collection;
	guint i;

	test->underlying = gcr_simple_collection_new ();
	collection = GCR_SIMPLE_COLLECTION (test->underlying);

	for (i = 0; i < NUM_OBJECTS; i++) {
		test->objects[i] = g_object_new (G_TYPE_OBJECT, NULL);
		g_object_set_data (test->objects[i], "value", GUINT_TO_POINTER (i));
		gcr_simple_collection_add (GCR_SIMPLE_COLLECTION (collection), test->objects[i]);
	}
}

static void
teardown (Test *test,
          gconstpointer unused)
{
	guint i;

	for (i = 0; i < NUM_OBJECTS; i++)
		g_object_unref (test->objects[i]);

	g_object_unref (test->underlying);
}

static void
test_create (Test *test,
             gconstpointer unused)
{
	GcrFilterCollection *filter;
	GcrCollection *collection;
	GcrCollection *underlying;
	GList *objects;
	guint i;

	collection = gcr_filter_collection_new_with_callback (test->underlying,
	                                                      NULL, NULL, NULL);
	filter = GCR_FILTER_COLLECTION (collection);

	g_assert (test->underlying == gcr_filter_collection_get_underlying (filter));
	g_object_get (collection, "underlying", &underlying, NULL);
	g_assert (test->underlying == underlying);
	g_object_unref (underlying);

	g_assert_cmpuint (gcr_collection_get_length (collection), ==, NUM_OBJECTS);
	for (i = 0; i < NUM_OBJECTS; i++)
		gcr_collection_contains (collection, test->objects[i]);
	objects = gcr_collection_get_objects (collection);
	g_assert_cmpuint (g_list_length (objects), ==, NUM_OBJECTS);
	for (i = 0; i < NUM_OBJECTS; i++)
		g_assert (g_list_find (objects, test->objects[i]) != NULL);
	g_list_free (objects);

	g_object_unref (collection);
}

static gboolean
on_filter_increment_value (GObject *object,
                           gpointer user_data)
{
	guint *value = user_data;

	g_assert_cmpuint (*value, >=, 0);
	g_assert_cmpuint (*value, <=, 20);
	(*value)++;

	return TRUE;
}

static void
destroy_change_value (gpointer data)
{
	guint *value = data;
	g_assert (value != NULL);
	*value = 0;
}

static void
test_callbacks (Test *test,
                gconstpointer unused)
{
	GcrCollection *collection;
	GcrFilterCollection *filter;
	guint value = 4;

	collection = gcr_filter_collection_new_with_callback (test->underlying,
	                                                      on_filter_increment_value,
	                                                      &value, destroy_change_value);
	g_assert_cmpuint (value, ==, 4 + NUM_OBJECTS);

	filter = GCR_FILTER_COLLECTION (collection);

	/* This should call destroy (value -> 0), and then refilter all values (value -> 10) */
	gcr_filter_collection_set_callback (filter, on_filter_increment_value,
	                                    &value, destroy_change_value);
	g_assert_cmpuint (value, ==, NUM_OBJECTS);

	g_object_unref (collection);
	g_assert_cmpuint (value, ==, 0);
}

static gboolean
on_filter_modulo (GObject *object,
                  gpointer user_data)
{
	guint value = mock_object_value (object);
	guint *modulo = user_data;

	g_assert (modulo != NULL);
	g_assert_cmpuint (value, >=, 0);
	g_assert_cmpuint (value, <, NUM_OBJECTS);

	return (value % *modulo) == 0;
}

static void
test_filtering (Test *test,
                gconstpointer unused)
{
	GcrFilterCollection *filter;
	GcrCollection *collection;
	GList *objects;
	guint modulo;
	guint i;

	modulo = 2;
	collection = gcr_filter_collection_new_with_callback (test->underlying,
	                                                      on_filter_modulo, &modulo, NULL);
	filter = GCR_FILTER_COLLECTION (collection);

	g_assert_cmpuint (gcr_collection_get_length (collection), ==, NUM_OBJECTS / modulo);
	for (i = 0; i < NUM_OBJECTS; i += modulo)
		gcr_collection_contains (collection, test->objects[i]);
	objects = gcr_collection_get_objects (collection);
	g_assert_cmpuint (g_list_length (objects), ==, NUM_OBJECTS / modulo);
	for (i = 0; i < NUM_OBJECTS; i += modulo)
		g_assert (g_list_find (objects, test->objects[i]) != NULL);
	g_list_free (objects);

	modulo = 5;
	gcr_filter_collection_refilter (filter);

	g_assert_cmpuint (gcr_collection_get_length (collection), ==, NUM_OBJECTS / modulo);
	for (i = 0; i < NUM_OBJECTS; i += modulo)
		gcr_collection_contains (collection, test->objects[i]);
	objects = gcr_collection_get_objects (collection);
	g_assert_cmpuint (g_list_length (objects), ==, NUM_OBJECTS / modulo);
	for (i = 0; i < NUM_OBJECTS; i += modulo)
		g_assert (g_list_find (objects, test->objects[i]) != NULL);
	g_list_free (objects);

	g_object_unref (collection);
}

static void
on_filter_added (GcrCollection *collection,
                 GObject *object,
                 gpointer user_data)
{
	guint *added = user_data;
	g_assert (added != NULL);
	(*added)++;
}

static void
on_filter_removed (GcrCollection *collection,
                   GObject *object,
                   gpointer user_data)
{
	guint *removed = user_data;
	g_assert (removed != NULL);
	(*removed)++;
}

static void
test_add_remove (Test *test,
                 gconstpointer unused)
{
	GcrCollection *collection;
	guint modulo;
	guint added = 0;
	guint removed = 0;
	guint i;

	modulo = 2;
	collection = gcr_filter_collection_new_with_callback (test->underlying,
	                                                      on_filter_modulo, &modulo, NULL);

	g_assert_cmpuint (gcr_collection_get_length (collection), ==, NUM_OBJECTS / modulo);

	g_signal_connect (collection, "added", G_CALLBACK (on_filter_added), &added);
	g_signal_connect (collection, "removed", G_CALLBACK (on_filter_removed), &removed);

	for (i = 0; i < NUM_OBJECTS; i++)
		gcr_simple_collection_remove (GCR_SIMPLE_COLLECTION (test->underlying),
		                              test->objects[i]);

	g_assert_cmpuint (gcr_collection_get_length (collection), ==, 0);
	g_assert_cmpuint (added, ==, 0);
	g_assert_cmpuint (removed, ==, NUM_OBJECTS / modulo);

	for (i = 0; i < NUM_OBJECTS; i++)
		gcr_simple_collection_add (GCR_SIMPLE_COLLECTION (test->underlying),
		                           test->objects[i]);

	g_assert_cmpuint (gcr_collection_get_length (collection), ==, NUM_OBJECTS / modulo);
	g_assert_cmpuint (added, ==, NUM_OBJECTS / modulo);
	g_assert_cmpuint (removed, ==, NUM_OBJECTS / modulo);

	g_object_unref (collection);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-filter-collection");

	g_test_add ("/gcr/filter-collection/create", Test, NULL, setup, test_create, teardown);
	g_test_add ("/gcr/filter-collection/callbacks", Test, NULL, setup, test_callbacks, teardown);
	g_test_add ("/gcr/filter-collection/filtering", Test, NULL, setup, test_filtering, teardown);
	g_test_add ("/gcr/filter-collection/add-remove", Test, NULL, setup, test_add_remove, teardown);

	return g_test_run ();
}
