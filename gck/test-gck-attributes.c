/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-gck-attributes.c - the GObject PKCS#11 wrapper library

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

#include <glib.h>
#include <string.h>

#include "egg/egg-secure-memory.h"

#include "gck/gck.h"
#include "gck/gck-test.h"

EGG_SECURE_DECLARE (test_gck_attributes);

#define ATTR_TYPE 55
#define ATTR_DATA (const guchar *)"TEST DATA"
#define N_ATTR_DATA ((gsize)9)

static void
test_init_memory (void)
{
	GckAttribute attr;

	g_assert_cmpuint (sizeof (attr), ==, sizeof (CK_ATTRIBUTE));

	gck_attribute_init (&attr, ATTR_TYPE, (const guchar *)ATTR_DATA, N_ATTR_DATA);
	g_assert_cmpuint (attr.type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr.value, attr.length, ATTR_DATA, N_ATTR_DATA);

	gck_attribute_clear (&attr);
}

static void
test_init_boolean (void)
{
	GckAttribute attr;
	CK_BBOOL ck_value = CK_FALSE;

	gck_attribute_init_boolean (&attr, ATTR_TYPE, TRUE);
	g_assert_cmpuint (attr.type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr.length, ==, sizeof (CK_BBOOL));
	memcpy(&ck_value, attr.value, sizeof (CK_BBOOL));
	g_assert_cmpint (ck_value, ==, CK_TRUE);

	gck_attribute_clear (&attr);
}

static void
test_init_date (void)
{
	GckAttribute attr;
	CK_DATE ck_date;
	GDate *date;

	date = g_date_new_dmy(05, 06, 1960);
	memcpy (ck_date.year, "1960", 4);
	memcpy (ck_date.month, "06", 2);
	memcpy (ck_date.day, "05", 2);
	gck_attribute_init_date (&attr, ATTR_TYPE, date);
	g_date_free (date);
	g_assert_cmpuint (attr.type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr.value, attr.length, &ck_date, sizeof (CK_DATE));

	gck_attribute_clear (&attr);
}

static void
test_init_ulong (void)
{
	GckAttribute attr;
	CK_ULONG ck_value = 0;

	gck_attribute_init_ulong (&attr, ATTR_TYPE, 88);
	g_assert_cmpuint (attr.type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr.length, ==, sizeof (CK_ULONG));
	memcpy(&ck_value, attr.value, sizeof (CK_ULONG));
	g_assert_cmpuint (ck_value, ==, 88);

	gck_attribute_clear (&attr);
}

static void
test_init_string (void)
{
	GckAttribute attr;

	gck_attribute_init_string (&attr, ATTR_TYPE, "a test string");
	g_assert_cmpuint (attr.type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr.value, attr.length, "a test string", strlen ("a test string"));

	gck_attribute_clear (&attr);
}

static void
test_init_invalid (void)
{
	GckAttribute attr;

	gck_attribute_init_invalid (&attr, ATTR_TYPE);
	g_assert_cmpuint (attr.type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr.length, ==, (gulong)-1);
	g_assert_null (attr.value);

	g_assert_true (gck_attribute_is_invalid (&attr));
	gck_attribute_clear (&attr);
}

static void
test_init_empty (void)
{
	GckAttribute attr;

	gck_attribute_init_empty (&attr, ATTR_TYPE);
	g_assert_cmpuint (attr.type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr.length, ==, 0);
	g_assert_null (attr.value);

	gck_attribute_clear (&attr);
}

static void
test_new_memory (void)
{
	GckAttribute *attr;

	attr = gck_attribute_new (ATTR_TYPE, ATTR_DATA, N_ATTR_DATA);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, ATTR_DATA, N_ATTR_DATA);

	gck_attribute_free (attr);
}

static void
test_new_boolean (void)
{
	GckAttribute *attr;
	CK_BBOOL ck_value = CK_FALSE;

	attr = gck_attribute_new_boolean (ATTR_TYPE, TRUE);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_BBOOL));
	memcpy(&ck_value, attr->value, sizeof (CK_BBOOL));
	g_assert_cmpuint (ck_value, ==, CK_TRUE);

	gck_attribute_free (attr);
}

static void
test_new_date (void)
{
	GckAttribute *attr;
	CK_DATE ck_date;
	GDate *date;

	date = g_date_new_dmy(05, 06, 1800);
	memcpy (ck_date.year, "1800", 4);
	memcpy (ck_date.month, "06", 2);
	memcpy (ck_date.day, "05", 2);
	attr = gck_attribute_new_date (ATTR_TYPE, date);
	g_date_free (date);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, &ck_date, sizeof (CK_DATE));

	gck_attribute_free (attr);
}

static void
test_new_ulong (void)
{
	GckAttribute *attr;
	CK_ULONG ck_value = 0;

	attr = gck_attribute_new_ulong (ATTR_TYPE, 88);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_ULONG));
	memcpy(&ck_value, attr->value, sizeof (CK_ULONG));
	g_assert_cmpuint (ck_value, ==, 88);

	gck_attribute_free (attr);
}


static void
test_new_string (void)
{
	GckAttribute *attr;

	attr = gck_attribute_new_string (ATTR_TYPE, "a test string");
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, "a test string", strlen ("a test string"));

	gck_attribute_free (attr);
}

static void
test_new_invalid (void)
{
	GckAttribute *attr;

	attr = gck_attribute_new_invalid (ATTR_TYPE);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, (gulong)-1);
	g_assert_null (attr->value);

	g_assert_true (gck_attribute_is_invalid (attr));

	gck_attribute_free (attr);
}

static void
test_new_empty (void)
{
	GckAttribute *attr;

	attr = gck_attribute_new_empty (ATTR_TYPE);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, 0);
	g_assert_null (attr->value);

	gck_attribute_free (attr);
}

static void
test_get_boolean (void)
{
	GckAttribute *attr;

	attr = gck_attribute_new_boolean (ATTR_TYPE, TRUE);
	g_assert_true (gck_attribute_get_boolean (attr));
	gck_attribute_free (attr);
}

static void
test_get_date (void)
{
	GckAttribute *attr;
	CK_DATE ck_date;
	GDate date, date2;

	g_date_set_dmy(&date, 05, 06, 1800);
	memcpy (ck_date.year, "1800", 4);
	memcpy (ck_date.month, "06", 2);
	memcpy (ck_date.day, "05", 2);
	attr = gck_attribute_new_date (ATTR_TYPE, &date);
	gck_attribute_get_date (attr, &date2);
	g_assert_true (g_date_compare (&date, &date2) == 0);
	gck_attribute_free (attr);
}

static void
test_get_ulong (void)
{
	GckAttribute *attr;

	attr = gck_attribute_new_ulong (ATTR_TYPE, 88);
	g_assert_cmpuint (gck_attribute_get_ulong (attr), ==, 88);
	gck_attribute_free (attr);
}

static void
test_get_string (void)
{
	GckAttribute *attr;
	gchar *value;

	attr = gck_attribute_new_string (ATTR_TYPE, "a test string");
	value = gck_attribute_get_string (attr);
	g_assert_cmpstr ("a test string", ==, value);
	g_free (value);
	gck_attribute_free (attr);

	/* Should be able to store null strings */
	attr = gck_attribute_new_string (ATTR_TYPE, NULL);
	value = gck_attribute_get_string (attr);
	g_assert_null (value);
	gck_attribute_free (attr);
}

static void
test_dup_attribute (void)
{
	GckAttribute attr, *dup;

	gck_attribute_init_ulong (&attr, ATTR_TYPE, 88);
	dup = gck_attribute_dup (&attr);
	gck_attribute_clear (&attr);
	g_assert_cmpuint (gck_attribute_get_ulong (dup), ==, 88);
	g_assert_cmpuint (dup->type, ==, ATTR_TYPE);
	gck_attribute_free (dup);

	/* Should be able to dup null */
	dup = gck_attribute_dup (NULL);
	g_assert_null (dup);
}

static void
test_copy_attribute (void)
{
	GckAttribute attr, copy;

	gck_attribute_init_ulong (&attr, ATTR_TYPE, 88);
	gck_attribute_init_copy (&copy, &attr);
	gck_attribute_clear (&attr);
	g_assert_cmpuint (gck_attribute_get_ulong (&copy), ==, 88);
	g_assert_cmpuint (copy.type, ==, ATTR_TYPE);
	gck_attribute_clear (&copy);
}

static void
builder_add_fixtures (GckBuilder *builder,
                      guint seed)
{
	GDate *date = g_date_new_dmy (11 + seed, 12, 2008);
	gck_builder_add_boolean (builder, 0UL, (TRUE + seed) % 2);
	gck_builder_add_ulong (builder, 101UL, 888 + seed);
	gck_builder_add_string (builder, 202UL, "string");
	gck_builder_add_date (builder, 303UL, date);
	g_date_free (date);
	gck_builder_add_data (builder, 404UL, (const guchar *)ATTR_DATA, N_ATTR_DATA);
	gck_builder_add_invalid (builder, 505UL);
	gck_builder_add_empty (builder, 606UL);
}

static void
test_builder_blank (void)
{
	GckBuilder builder;

	gck_builder_init (&builder);
	g_assert_null (gck_builder_find (&builder, 88));
	gck_builder_clear (&builder);
}

static void
test_build_data (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;

	gck_builder_add_data (&builder, ATTR_TYPE, (const guchar *)"Hello", 5);
	attr = gck_builder_find (&builder, ATTR_TYPE);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, "Hello", 5);

	gck_builder_set_data (&builder, ATTR_TYPE, (const guchar *)ATTR_DATA, N_ATTR_DATA);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, ATTR_DATA, N_ATTR_DATA);

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, ATTR_DATA, N_ATTR_DATA);

	gck_attributes_unref (attrs);
}

static void
test_build_data_invalid (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;

	gck_builder_add_data (&builder, ATTR_TYPE, NULL, GCK_INVALID);
	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);

	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_true (gck_attribute_is_invalid (attr));

	gck_attributes_unref (attrs);
}

static void
test_build_data_secure (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	guchar *memory;

	memory = egg_secure_strdup ("password");
	gck_builder_add_data (&builder, ATTR_TYPE, memory, 8);
	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);

	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, "password", 8);
	g_assert_true (egg_secure_check (attr->value));

	egg_secure_free (memory);
	gck_attributes_unref (attrs);
}

static void
test_build_take (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	guchar *memory;

	memory = g_memdup2 (ATTR_DATA, N_ATTR_DATA);
	gck_builder_take_data (&builder, ATTR_TYPE, memory, N_ATTR_DATA);
	attrs = gck_builder_end (&builder);

	attr = gck_attributes_at (attrs, 0);

	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, ATTR_DATA, N_ATTR_DATA);

	gck_attributes_unref (attrs);
}

static void
test_build_take_invalid (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	gpointer memory;

	/* This memory should be freed */
	memory = g_strdup ("BLAH");
	gck_builder_take_data (&builder, ATTR_TYPE, memory, GCK_INVALID);

	/* This memory should be freed */
	memory = egg_secure_strdup ("BLAH");
	gck_builder_take_data (&builder, ATTR_TYPE, memory, GCK_INVALID);

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);

	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_true (gck_attribute_is_invalid (attr));

	gck_attributes_unref (attrs);
}

static void
test_build_take_secure (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	guchar *memory;

	memory = egg_secure_strdup ("password");
	gck_builder_take_data (&builder, ATTR_TYPE, memory, 8);
	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);

	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, "password", 8);
	g_assert_true (egg_secure_check (attr->value));

	gck_attributes_unref (attrs);
}


static void
test_value_to_boolean (void)
{
	CK_BBOOL data = CK_TRUE;
	gboolean result = FALSE;

	if (!gck_value_to_boolean (&data, sizeof (data), &result))
		g_assert_not_reached ();

	g_assert_true (result);

	if (!gck_value_to_boolean (&data, sizeof (data), NULL))
		g_assert_not_reached ();

	/* Should fail */
	if (gck_value_to_boolean (&data, 0, NULL))
		g_assert_not_reached ();
	if (gck_value_to_boolean (&data, 2, NULL))
		g_assert_not_reached ();
	if (gck_value_to_boolean (&data, (CK_ULONG)-1, NULL))
		g_assert_not_reached ();
}

static void
test_value_to_ulong (void)
{
	CK_ULONG data = 34343;
	gulong result = 0;

	if (!gck_value_to_ulong ((const guchar *)&data, sizeof (data), &result))
		g_assert_not_reached ();

	g_assert_cmpuint (result, ==, 34343);

	if (!gck_value_to_ulong ((const guchar *)&data, sizeof (data), NULL))
		g_assert_not_reached ();

	/* Should fail */
	if (gck_value_to_ulong ((const guchar *)&data, 0, NULL))
		g_assert_not_reached ();
	if (gck_value_to_ulong ((const guchar *)&data, 2, NULL))
		g_assert_not_reached ();
	if (gck_value_to_ulong ((const guchar *)&data, (CK_ULONG)-1, NULL))
		g_assert_not_reached ();
}

static void
test_build_boolean (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	gboolean value;
	CK_BBOOL ck_value = CK_FALSE;

	g_assert_false (gck_builder_find_boolean (&builder, 5, &value));

	gck_builder_add_boolean (&builder, ATTR_TYPE, FALSE);

	gck_builder_set_invalid (&builder, 5);
	g_assert_false (gck_builder_find_boolean (&builder, 5, &value));
	gck_builder_set_boolean (&builder, 5, TRUE);

	attr = gck_builder_find (&builder, ATTR_TYPE);
	g_assert_nonnull (attr);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_BBOOL));
	memcpy(&ck_value, attr->value, sizeof (CK_BBOOL));
	g_assert_cmpuint (ck_value, ==, CK_FALSE);
	if (!gck_builder_find_boolean (&builder, ATTR_TYPE, &value))
		g_assert_not_reached ();
	g_assert_false (value);

	gck_builder_set_boolean (&builder, ATTR_TYPE, TRUE);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_BBOOL));
	memcpy(&ck_value, attr->value, sizeof (CK_BBOOL));
	g_assert_cmpuint (ck_value, ==, CK_TRUE);
	if (!gck_builder_find_boolean (&builder, ATTR_TYPE, &value))
		g_assert_not_reached ();
	g_assert_true (value);

	if (!gck_builder_find_boolean (&builder, 5, &value))
		g_assert_not_reached ();
	g_assert_true (value);

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_nonnull (attr);

	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_BBOOL));
	memcpy(&ck_value, attr->value, sizeof (CK_BBOOL));
	g_assert_cmpuint (ck_value, ==, CK_TRUE);

	if (!gck_attributes_find_boolean (attrs, ATTR_TYPE, &value))
		g_assert_not_reached ();
	g_assert_true (value);

	g_assert_true (gck_attribute_get_boolean (attr));

	g_assert_cmpuint (gck_attributes_count (attrs), ==, 2);
	gck_attributes_unref (attrs);
}

static void
test_build_date (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	CK_DATE ck_date;
	GDate *date, date2;

	g_assert_false (gck_builder_find_date (&builder, 5, &date2));

	date = g_date_new_dmy(8, 8, 1960);
	memcpy (ck_date.year, "1960", 4);
	memcpy (ck_date.month, "08", 2);
	memcpy (ck_date.day, "08", 2);

	gck_builder_add_date (&builder, ATTR_TYPE, date);

	gck_builder_set_invalid (&builder, 5);
	g_assert_false (gck_builder_find_date (&builder, 5, &date2));
	attr = gck_builder_find (&builder, 5);
	gck_attribute_get_date (attr, &date2);
	g_assert_cmpint (date2.day, ==, 0);
	g_assert_cmpint (date2.month, ==, 0);
	g_assert_cmpint (date2.year, ==, 0);

	gck_builder_set_date (&builder, 5, date);

	attr = gck_builder_find (&builder, ATTR_TYPE);
	g_assert_nonnull (attr);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, &ck_date, sizeof (CK_DATE));
	if (!gck_builder_find_date (&builder, ATTR_TYPE, &date2))
		g_assert_not_reached ();
	g_assert_true (g_date_compare (date, &date2) == 0);

	if (!gck_builder_find_date (&builder, 5, &date2))
		g_assert_not_reached ();
	g_assert_true (g_date_compare (date, &date2) == 0);

	g_date_free (date);

	date = g_date_new_dmy(05, 06, 1960);
	memcpy (ck_date.year, "1960", 4);
	memcpy (ck_date.month, "06", 2);
	memcpy (ck_date.day, "05", 2);
	gck_builder_set_date (&builder, ATTR_TYPE, date);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, &ck_date, sizeof (CK_DATE));
	if (!gck_builder_find_date (&builder, ATTR_TYPE, &date2))
		g_assert_not_reached ();
	g_assert_true (g_date_compare (date, &date2) == 0);

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_nonnull (attr);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, &ck_date, sizeof (CK_DATE));

	gck_attribute_get_date (attr, &date2);
	g_assert_true (g_date_compare (date, &date2) == 0);

	g_date_free (date);

	g_assert_cmpuint (gck_attributes_count (attrs), ==, 2);
	gck_attributes_unref (attrs);
}

static void
test_build_ulong (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	gulong value;
	CK_ULONG ck_value = 0;

	g_assert_false (gck_builder_find_ulong (&builder, 5, &value));

	gck_builder_add_ulong (&builder, ATTR_TYPE, 99);

	gck_builder_set_invalid (&builder, 5);
	g_assert_false (gck_builder_find_ulong (&builder, 5, &value));
	gck_builder_set_ulong (&builder, 5, 292);

	attr = gck_builder_find (&builder, ATTR_TYPE);
	g_assert_nonnull (attr);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_ULONG));
	memcpy(&ck_value, attr->value, sizeof (CK_ULONG));
	g_assert_cmpuint (ck_value, ==, 99);
	if (!gck_builder_find_ulong (&builder, ATTR_TYPE, &value))
		g_assert_not_reached ();
	g_assert_cmpuint (value, ==, 99);

	gck_builder_set_ulong (&builder, ATTR_TYPE, 88);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_ULONG));
	memcpy(&ck_value, attr->value, sizeof (CK_ULONG));
	g_assert_cmpuint (ck_value, ==, 88);
	if (!gck_builder_find_ulong (&builder, ATTR_TYPE, &value))
		g_assert_not_reached ();
	g_assert_cmpuint (value, ==, 88);

	if (!gck_builder_find_ulong (&builder, 5, &value))
		g_assert_not_reached ();
	g_assert_cmpuint (value, ==, 292);

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_nonnull (attr);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, sizeof (CK_ULONG));
	memcpy(&ck_value, attr->value, sizeof (CK_ULONG));
	g_assert_cmpuint (ck_value, ==, 88);

	if (!gck_attributes_find_ulong (attrs, ATTR_TYPE, &value))
		g_assert_not_reached ();
	g_assert_cmpuint (value, ==, 88);
	g_assert_cmpuint (gck_attribute_get_ulong (attr), ==, 88);

	g_assert_cmpuint (gck_attributes_count (attrs), ==, 2);
	gck_attributes_unref (attrs);
}

static void
test_build_string (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	gchar *value;

	g_assert_false (gck_builder_find_string (&builder, 5, &value));

	gck_builder_add_string (&builder, ATTR_TYPE, "My my");

	gck_builder_set_invalid (&builder, 5);
	g_assert_false (gck_builder_find_string (&builder, 5, &value));
	gck_builder_set_string (&builder, 5, "Hello");

	attr = gck_builder_find (&builder, ATTR_TYPE);
	g_assert_nonnull (attr);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length, "My my", strlen ("My my"));

	if (!gck_builder_find_string (&builder, 5, &value))
		g_assert_not_reached ();
	g_assert_cmpstr (value, ==, "Hello");
	g_free (value);

	gck_builder_set_string (&builder, ATTR_TYPE, "a test string");
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length,
	                 "a test string", strlen ("a test string"));

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_nonnull (attr);

	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpmem (attr->value, attr->length,
	                 "a test string", strlen ("a test string"));

	if (!gck_attributes_find_string (attrs, ATTR_TYPE, &value))
		g_assert_not_reached ();
	g_assert_cmpstr ("a test string", ==, value);
	g_free (value);

	value = gck_attribute_get_string (attr);
	g_assert_cmpstr ("a test string", ==, value);
	g_free (value);

	g_assert_cmpuint (gck_attributes_count (attrs), ==, 2);
	gck_attributes_unref (attrs);
}

static void
test_build_string_null (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	gchar *value;

	gck_builder_add_string (&builder, ATTR_TYPE, NULL);

	g_assert_false (gck_builder_find_string (&builder, ATTR_TYPE, &value));

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_null (attr->value);
	g_assert_cmpuint (attr->length, ==, 0);

	value = gck_attribute_get_string (attr);
	g_assert_null (value);

	gck_attributes_unref (attrs);
}

static void
test_build_invalid (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;

	gck_builder_add_invalid (&builder, ATTR_TYPE);
	gck_builder_set_invalid (&builder, ATTR_TYPE);
	gck_builder_set_invalid (&builder, 5);

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, (gulong)-1);
	g_assert_null (attr->value);

	g_assert_true (gck_attribute_is_invalid (attr));

	g_assert_cmpuint (gck_attributes_count (attrs), ==, 2);
	gck_attributes_unref (attrs);
}

static void
test_build_empty (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;

	gck_builder_add_empty (&builder, ATTR_TYPE);
	gck_builder_set_empty (&builder, ATTR_TYPE);
	gck_builder_set_empty (&builder, 5);

	attr = gck_builder_find (&builder, 5);
	g_assert_cmpuint (attr->type, ==, 5);
	g_assert_cmpuint (attr->length, ==, 0);
	g_assert_null (attr->value);

	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);
	g_assert_cmpuint (attr->length, ==, 0);
	g_assert_null (attr->value);

	g_assert_cmpuint (gck_attributes_count (attrs), ==, 2);
	gck_attributes_unref (attrs);
}

static void
test_builder_secure (void)
{
	GckAttributes *attrs;
	GckBuilder builder;
	const GckAttribute *attr;

	gck_builder_init_full (&builder, GCK_BUILDER_SECURE_MEMORY);

	gck_builder_add_boolean (&builder, 88, TRUE);
	attrs = gck_builder_end (&builder);
	attr = gck_attributes_at (attrs, 0);

	g_assert_true (egg_secure_check (attr->value));

	gck_attributes_unref (attrs);
}

static void
test_builder_copy (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	GckBuilder *copy;
	const GckAttribute *attr;

	gck_builder_add_ulong (&builder, ATTR_TYPE, 88);
	copy = gck_builder_copy (&builder);
	gck_builder_clear (&builder);

	attrs = gck_builder_end (copy);
	gck_builder_unref (copy);

	attr = gck_attributes_at (attrs, 0);
	g_assert_cmpuint (gck_attribute_get_ulong (attr), ==, 88);
	g_assert_cmpuint (attr->type, ==, ATTR_TYPE);

	/* Should be able to copy null */
	copy = gck_builder_copy (NULL);
	g_assert_null (copy);

	gck_attributes_unref (attrs);
}

static void
test_builder_refs (void)
{
	GckBuilder *builder, *two;
	gulong check;

	builder = gck_builder_new (GCK_BUILDER_NONE);
	gck_builder_add_ulong (builder, 88, 99);

	two = gck_builder_ref (builder);

	g_assert_true (builder == two);

	if (!gck_builder_find_ulong (builder, 88, &check))
		g_assert_not_reached ();
	g_assert_cmpuint (check, ==, 99);

	gck_builder_unref (builder);

	if (!gck_builder_find_ulong (two, 88, &check))
		g_assert_not_reached ();
	g_assert_cmpuint (check, ==, 99);

	gck_builder_unref (two);
}

static void
test_builder_boxed (void)
{
	GckBuilder *builder, *two;
	gulong check;

	builder = gck_builder_new (GCK_BUILDER_NONE);
	gck_builder_add_ulong (builder, 88, 99);

	two = g_boxed_copy (GCK_TYPE_BUILDER, builder);

	g_assert_true (builder == two);

	if (!gck_builder_find_ulong (builder, 88, &check))
		g_assert_not_reached ();
	g_assert_cmpuint (check, ==, 99);

	g_boxed_free (GCK_TYPE_BUILDER, builder);

	if (!gck_builder_find_ulong (two, 88, &check))
		g_assert_not_reached ();
	g_assert_cmpuint (check, ==, 99);

	gck_builder_unref (two);
}

static void
test_builder_add_attr (void)
{
	GckBuilder bone = GCK_BUILDER_INIT;
	GckBuilder btwo = GCK_BUILDER_INIT;
	const GckAttribute *aone, *atwo;
	GckAttributes *aones, *atwos;
	gchar *value;

	gck_builder_add_string (&bone, ATTR_TYPE, "blah");
	aones = gck_builder_end (&bone);
	aone = gck_attributes_at (aones, 0);

	gck_builder_add_all (&btwo, aones);
	atwos = gck_builder_end (&btwo);
	atwo = gck_attributes_at (atwos, 0);

	/* Should be equal, and also share the values */
	gck_attribute_equal (aone, atwo);
	g_assert_true (aone->value == atwo->value);

	gck_attributes_unref (aones);

	value = gck_attribute_get_string (atwo);
	g_assert_cmpstr (value, ==, "blah");
	g_free (value);

	gck_attributes_unref (atwos);
}

static void
test_attribute_hash (void)
{
	guchar *data = (guchar *)"extra attribute";
	GckAttribute one = { CKA_LABEL, (guchar *)"yay", 3 };
	GckAttribute null = { CKA_LABEL, (guchar *)NULL, 3 };
	GckAttribute zero = { CKA_LABEL, (guchar *)NULL, 0 };
	GckAttribute two = { CKA_VALUE, (guchar *)"yay", 3 };
	GckAttribute other = { CKA_VALUE, data, 5 };
	GckAttribute overflow = { CKA_VALUE, data, 5 };
	GckAttribute content = { CKA_VALUE, (guchar *)"conte", 5 };
	guint hash;

	hash = gck_attribute_hash (&one);
	g_assert_cmpuint (hash, !=, 0);

	g_assert_cmpuint (gck_attribute_hash (&one), ==, hash);
	g_assert_cmpuint (gck_attribute_hash (&two), !=, hash);
	g_assert_cmpuint (gck_attribute_hash (&other), !=, hash);
	g_assert_cmpuint (gck_attribute_hash (&overflow), !=, hash);
	g_assert_cmpuint (gck_attribute_hash (&null), !=, hash);
	g_assert_cmpuint (gck_attribute_hash (&zero), !=, hash);
	g_assert_cmpuint (gck_attribute_hash (&content), !=, hash);
}

static void
test_attributes_refs (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;

	attrs = gck_builder_end (&builder);
	g_assert_nonnull (attrs);
	g_assert_cmpuint (gck_attributes_count (attrs), ==, 0);

	g_assert_true (gck_attributes_ref (attrs) == attrs);
	gck_attributes_unref (attrs);

	gck_attributes_unref (attrs);

	/* Can unref NULL */
	gck_attributes_unref (NULL);
}

static void
test_attributes_contents (GckAttributes *attrs,
                          gboolean extras,
                          gint count)
{
	const GckAttribute *attr;
	gchar *value;
	GDate date, *check;

	g_assert_nonnull (attrs);
	if (count < 0)
		count = extras ? 7 : 5;
	g_assert_cmpuint (gck_attributes_count (attrs), ==, count);

	attr = gck_attributes_at (attrs, 0);
	g_assert_cmpuint (attr->type, ==, 0);
	g_assert_true (gck_attribute_get_boolean (attr));

	attr = gck_attributes_at (attrs, 1);
	g_assert_cmpuint (attr->type, ==, 101);
	gck_assert_cmpulong (gck_attribute_get_ulong (attr), ==, 888);

	attr = gck_attributes_at (attrs, 2);
	g_assert_cmpuint (attr->type, ==, 202);
	value = gck_attribute_get_string (attr);
	g_assert_cmpstr (value, ==, "string");
	g_free (value);

	attr = gck_attributes_at (attrs, 3);
	g_assert_cmpuint (attr->type, ==, 303);
	check = g_date_new_dmy (11, 12, 2008);
	gck_attribute_get_date (attr, &date);
	g_assert_true (g_date_compare (&date, check) == 0);
	g_date_free (check);

	attr = gck_attributes_at (attrs, 4);
	g_assert_cmpuint (attr->type, ==, 404);
	g_assert_cmpmem (attr->value, attr->length, ATTR_DATA, N_ATTR_DATA);

	if (!extras)
		return;

	attr = gck_attributes_at (attrs, 5);
	g_assert_cmpuint (attr->type, ==, 505);
	g_assert_cmpuint (attr->length, ==, (gulong)-1);
	g_assert_null (attr->value);
	g_assert_true (gck_attribute_is_invalid (attr));

	attr = gck_attributes_at (attrs, 6);
	g_assert_cmpuint (attr->type, ==, 606);
	g_assert_cmpuint (attr->length, ==, 0);
	g_assert_null (attr->value);
}

static void
test_attributes_new_empty (void)
{
	GckAttributes *attrs;
	const GckAttribute *attr;

	attrs = gck_attributes_new_empty (GCK_INVALID);
	g_assert_cmpuint (gck_attributes_count (attrs), ==, 0);
	gck_attributes_unref (attrs);

	attrs = gck_attributes_new_empty (CKA_ID, CKA_LABEL, GCK_INVALID);
	g_assert_cmpuint (gck_attributes_count (attrs), ==, 2);
	attr = gck_attributes_at (attrs, 0);
	g_assert_cmpuint (attr->type, ==, CKA_ID);
	g_assert_cmpuint (attr->length, ==, 0);
	g_assert_null (attr->value);
	attr = gck_attributes_at (attrs, 1);
	g_assert_cmpuint (attr->type, ==, CKA_LABEL);
	g_assert_cmpuint (attr->length, ==, 0);
	g_assert_null (attr->value);
	gck_attributes_unref (attrs);
}

static void
test_attributes_empty (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	const GckAttribute *attr;
	guint i;

	gck_builder_add_empty (&builder, 101UL);
	gck_builder_add_empty (&builder, 202UL);
	gck_builder_add_empty (&builder, 303UL);
	gck_builder_add_empty (&builder, 404UL);
	attrs = gck_builder_end (&builder);

	g_assert_cmpuint (gck_attributes_count (attrs), ==, 4);
	for (i = 0; i < gck_attributes_count (attrs); ++i) {
		attr = gck_attributes_at (attrs, i);
		g_assert_cmpuint (attr->type, ==, ((i + 1) * 100) + i + 1);
		g_assert_null (attr->value);
		g_assert_cmpuint (attr->length, ==, 0);
	}

	gck_attributes_unref (attrs);
}

static void
test_builder_add_from (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckBuilder two = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	guint i;

	builder_add_fixtures (&builder, 0);
	attrs = gck_builder_end (&builder);

	for (i = 0; i < gck_attributes_count (attrs); i++)
		gck_builder_add_attribute (&two, gck_attributes_at (attrs, i));

	gck_attributes_unref (attrs);
	attrs = gck_builder_end (&two);

	test_attributes_contents (attrs, TRUE, -1);
	gck_attributes_unref (attrs);
}


static void
test_builder_add_all (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckBuilder two = GCK_BUILDER_INIT;
	GckAttributes *attrs;

	builder_add_fixtures (&builder, 0);
	attrs = gck_builder_end (&builder);

	gck_builder_add_all (&two, attrs);
	gck_attributes_unref (attrs);
	attrs = gck_builder_end (&two);

	test_attributes_contents (attrs, TRUE, -1);
	gck_attributes_unref (attrs);
}

static void
test_builder_set_all (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckBuilder two = GCK_BUILDER_INIT;
	GckAttributes *attrs;

	builder_add_fixtures (&builder, 5);
	builder_add_fixtures (&two, 0);
	attrs = gck_builder_end (&two);
	gck_builder_set_all (&builder, attrs);
	gck_attributes_unref (attrs);
	attrs = gck_builder_end (&builder);

	test_attributes_contents (attrs, TRUE, -1);
	gck_attributes_unref (attrs);
}


static void
test_builder_set_blank (void)
{
	GckBuilder builder;
	gboolean value;

	gck_builder_init (&builder);
	gck_builder_set_boolean (&builder, 5, TRUE);
	if (!gck_builder_find_boolean (&builder, 5, &value))
		g_assert_not_reached ();
	g_assert_true (value);
	gck_builder_clear (&builder);
}

static void
test_builder_add_only (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckBuilder two = GCK_BUILDER_INIT;
	GckAttributes *attrs;

	builder_add_fixtures (&builder, 0);
	attrs = gck_builder_end (&builder);

	gck_builder_add_only (&two, attrs, 0UL, 202UL, 404UL, 606UL, GCK_INVALID);
	gck_attributes_unref (attrs);
	attrs = gck_builder_end (&two);

	g_assert_nonnull (gck_attributes_find (attrs, 0UL));
	g_assert_nonnull (gck_attributes_find (attrs, 202UL));
	g_assert_nonnull (gck_attributes_find (attrs, 404UL));
	g_assert_nonnull (gck_attributes_find (attrs, 606UL));

	g_assert_null (gck_attributes_find (attrs, 101UL));
	g_assert_null (gck_attributes_find (attrs, 303UL));
	g_assert_null (gck_attributes_find (attrs, 505UL));

	gck_attributes_unref (attrs);
}

static void
test_builder_add_except (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckBuilder two = GCK_BUILDER_INIT;
	GckAttributes *attrs;

	builder_add_fixtures (&builder, 0);
	attrs = gck_builder_end (&builder);

	gck_builder_add_except (&two, attrs, 0UL, 202UL, 404UL, 606UL, GCK_INVALID);
	gck_attributes_unref (attrs);
	attrs = gck_builder_end (&two);

	g_assert_null (gck_attributes_find (attrs, 0UL));
	g_assert_null (gck_attributes_find (attrs, 202UL));
	g_assert_null (gck_attributes_find (attrs, 404UL));
	g_assert_null (gck_attributes_find (attrs, 606UL));

	g_assert_nonnull (gck_attributes_find (attrs, 101UL));
	g_assert_nonnull (gck_attributes_find (attrs, 303UL));
	g_assert_nonnull (gck_attributes_find (attrs, 505UL));

	gck_attributes_unref (attrs);
}

static void
test_builder_add_only_and_except (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckBuilder two = GCK_BUILDER_INIT;
	GckAttributes *attrs;

	builder_add_fixtures (&builder, 0);
	attrs = gck_builder_end (&builder);

	gck_builder_add_only (&two, attrs, 0UL, 101UL, 202UL, 303UL, GCK_INVALID);
	gck_builder_add_except (&two, attrs, 0UL, 101UL, 202UL, 303UL, GCK_INVALID);
	gck_attributes_unref (attrs);
	attrs = gck_builder_end (&two);

	test_attributes_contents (attrs, TRUE, -1);
	gck_attributes_unref (attrs);
}

static void
test_find_attributes (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GDate check, *date = g_date_new_dmy (13, 12, 2008);
	GckAttributes *attrs;
	const GckAttribute *attr;
	gboolean bvalue, ret;
	gulong uvalue;
	gchar *svalue;

	gck_builder_add_boolean (&builder, 0UL, TRUE);
	gck_builder_add_ulong (&builder, 101UL, 888UL);
	gck_builder_add_string (&builder, 202UL, "string");
	gck_builder_add_date (&builder, 303UL, date);
	gck_builder_add_data (&builder, 404UL, (const guchar *)ATTR_DATA, N_ATTR_DATA);
	attrs = gck_builder_end (&builder);

	attr = gck_attributes_find (attrs, 404);
	g_assert_nonnull (attr);
	g_assert_cmpmem (attr->value, attr->length, ATTR_DATA, N_ATTR_DATA);

	ret = gck_attributes_find_boolean (attrs, 0UL, &bvalue);
	g_assert_true (ret);
	g_assert_true (bvalue);

	ret = gck_attributes_find_ulong (attrs, 101UL, &uvalue);
	g_assert_true (ret);
	g_assert_cmpuint (uvalue, ==, 888);

	ret = gck_attributes_find_string (attrs, 202UL, &svalue);
	g_assert_true (ret);
	g_assert_nonnull (svalue);
	g_assert_cmpstr (svalue, ==, "string");
	g_free (svalue);

	ret = gck_attributes_find_date (attrs, 303UL, &check);
	g_assert_true (ret);
	g_assert_true (g_date_compare (date, &check) == 0);

	gck_attributes_unref (attrs);
	g_date_free (date);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/gck/value/to_boolean", test_value_to_boolean);
	g_test_add_func ("/gck/value/to_ulong", test_value_to_ulong);
	g_test_add_func ("/gck/attribute/init_memory", test_init_memory);
	g_test_add_func ("/gck/attribute/init_boolean", test_init_boolean);
	g_test_add_func ("/gck/attribute/init_date", test_init_date);
	g_test_add_func ("/gck/attribute/init_ulong", test_init_ulong);
	g_test_add_func ("/gck/attribute/init_string", test_init_string);
	g_test_add_func ("/gck/attribute/init_invalid", test_init_invalid);
	g_test_add_func ("/gck/attribute/init_empty", test_init_empty);
	g_test_add_func ("/gck/attribute/new_memory", test_new_memory);
	g_test_add_func ("/gck/attribute/new_boolean", test_new_boolean);
	g_test_add_func ("/gck/attribute/new_date", test_new_date);
	g_test_add_func ("/gck/attribute/new_ulong", test_new_ulong);
	g_test_add_func ("/gck/attribute/new_string", test_new_string);
	g_test_add_func ("/gck/attribute/new_invalid", test_new_invalid);
	g_test_add_func ("/gck/attribute/new_empty", test_new_empty);
	g_test_add_func ("/gck/attribute/get_boolean", test_get_boolean);
	g_test_add_func ("/gck/attribute/get_date", test_get_date);
	g_test_add_func ("/gck/attribute/get_ulong", test_get_ulong);
	g_test_add_func ("/gck/attribute/get_string", test_get_string);
	g_test_add_func ("/gck/attribute/dup_attribute", test_dup_attribute);
	g_test_add_func ("/gck/attribute/copy_attribute", test_copy_attribute);
	g_test_add_func ("/gck/attribute/hash", test_attribute_hash);
	g_test_add_func ("/gck/builder/blank", test_builder_blank);
	g_test_add_func ("/gck/builder/data", test_build_data);
	g_test_add_func ("/gck/builder/data-invalid", test_build_data_invalid);
	g_test_add_func ("/gck/builder/data-secure", test_build_data_secure);
	g_test_add_func ("/gck/builder/take", test_build_take);
	g_test_add_func ("/gck/builder/take-invalid", test_build_take_invalid);
	g_test_add_func ("/gck/builder/take-secure", test_build_take_secure);
	g_test_add_func ("/gck/builder/boolean", test_build_boolean);
	g_test_add_func ("/gck/builder/date", test_build_date);
	g_test_add_func ("/gck/builder/ulong", test_build_ulong);
	g_test_add_func ("/gck/builder/string", test_build_string);
	g_test_add_func ("/gck/builder/string-null", test_build_string_null);
	g_test_add_func ("/gck/builder/invalid", test_build_invalid);
	g_test_add_func ("/gck/builder/empty", test_build_empty);
	g_test_add_func ("/gck/builder/secure", test_builder_secure);
	g_test_add_func ("/gck/builder/copy", test_builder_copy);
	g_test_add_func ("/gck/builder/refs", test_builder_refs);
	g_test_add_func ("/gck/builder/boxed", test_builder_boxed);
	g_test_add_func ("/gck/builder/add-attr", test_builder_add_attr);
	g_test_add_func ("/gck/builder/add-all", test_builder_add_all);
	g_test_add_func ("/gck/builder/add-from", test_builder_add_from);
	g_test_add_func ("/gck/builder/add-only", test_builder_add_only);
	g_test_add_func ("/gck/builder/add-except", test_builder_add_except);
	g_test_add_func ("/gck/builder/add-only-and-except", test_builder_add_only_and_except);
	g_test_add_func ("/gck/builder/set-all", test_builder_set_all);
	g_test_add_func ("/gck/builder/set-blank", test_builder_set_blank);
	g_test_add_func ("/gck/attributes/refs", test_attributes_refs);
	g_test_add_func ("/gck/attributes/new-empty", test_attributes_new_empty);
	g_test_add_func ("/gck/attributes/empty", test_attributes_empty);
	g_test_add_func ("/gck/attributes/find_attributes", test_find_attributes);

	return g_test_run ();
}
