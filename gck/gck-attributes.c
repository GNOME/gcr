/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-attribute.c - the GObject PKCS#11 wrapper library

   Copyright (C) 2008, Stefan Walter

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

   Author: Stef Walter <nielsen@memberwebs.com>
*/

#include "config.h"

#include "gck.h"
#include "gck-private.h"
#include "pkcs11-trust-assertions.h"

#include "egg/egg-secure-memory.h"

#include <stdlib.h>
#include <string.h>

/**
 * GckAttribute:
 * @type: The attribute type, such as `CKA_LABEL`.
 * @value: (array length=length): The value of the attribute. May be %NULL.
 * @length: The length of the attribute. May be [const@INVALID] if the
 * attribute is invalid.
 *
 * This structure represents a PKCS#11 `CK_ATTRIBUTE`. These attributes contain
 * information about a PKCS#11 object. Use [method@Object.get] or
 * [method@Object.set] to set and attributes on an object.
 *
 * Although you are free to allocate a `GckAttribute` in your own code, no
 * functions in this library will operate on such an attribute.
 */

G_STATIC_ASSERT (sizeof (GckAttribute) == sizeof (CK_ATTRIBUTE));

struct _GckAttributes {
	GckAttribute *data;
	gulong count;
	gint refs;
};

typedef struct {
	GArray *array;
	gboolean secure;
	gint refs;
} GckRealBuilder;

G_STATIC_ASSERT (sizeof (GckRealBuilder) <= sizeof (GckBuilder));

EGG_SECURE_DECLARE (attributes);

#define MAX_ALIGN 16

static guchar *
value_take (gpointer data,
            gsize length,
            gboolean secure)
{
	gsize len = length + MAX_ALIGN;
	guchar *value;

	if (secure)
		value = egg_secure_realloc (data, len);
	else
		value = g_realloc (data, len);
	g_assert (value != NULL);

	memmove (value + MAX_ALIGN, value, length);
	g_atomic_int_set ((gint *)value, 1);
	return value + MAX_ALIGN;
}

static guchar *
value_blank (gsize length,
             gboolean secure)
{
	gsize len = length + MAX_ALIGN;
	guchar *value;

	if (secure)
		value = egg_secure_alloc (len);
	else
		value = g_malloc (len);
	g_assert (value != NULL);

	g_atomic_int_set ((gint *)value, 1);
	return value + MAX_ALIGN;
}

static guchar *
value_new (gconstpointer data,
           gsize length,
           gboolean secure)
{
	guchar *result;

	result = value_blank (length, secure);
	memcpy (result, data, length);
	return result;
}

static guchar *
value_ref (guchar *data)
{
	guchar *value = data - MAX_ALIGN;
	gint previous;

	g_assert (data != NULL);

	previous = g_atomic_int_add ((gint *)value, 1);
	if (G_UNLIKELY (previous <= 0)) {
		g_warning ("An owned GckAttribute value has been modified outside of the "
		           "gck library or an invalid attribute was passed to gck_builder_add_attribute()");
		return NULL;
	}

	return data;
}

static void
value_unref (guchar *data)
{
	guchar *value = data - MAX_ALIGN;

	g_assert (data != NULL);

	if (g_atomic_int_dec_and_test ((gint *)value)) {
		if (egg_secure_check (value))
			egg_secure_free (value);
		else
			g_free (value);
	}
}

G_DEFINE_BOXED_TYPE (GckBuilder, gck_builder,
                     gck_builder_ref, gck_builder_unref)

/**
 * GckBuilder:
 *
 * A builder for a set of attributes. Add attributes to a builder, and then use
 * [method@Builder.end] to get the completed [struct@Attributes].
 *
 * The fields of #GckBuilder are private and not to be accessed directly.
 */

/**
 * GckBuilderFlags:
 * @GCK_BUILDER_NONE: no special flags
 * @GCK_BUILDER_SECURE_MEMORY: use non-pageable memory for the values of the attributes
 *
 * Flags to be used with a [method@Builder.init_full] and [ctor@Builder.new].
 */

/**
 * GCK_BUILDER_INIT:
 *
 * Values that can be assigned to a #GckBuilder allocated on the stack.
 *
 * ```c
 * GckBuilder builder = GCK_BUILDER_INIT;
 * ```
 */

/**
 * gck_builder_new:
 * @flags: flags for the new builder
 *
 * Create a new `GckBuilder` not allocated on the stack, so it can be shared
 * across a single scope, and referenced / unreferenced.
 *
 * Normally a `GckBuilder` is created on the stack, and simply initialized.
 *
 * If the %GCK_BUILDER_SECURE_MEMORY flag is specified then non-pageable memory
 * will be used for the various values of the attributes in the builder
 *
 * Returns: (transfer full): a new builder, to be freed with gck_builder_unref()
 */
GckBuilder *
gck_builder_new (GckBuilderFlags flags)
{
	GckBuilder *builder;
	GckRealBuilder *real;
	builder = g_new0 (GckBuilder, 1);
	gck_builder_init_full (builder, flags);
	real = (GckRealBuilder *)builder;
	real->refs = 1;
	return builder;
}

/**
 * gck_builder_ref:
 * @builder: the builder
 *
 * Add a reference to a builder that was created with [ctor@Builder.new]. The
 * builder must later be unreferenced again with gck_builder_unref().
 *
 * It is an error to use this function on builders that were allocated on the
 * stack.
 *
 * Returns: the builder
 */
GckBuilder *
gck_builder_ref (GckBuilder *builder)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	gboolean stack;

	g_return_val_if_fail (builder != NULL, NULL);

	stack = g_atomic_int_add (&real->refs, 1) == 0;
	if G_UNLIKELY (stack) {
		g_warning ("Never call gck_builder_ref() on a stack allocated GckBuilder structure");
		return NULL;
	}

	return builder;
}

/**
 * gck_builder_unref:
 * @builder: (transfer full): the builder
 *
 * Unreferences a builder. If this was the last reference then the builder
 * is freed.
 *
 * It is an error to use this function on builders that were allocated on the
 * stack.
 */
void
gck_builder_unref (GckBuilder *builder)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;

	if (builder == NULL)
		return;

	if (g_atomic_int_dec_and_test (&real->refs)) {
		gck_builder_clear (builder);
		g_free (builder);
	}
}

/**
 * gck_builder_init_full:
 * @builder: the builder
 * @flags: the flags for the new builder
 *
 * Initialize a stack allocated builder, with the appropriate flags.
 *
 * If the %GCK_BUILDER_SECURE_MEMORY flag is specified then non-pageable memory
 * will be used for the various values of the attributes in the builder
 */
void
gck_builder_init_full (GckBuilder *builder,
                       GckBuilderFlags flags)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;

	g_return_if_fail (builder != NULL);

	memset (builder, 0, sizeof (GckBuilder));
	real->secure = flags & GCK_BUILDER_SECURE_MEMORY;
}

/**
 * gck_builder_init:
 * @builder: the builder
 *
 * Initialize a stack allocated builder, with the default flags.
 *
 * This is equivalent to initializing a builder variable with the
 * %GCK_BUILDER_INIT constant, or setting it to zeroed memory.
 *
 * ```c
 * // Equivalent ways of initializing a GckBuilder
 * GckBuilder builder = GCK_BUILDER_INIT;
 * GckBuilder builder2;
 * GckBuilder builder3;
 *
 * gck_builder_init (&builder2);
 *
 * memset (&builder3, 0, sizeof (builder3));
 * ```
 */
void
gck_builder_init (GckBuilder *builder)
{
	gck_builder_init_full (builder, GCK_BUILDER_NONE);
}

static GckAttribute *
builder_push (GckBuilder *builder,
              gulong attr_type)
{
	GckAttribute attr = { attr_type, NULL, 0 };
	GckRealBuilder *real = (GckRealBuilder *)builder;
	if (real->array == NULL)
		real->array = g_array_new (FALSE, TRUE, sizeof (GckAttribute));
	g_array_append_val (real->array, attr);
	return &g_array_index (real->array, GckAttribute, real->array->len - 1);
}

static void
builder_clear (GckAttribute *attr)
{
	attr->length = 0;
	if (attr->value)
		value_unref (attr->value);
	attr->value = NULL;
}

static GckAttribute *
find_attribute (GckAttribute *attrs,
                gsize n_attrs,
                gulong attr_type)
{
	guint i;

	for (i = 0; i < n_attrs; ++i) {
		if (attrs[i].type == attr_type)
			return attrs + i;
	}

	return NULL;
}

static GckAttribute *
builder_clear_or_push (GckBuilder *builder,
                       gulong attr_type)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckAttribute *attr = NULL;

	if (real->array)
		attr = find_attribute ((GckAttribute *)real->array->data,
		                       real->array->len, attr_type);
	if (attr == NULL)
		attr = builder_push (builder, attr_type);
	else
		builder_clear (attr);
	return attr;
}

static void
builder_copy (GckBuilder *builder,
              const GckAttribute *attr,
              gboolean performing_set)
{
	GckAttribute *copy;

	if (performing_set)
		copy = builder_clear_or_push (builder, attr->type);
	else
		copy = builder_push (builder, attr->type);
	if (attr->length == G_MAXULONG) {
		copy->value = NULL;
		copy->length = G_MAXULONG;
	} else if (attr->value == NULL) {
		copy->value = NULL;
		copy->length = 0;
	} else {
		copy->value = value_ref (attr->value);
		copy->length = attr->length;
	}
}

/**
 * gck_builder_copy:
 * @builder: the builder to copy
 *
 * Make a copy of the builder and its state. The new builder is allocated
 * with [ctor@Builder.new] and should be freed with gck_builder_unref().
 *
 * Attribute value memory is automatically shared between the two builders,
 * and is only freed when both are gone.
 *
 * Returns: (transfer full): the builder copy, which should be freed with
 *          gck_builder_unref().
 */
GckBuilder *
gck_builder_copy (GckBuilder *builder)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckBuilder *copy;
	guint i;

	if (builder == NULL)
		return NULL;

	copy = gck_builder_new (real->secure ? GCK_BUILDER_SECURE_MEMORY : GCK_BUILDER_NONE);
	for (i = 0; real->array && i < real->array->len; i++)
		builder_copy (copy, &g_array_index (real->array, GckAttribute, i), FALSE);

	return copy;
}

/**
 * gck_builder_take_data:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: (transfer full) (array length=length) (nullable): the new
 *         attribute memory
 * @length: the length of the memory
 *
 * Add a new attribute to the builder with an arbitrary value. Unconditionally
 * adds a new attribute, even if one with the same @attr_type already exists.
 *
 * Ownership of the @value memory is taken by the builder, may be reallocated,
 * and is eventually freed with g_free(). The memory must have been allocated
 * using the standard GLib memory allocation routines.
 *
 * %NULL may be specified for the @value argument, in which case an empty
 * attribute is created. [const@INVALID] may be specified for the length, in
 * which case an invalid attribute is created in the PKCS#11 style.
 */
void
gck_builder_take_data (GckBuilder *builder,
                       gulong attr_type,
                       guchar *value,
                       gsize length)
{
	GckAttribute *attr;
	gboolean secure;

	g_return_if_fail (builder != NULL);

	secure = value && egg_secure_check (value);

	attr = builder_push (builder, attr_type);
	if (length == G_MAXULONG) {
		if (secure)
			egg_secure_free (value);
		else
			g_free (value);
		attr->value = NULL;
		attr->length = G_MAXULONG;
	} else if (value == NULL) {
		attr->value = NULL;
		attr->length = 0;
	} else {
		attr->value = value_take (value, length, secure);
		attr->length = length;
	}
}

/**
 * gck_builder_add_data:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: (array length=length) (nullable): the new attribute memory
 * @length: the length of the memory
 *
 * Add a new attribute to the builder with an arbitrary value. Unconditionally
 * adds a new attribute, even if one with the same @attr_type already exists.
 *
 * The memory in @value is copied by the builder.
 *
 * %NULL may be specified for the @value argument, in which case an empty
 * attribute is created. [const@INVALID] may be specified for the length, in
 * which case an invalid attribute is created in the PKCS#11 style.
 */
void
gck_builder_add_data (GckBuilder *builder,
                      gulong attr_type,
                      const guchar *value,
                      gsize length)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckAttribute *attr;

	g_return_if_fail (builder != NULL);

	attr = builder_push (builder, attr_type);
	if (length == G_MAXULONG) {
		attr->value = NULL;
		attr->length = G_MAXULONG;
	} else if (value == NULL) {
		attr->value = NULL;
		attr->length = 0;
	} else {
		attr->value = value_new (value, length,
		                         real->secure || egg_secure_check (value));
		attr->length = length;
	}
}

/**
 * gck_builder_set_data:
 * @builder: the builder
 * @attr_type: the attribute type
 * @value: (array length=length) (nullable): the new attribute memory
 * @length: the length of the memory
 *
 * Set a new attribute to the builder with an arbitrary value. If an attribute
 * with @attr_type already exists in the builder then it is changed to the new
 * value, otherwise an attribute is added.
 *
 * The memory in @value is copied by the builder.
 *
 * %NULL may be specified for the @value argument, in which case an empty
 * attribute is created. [const@INVALID] may be specified for the length, in
 * which case an invalid attribute is created in the PKCS#11 style.
 */
void
gck_builder_set_data (GckBuilder *builder,
                      gulong attr_type,
                      const guchar *value,
                      gsize length)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckAttribute *attr;

	g_return_if_fail (builder != NULL);

	attr = builder_clear_or_push (builder, attr_type);
	if (length == G_MAXULONG) {
		attr->value = NULL;
		attr->length = G_MAXULONG;
	} else if (value == NULL) {
		attr->value = NULL;
		attr->length = 0;
	} else {
		attr->value = value_new (value, length,
		                         real->secure || egg_secure_check (value));
		attr->length = length;
	}
}

/**
 * gck_builder_add_empty:
 * @builder: the builder
 * @attr_type: the new attribute type
 *
 * Add a new attribute to the builder that is empty. Unconditionally
 * adds a new attribute, even if one with the same @attr_type already exists.
 */
void
gck_builder_add_empty (GckBuilder *builder,
                       gulong attr_type)
{
	g_return_if_fail (builder != NULL);

	builder_push (builder, attr_type);
}

/**
 * gck_builder_set_empty:
 * @builder: the builder
 * @attr_type: the attribute type
 *
 * Set an attribute on the builder that is empty. If an attribute
 * with @attr_type already exists in the builder then it is changed to the new
 * value, otherwise an attribute is added.
 */
void
gck_builder_set_empty (GckBuilder *builder,
                       gulong attr_type)
{
	g_return_if_fail (builder != NULL);

	builder_clear_or_push (builder, attr_type);
}

/**
 * gck_builder_add_invalid:
 * @builder: the builder
 * @attr_type: the new attribute type
 *
 * Add a new attribute to the builder that is invalid in the PKCS#11 sense.
 * Unconditionally adds a new attribute, even if one with the same @attr_type
 * already exists.
 */
void
gck_builder_add_invalid (GckBuilder *builder,
                         gulong attr_type)
{
	GckAttribute *attr;

	g_return_if_fail (builder != NULL);

	attr = builder_push (builder, attr_type);
	attr->length = (gulong)-1;
}

/**
 * gck_builder_set_invalid:
 * @builder: the builder
 * @attr_type: the attribute type
 *
 * Set an attribute on the builder that is invalid in the PKCS#11 sense.
 * If an attribute with @attr_type already exists in the builder then it is
 * changed to the new value, otherwise an attribute is added.
 */
void
gck_builder_set_invalid (GckBuilder *builder,
                         gulong attr_type)
{
	GckAttribute *attr;

	g_return_if_fail (builder != NULL);

	attr = builder_clear_or_push (builder, attr_type);
	attr->length = (gulong)-1;
}

/**
 * gck_builder_add_ulong:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: the attribute value
 *
 * Add a new attribute to the builder for the unsigned long @value.
 * Unconditionally adds a new attribute, even if one with the same @attr_type
 * already exists.
 */
void
gck_builder_add_ulong (GckBuilder *builder,
                       gulong attr_type,
                       gulong value)
{
	CK_ULONG uval = value;
	gck_builder_add_data (builder, attr_type,
	                      (const guchar *)&uval, sizeof (uval));
}

/**
 * gck_builder_set_ulong:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: the attribute value
 *
 * Set an attribute on the builder for the unsigned long @value.
 * If an attribute with @attr_type already exists in the builder then it is
 * changed to the new value, otherwise an attribute is added.
 */
void
gck_builder_set_ulong (GckBuilder *builder,
                       gulong attr_type,
                       gulong value)
{
	CK_ULONG uval = value;
	gck_builder_set_data (builder, attr_type,
	                      (const guchar *)&uval, sizeof (uval));
}

/**
 * gck_builder_add_boolean:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: the attribute value
 *
 * Add a new attribute to the builder for the boolean @value.
 * Unconditionally adds a new attribute, even if one with the same @attr_type
 * already exists.
 */
void
gck_builder_add_boolean (GckBuilder *builder,
                         gulong attr_type,
                         gboolean value)
{
	CK_BBOOL bval = value ? CK_TRUE : CK_FALSE;
	gck_builder_add_data (builder, attr_type,
	                      (const guchar *)&bval, sizeof (bval));
}

/**
 * gck_builder_set_boolean:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: the attribute value
 *
 * Set an attribute on the builder for the boolean @value.
 * If an attribute with @attr_type already exists in the builder then it is
 * changed to the new value, otherwise an attribute is added.
 */
void
gck_builder_set_boolean (GckBuilder *builder,
                         gulong attr_type,
                         gboolean value)
{
	CK_BBOOL bval = value ? CK_TRUE : CK_FALSE;
	gck_builder_set_data (builder, attr_type,
	                      (const guchar *)&bval, sizeof (bval));
}

static void
convert_gdate_to_ckdate (const GDate *value,
                         CK_DATE *date)
{
	gchar buffer[9];
	g_snprintf (buffer, sizeof (buffer), "%04d%02d%02d",
	            (int)g_date_get_year (value),
	            (int)g_date_get_month (value),
	            (int)g_date_get_day (value));
	memcpy (&date->year, buffer + 0, 4);
	memcpy (&date->month, buffer + 4, 2);
	memcpy (&date->day, buffer + 6, 2);
}

/**
 * gck_builder_add_date:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: the attribute value
 *
 * Add a new attribute to the builder for the date @value.
 * Unconditionally adds a new attribute, even if one with the same @attr_type
 * already exists.
 */
void
gck_builder_add_date (GckBuilder *builder,
                      gulong attr_type,
                      const GDate *value)
{
	CK_DATE date;

	g_return_if_fail (value != NULL);

	convert_gdate_to_ckdate (value, &date);
	gck_builder_add_data (builder, attr_type,
	                      (const guchar *)&date, sizeof (CK_DATE));
}

/**
 * gck_builder_set_date:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: the attribute value
 *
 * Set an attribute on the builder for the date @value.
 * If an attribute with @attr_type already exists in the builder then it is
 * changed to the new value, otherwise an attribute is added.
 */
void
gck_builder_set_date (GckBuilder *builder,
                      gulong attr_type,
                      const GDate *value)
{
	CK_DATE date;

	g_return_if_fail (value != NULL);

	convert_gdate_to_ckdate (value, &date);
	gck_builder_set_data (builder, attr_type,
	                      (const guchar *)&date, sizeof (CK_DATE));
}

/**
 * gck_builder_add_string:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: (nullable): the attribute value
 *
 * Add a new attribute to the builder for the string @value or %NULL.
 * Unconditionally adds a new attribute, even if one with the same @attr_type
 * already exists.
 */
void
gck_builder_add_string (GckBuilder *builder,
                        gulong attr_type,
                        const gchar *value)
{
	gck_builder_add_data (builder, attr_type,
	                      (const guchar *)value, value ? strlen (value) : 0);
}

/**
 * gck_builder_set_string:
 * @builder: the builder
 * @attr_type: the new attribute type
 * @value: the attribute value
 *
 * Set an attribute on the builder for the string @value or %NULL.
 * If an attribute with @attr_type already exists in the builder then it is
 * changed to the new value, otherwise an attribute is added.
 */
void
gck_builder_set_string (GckBuilder *builder,
                        gulong attr_type,
                        const gchar *value)
{
	gck_builder_set_data (builder, attr_type,
	                      (const guchar *)value, value ? strlen (value) : 0);
}

/**
 * gck_builder_add_attribute:
 * @builder: the builder
 * @attr: the attribute to add
 *
 * Add an attribute to the builder. The attribute is added unconditionally whether
 * or not an attribute with the same type already exists on the builder.
 *
 * The @attr attribute must have been created or owned by the Gck library.
 * If you call this function on an arbitrary `GckAttribute` that is allocated on
 * the stack or elsewhere, then this will result in undefined behavior.
 *
 * As an optimization, the attribute memory value is automatically shared
 * between the attribute and the builder.
 */
void
gck_builder_add_attribute (GckBuilder *builder,
                           const GckAttribute *attr)
{
	g_return_if_fail (builder != NULL);
	g_return_if_fail (attr != NULL);

	builder_copy (builder, attr, FALSE);
}

/**
 * gck_builder_add_all:
 * @builder: the builder
 * @attrs: the attributes to add
 *
 * Add all the @attrs attributes to the builder. The attributes are added
 * uncondititionally whether or not attributes with the same types already
 * exist in the builder.
 *
 * As an optimization, the attribute memory values are automatically shared
 * between the attributes and the builder.
 */
void
gck_builder_add_all (GckBuilder *builder,
                     GckAttributes *attrs)
{
	gulong i;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (attrs != NULL);

	for (i = 0; i < attrs->count; i++)
		builder_copy (builder, &attrs->data[i], FALSE);
}

/**
 * gck_builder_add_only: (skip)
 * @builder: the builder
 * @attrs: the attributes to add
 * @only_type: the first type of attribute to add
 * @...: the remaining attribute types to add, ending with [const@INVALID]
 *
 * Add the attributes specified in the argument list from @attrs to the
 * builder. The attributes are added uncondititionally whether or not
 * attributes with the same types already exist in the builder.
 *
 * The variable arguments must be unsigned longs.
 *
 * ```c
 * // Add the CKA_ID and CKA_CLASS attributes from attrs to builder
 * gck_builder_add_only (builder, attrs, CKA_ID, CKA_CLASS, GCK_INVALID);
 * ```
 *
 * As an optimization, the attribute memory values are automatically shared
 * between the attributes and the builder.
 */
void
gck_builder_add_only (GckBuilder *builder,
                      GckAttributes *attrs,
                      gulong only_type,
                      ...)
{
	GArray *types;
	va_list va;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (attrs != NULL);

	types = g_array_new (FALSE, FALSE, sizeof (gulong));

	va_start (va, only_type);
	while (only_type != GCK_INVALID) {
		g_array_append_val (types, only_type);
		only_type = va_arg (va, gulong);
	}
	va_end (va);

	gck_builder_add_onlyv (builder, attrs, (gulong *)types->data, types->len);
	g_array_free (types, TRUE);
}

/**
 * gck_builder_add_onlyv: (rename-to gck_builder_add_only)
 * @builder: the builder
 * @attrs: the attributes to add
 * @only_types: (array length=n_only_types): the types of attributes to add
 * @n_only_types: the number of attributes
 *
 * Add the attributes with the types in @only_types from @attrs to the
 * builder. The attributes are added uncondititionally whether or not
 * attributes with the same types already exist in the builder.
 *
 * ```c
 * // Add the CKA_ID and CKA_CLASS attributes from attrs to builder
 * gulong only[] = { CKA_ID, CKA_CLASS };
 * gck_builder_add_onlyv (builder, attrs, only, 2);
 * ```
 *
 * As an optimization, the attribute memory values are automatically shared
 * between the attributes and the builder.
 */
void
gck_builder_add_onlyv (GckBuilder *builder,
                       GckAttributes *attrs,
                       const gulong *only_types,
                       guint n_only_types)
{
	gulong i;
	guint j;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (attrs != NULL);

	for (i = 0; i < attrs->count; i++) {
		for (j = 0; j < n_only_types; j++) {
			if (attrs->data[i].type == only_types[j])
				builder_copy (builder, &attrs->data[i], FALSE);
		}
	}
}

/**
 * gck_builder_add_except: (skip)
 * @builder: the builder
 * @attrs: the attributes to add
 * @except_type: the first type of attribute to to exclude
 * @...: the remaining attribute types to exclude, ending with [const@INVALID]
 *
 * Add the attributes in @attrs to the builder, with the exception of those
 * in the argument list. The attributes are added uncondititionally whether or
 * not attributes with the same types already exist in the builder.
 *
 * The variable arguments must be unsigned longs.
 *
 * ```c
 * // Add all attributes in attrs except CKA_CLASS to the builder
 * gck_builder_add_except (builder, attrs, CKA_CLASS, GCK_INVALID);
 * ```
 *
 * As an optimization, the attribute memory values are automatically shared
 * between the attributes and the builder.
 */
void
gck_builder_add_except (GckBuilder *builder,
                        GckAttributes *attrs,
                        gulong except_type,
                        ...)
{
	GArray *types;
	va_list va;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (attrs != NULL);

	types = g_array_new (FALSE, FALSE, sizeof (gulong));

	va_start (va, except_type);
	while (except_type != GCK_INVALID) {
		g_array_append_val (types, except_type);
		except_type = va_arg (va, gulong);
	}
	va_end (va);

	gck_builder_add_exceptv (builder, attrs, (gulong *)types->data, types->len);
	g_array_free (types, TRUE);
}

/**
 * gck_builder_add_exceptv: (skip)
 * @builder: the builder
 * @attrs: the attributes to add
 * @except_types: (array length=n_except_types): the except types
 * @n_except_types: the number of except types
 *
 * Add the attributes in @attrs to the builder, with the exception of those
 * whose types are specified in @except_types. The attributes are added
 * uncondititionally whether or not attributes with the same types already
 * exist in the builder.
 *
 * ```c
 * // Add all attributes in attrs except CKA_CLASS to the builder
 * unsigned long except_types[] = { CKA_CLASS };
 * gck_builder_add_exceptv (builder, attrs, except_types, 1);
 * ```
 *
 * As an optimization, the attribute memory values are automatically shared
 * between the attributes and the builder.
 */
void
gck_builder_add_exceptv (GckBuilder *builder,
                         GckAttributes *attrs,
                         const gulong *except_types,
                         guint n_except_types)
{
	gulong i;
	guint j;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (attrs != NULL);

	for (i = 0; i < attrs->count; i++) {
		for (j = 0; j < n_except_types; j++) {
			if (attrs->data[i].type == except_types[j])
				break;
		}
		if (j == n_except_types)
			builder_copy (builder, &attrs->data[i], FALSE);
	}
}

/**
 * gck_builder_set_all:
 * @builder: the builder
 * @attrs: the attributes to set
 *
 * Set all the @attrs attributes to the builder. If any attributes with the
 * same types are already present in the builder, then those attributes are
 * changed to the new values.
 *
 * As an optimization, the attribute memory values are automatically shared
 * between the attributes and the builder.
 */
void
gck_builder_set_all (GckBuilder *builder,
                     GckAttributes *attrs)
{
	gulong i;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (attrs != NULL);

	for (i = 0; i < attrs->count; i++)
		builder_copy (builder, &attrs->data[i], TRUE);
}

/**
 * gck_builder_find:
 * @builder: the builder
 * @attr_type: the type of attribute to find
 *
 * Find an attribute in the builder. Both valid and invalid attributes (in
 * the PKCS#11 sense) are returned. If multiple attributes exist for the given
 * attribute type, then the first one is returned.
 *
 * The returned [struct@Attribute] is owned by the builder and may not be
 * modified in any way. It is only valid until another attribute is added to or
 * set on the builder, or until the builder is cleared or unreferenced.
 *
 * Returns: the attribute or %NULL if not found
 */
const GckAttribute *
gck_builder_find (GckBuilder *builder,
                  gulong attr_type)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;

	g_return_val_if_fail (builder != NULL, NULL);

	if (real->array == NULL)
		return NULL;

	return find_attribute ((GckAttribute *)real->array->data,
	                       real->array->len, attr_type);
}

static gboolean
find_attribute_boolean (GckAttribute *attrs,
                        gsize n_attrs,
                        gulong attr_type,
                        gboolean *value)
{
	GckAttribute *attr;

	attr = find_attribute (attrs, n_attrs, attr_type);
	if (!attr || gck_attribute_is_invalid (attr))
		return FALSE;
	return gck_value_to_boolean (attr->value, attr->length, value);
}

/**
 * gck_builder_find_boolean:
 * @builder: the builder
 * @attr_type: the type of attribute to find
 * @value: (out): the location to place the found value
 *
 * Find a boolean attribute in the builder that has the type @attr_type, is
 * of the correct boolean size, and is not invalid in the PKCS#11 sense.
 * If multiple attributes exist for the given attribute type, then the first\
 * one is returned.
 *
 * Returns: whether a valid boolean attribute was found
 */
gboolean
gck_builder_find_boolean (GckBuilder *builder,
                          gulong attr_type,
                          gboolean *value)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;

	g_return_val_if_fail (builder != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	if (real->array == NULL)
		return FALSE;

	return find_attribute_boolean ((GckAttribute *)real->array->data,
	                               real->array->len, attr_type, value);
}

static gboolean
find_attribute_ulong (GckAttribute *attrs,
                      gsize n_attrs,
                      gulong attr_type,
                      gulong *value)
{
	GckAttribute *attr;

	attr = find_attribute (attrs, n_attrs, attr_type);
	if (!attr || gck_attribute_is_invalid (attr))
		return FALSE;
	return gck_value_to_ulong (attr->value, attr->length, value);
}

/**
 * gck_builder_find_ulong:
 * @builder: the builder
 * @attr_type: the type of attribute to find
 * @value: (out): the location to place the found value
 *
 * Find a unsigned long attribute in the builder that has the type @attr_type,
 * is of the correct unsigned long size, and is not invalid in the PKCS#11 sense.
 * If multiple attributes exist for the given attribute type, then the first
 * one is returned.
 *
 * Returns: whether a valid unsigned long attribute was found
 */
gboolean
gck_builder_find_ulong (GckBuilder *builder,
                        gulong attr_type,
                        gulong *value)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;

	g_return_val_if_fail (builder != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	if (real->array == NULL)
		return FALSE;

	return find_attribute_ulong ((GckAttribute *)real->array->data,
	                             real->array->len, attr_type, value);
}

static gboolean
find_attribute_string (GckAttribute *attrs,
                       gsize n_attrs,
                       gulong attr_type,
                       gchar **value)
{
	GckAttribute *attr;
	gchar *string;

	attr = find_attribute (attrs, n_attrs, attr_type);
	if (!attr || gck_attribute_is_invalid (attr))
		return FALSE;
	string = gck_attribute_get_string (attr);
	if (string == NULL)
		return FALSE;
	*value = string;
	return TRUE;
}

/**
 * gck_builder_find_string:
 * @builder: the builder
 * @attr_type: the type of attribute to find
 * @value: (out): the location to place the found value
 *
 * Find a string attribute in the builder that has the type @attr_type, has a
 * non %NULL value pointer, and is not invalid in the PKCS#11 sense.
 * If multiple attributes exist for the given attribute type, then the first
 * one is returned.
 *
 * Returns: whether a valid string attribute was found
 */
gboolean
gck_builder_find_string (GckBuilder *builder,
                         gulong attr_type,
                         gchar **value)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;

	g_return_val_if_fail (builder != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	if (real->array == NULL)
		return FALSE;

	return find_attribute_string ((GckAttribute *)real->array->data,
	                              real->array->len, attr_type, value);
}

static gboolean
find_attribute_date (GckAttribute *attrs,
                     gsize n_attrs,
                     gulong attr_type,
                     GDate *value)
{
	GckAttribute *attr;

	attr = find_attribute (attrs, n_attrs, attr_type);
	if (!attr || gck_attribute_is_invalid (attr))
		return FALSE;
	gck_attribute_get_date (attr, value);
	return TRUE;
}

/**
 * gck_builder_find_date:
 * @builder: the builder
 * @attr_type: the type of attribute to find
 * @value: (out): the location to place the found value
 *
 * Find a date attribute in the builder that has the type @attr_type, is of
 * the correct date size, and is not invalid in the PKCS#11 sense.
 * If multiple attributes exist for the given attribute type, then the first
 * one is returned.
 *
 * Returns: whether a valid date attribute was found
 */
gboolean
gck_builder_find_date (GckBuilder *builder,
                       gulong attr_type,
                       GDate *value)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;

	g_return_val_if_fail (builder != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	if (real->array == NULL)
		return FALSE;

	return find_attribute_date ((GckAttribute *)real->array->data,
	                            real->array->len, attr_type, value);
}

/**
 * gck_builder_end:
 * @builder: the builder
 *
 * Take the attributes that have been built in the #GckBuilder. The builder
 * will no longer contain any attributes after this function call.
 *
 * Returns: (transfer full): the attributes, which should be freed with
 *          gck_attributes_unref()
 */
GckAttributes *
gck_builder_end (GckBuilder *builder)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckAttributes *attrs;
	gpointer data;
	gulong length;

	g_return_val_if_fail (builder != NULL, NULL);

	if (real->array) {
		length = real->array->len;
		data = g_array_free (real->array, FALSE);
		real->array = NULL;
	} else {
		length = 0;
		data = NULL;
	}

	attrs = g_new0 (GckAttributes, 1);
	attrs->count = length;
	attrs->data = data;
	attrs->refs = 1;

	return attrs;
}

/**
 * gck_builder_clear:
 * @builder: the builder
 *
 * Clear the builder and release all allocated memory. The builder may be used
 * again to build another set of attributes after this function call.
 *
 * If memory is shared between this builder and other attributes, then that
 * memory is only freed when both of them are cleared or unreferenced.
 */
void
gck_builder_clear (GckBuilder *builder)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckAttribute *attr;
	guint i;

	g_return_if_fail (builder != NULL);

	if (real->array == NULL)
		return;

	for (i = 0; i < real->array->len; i++) {
		attr = &g_array_index (real->array, GckAttribute, i);
		builder_clear (attr);
	}

	g_array_free (real->array, TRUE);
	real->array = NULL;
}

/**
 * gck_attribute_is_invalid:
 * @attr: The attribute to check.
 *
 * Check if the PKCS#11 attribute represents 'invalid' or 'not found'
 * according to the PKCS#11 spec. That is, having length
 * of (CK_ULONG)-1.
 *
 * Return value: Whether the attribute represents invalid or not.
 */
gboolean
gck_attribute_is_invalid (const GckAttribute *attr)
{
	g_return_val_if_fail (attr, TRUE);
	return attr->length == (gulong)-1;
}

/**
 * gck_attribute_get_boolean:
 * @attr: The attribute to retrieve value from.
 *
 * Get the CK_BBOOL of a PKCS#11 attribute. No conversion
 * is performed. It is an error to pass an attribute to this
 * function unless you're know it's supposed to contain a
 * boolean value.
 *
 * Return value: The boolean value of the attribute.
 */
gboolean
gck_attribute_get_boolean (const GckAttribute *attr)
{
	gboolean value;

	g_return_val_if_fail (attr, FALSE);
	if (gck_attribute_is_invalid (attr))
		return FALSE;
	if (!gck_value_to_boolean (attr->value, attr->length, &value))
		g_return_val_if_reached (FALSE);
	return value;
}

/**
 * gck_attribute_get_ulong:
 * @attr: The attribute to retrieve value from.
 *
 * Get the CK_ULONG value of a PKCS#11 attribute. No
 * conversion is performed. It is an error to pass an attribute
 * to this function unless you're know it's supposed to contain
 * a value of the right type.
 *
 * Return value: The ulong value of the attribute.
 */
gulong
gck_attribute_get_ulong (const GckAttribute *attr)
{
	gulong value;

	g_return_val_if_fail (attr, FALSE);
	if (gck_attribute_is_invalid (attr))
		return 0;
	if (!gck_value_to_ulong (attr->value, attr->length, &value))
		g_return_val_if_reached ((gulong)-1);
	return value;
}

/**
 * gck_attribute_get_string:
 * @attr: The attribute to retrieve value from.
 *
 * Get the string value of a PKCS#11 attribute. No
 * conversion is performed. It is an error to pass an attribute
 * to this function unless you're know it's supposed to contain
 * a value of the right type.
 *
 * Return value: (nullable): a null terminated string, to be freed with
 *               g_free(), or %NULL if the value was invalid
 */
gchar*
gck_attribute_get_string (const GckAttribute *attr)
{
	g_return_val_if_fail (attr, NULL);

	if (gck_attribute_is_invalid (attr))
		return NULL;
	if (!attr->value)
		return NULL;

	return g_strndup ((gchar*)attr->value, attr->length);
}

/**
 * gck_attribute_get_date:
 * @attr: The attribute to retrieve value from.
 * @value: The date value to fill in with the parsed date.
 *
 * Get the CK_DATE of a PKCS#11 attribute. No
 * conversion is performed. It is an error to pass an attribute
 * to this function unless you're know it's supposed to contain
 * a value of the right type.
 */
void
gck_attribute_get_date (const GckAttribute *attr,
                        GDate *value)
{
	guint year, month, day;
	gchar buffer[5];
	CK_DATE *date;
	gchar *end;

	g_return_if_fail (attr);

	if (gck_attribute_is_invalid (attr)) {
		g_date_clear (value, 1);
		return;
	}

	g_return_if_fail (attr->length == sizeof (CK_DATE));
	g_return_if_fail (attr->value);
	date = (CK_DATE*)attr->value;

	memset (&buffer, 0, sizeof (buffer));
	memcpy (buffer, date->year, 4);
	year = strtol (buffer, &end, 10);
	g_return_if_fail (end != buffer && !*end);

	memset (&buffer, 0, sizeof (buffer));
	memcpy (buffer, date->month, 2);
	month = strtol (buffer, &end, 10);
	g_return_if_fail (end != buffer && !*end);

	memset (&buffer, 0, sizeof (buffer));
	memcpy (buffer, date->day, 2);
	day = strtol (buffer, &end, 10);
	g_return_if_fail (end != buffer && !*end);

	g_date_set_dmy (value, day, month, year);
}

/**
 * gck_attribute_get_data:
 * @attr: an attribute
 * @length: the length of the returned data
 *
 * Get the raw value in the attribute.
 *
 * This is useful from scripting languages. C callers will generally
 * access the #GckAttribute struct directly.
 *
 * This function will %NULL if the attribute contains empty or invalid
 * data. The returned data must not be modified and is only valid
 * as long as this @attribute.
 *
 * Returns: (transfer none) (array length=length): the value data or %NULL
 */
const guchar *
gck_attribute_get_data (const GckAttribute *attr,
                        gsize *length)
{
	g_return_val_if_fail (attr != NULL, NULL);

	if (attr->length == G_MAXULONG) {
		*length = 0;
		return NULL;
	}
	*length = attr->length;
	return attr->value;
}

/**
 * gck_attribute_init: (skip)
 * @attr: an uninitialized attribute
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: (array length=length): The raw value of the attribute.
 * @length: The length of the raw value.
 *
 * Initialize a PKCS#11 attribute. This copies the value memory
 * into an internal buffer.
 *
 * When done with the attribute you should use [method@Attribute.clear]
 * to free the internal memory.
 **/
void
gck_attribute_init (GckAttribute *attr,
                    gulong attr_type,
                    const guchar *value,
                    gsize length)
{
	g_return_if_fail (attr != NULL);

	attr->type = attr_type;
	if (length == G_MAXULONG) {
		attr->value = NULL;
		attr->length = G_MAXULONG;
	} else if (value == NULL) {
		attr->value = NULL;
		attr->length = 0;
	} else {
		attr->value = value_new (value, length, egg_secure_check (value));
		attr->length = length;
	}
}

/**
 * gck_attribute_init_invalid: (skip)
 * @attr: an uninitialized attribute
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 *
 * Initialize a PKCS#11 attribute to an 'invalid' or 'not found'
 * state. Specifically this sets the value length to (CK_ULONG)-1
 * as specified in the PKCS#11 specification.
 *
 * When done with the attribute you should use [method@Attribute.clear]
 * to free the internal memory.
 **/
void
gck_attribute_init_invalid (GckAttribute *attr,
                            gulong attr_type)
{
	g_return_if_fail (attr != NULL);
	attr->type = attr_type;
	attr->value = NULL;
	attr->length = G_MAXULONG;
}

/**
 * gck_attribute_init_empty: (skip)
 * @attr: an uninitialized attribute
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 *
 * Initialize a PKCS#11 attribute to an empty state. The attribute
 * type will be set, but no data will be set.
 *
 * When done with the attribute you should use [method@Attribute.clear]
 * to free the internal memory.
 **/
void
gck_attribute_init_empty (GckAttribute *attr, gulong attr_type)
{
	g_return_if_fail (attr != NULL);

	attr->type = attr_type;
	attr->length = 0;
	attr->value = 0;
}

/**
 * gck_attribute_init_boolean: (skip)
 * @attr: an uninitialized attribute
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the boolean value of the attribute
 *
 * Initialize a PKCS#11 attribute to boolean. This will result
 * in a CK_BBOOL attribute from the PKCS#11 specs.
 *
 * When done with the attribute you should use [method@Attribute.clear]
 * to free the internal memory.
 **/
void
gck_attribute_init_boolean (GckAttribute *attr,
                            gulong attr_type,
                            gboolean value)
{
	CK_BBOOL val = value ? CK_TRUE : CK_FALSE;
	g_return_if_fail (attr != NULL);
	gck_attribute_init (attr, attr_type, &val, sizeof (val));
}

/**
 * gck_attribute_init_date: (skip)
 * @attr: an uninitialized attribute
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the date value of the attribute
 *
 * Initialize a PKCS#11 attribute to a date. This will result
 * in a CK_DATE attribute from the PKCS#11 specs.
 *
 * When done with the attribute you should use [method@Attribute.clear]
 * to free the internal memory.
 **/
void
gck_attribute_init_date (GckAttribute *attr,
                         gulong attr_type,
                         const GDate *value)
{
	CK_DATE date;

	g_return_if_fail (attr != NULL);
	g_return_if_fail (value != NULL);

	convert_gdate_to_ckdate (value, &date);
	gck_attribute_init (attr, attr_type, (const guchar *)&date, sizeof (CK_DATE));
}

/**
 * gck_attribute_init_ulong: (skip)
 * @attr: an uninitialized attribute
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the ulong value of the attribute
 *
 * Initialize a PKCS#11 attribute to a unsigned long. This will result
 * in a CK_ULONG attribute from the PKCS#11 specs.
 *
 * When done with the attribute you should use [method@Attribute.clear]
 * to free the internal memory.
 **/
void
gck_attribute_init_ulong (GckAttribute *attr,
                          gulong attr_type,
                          gulong value)
{
	CK_ULONG val = value;
	g_return_if_fail (attr != NULL);
	gck_attribute_init (attr, attr_type, (const guchar *)&val, sizeof (val));
}

/**
 * gck_attribute_init_string: (skip)
 * @attr: an uninitialized attribute
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the null terminated string value of the attribute
 *
 * Initialize a PKCS#11 attribute to a string. This will result
 * in an attribute containing the text, but not the null terminator.
 * The text in the attribute will be of the same encoding as you pass
 * to this function.
 *
 * When done with the attribute you should use [method@Attribute.clear]
 * to free the internal memory.
 **/
void
gck_attribute_init_string (GckAttribute *attr,
                           gulong attr_type,
                           const gchar *value)
{
	g_return_if_fail (attr != NULL);
	gck_attribute_init (attr, attr_type, (const guchar *)value,
	                    value ? strlen (value) : 0);
}

G_DEFINE_BOXED_TYPE (GckAttribute, gck_attribute,
                     gck_attribute_dup, gck_attribute_free)

/**
 * gck_attribute_new:
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the raw value of the attribute
 * @length: the length of the attribute
 *
 * Create a new PKCS#11 attribute. The value will be copied
 * into the new attribute.
 *
 * Returns: (transfer full): the new attribute; when done with the attribute
 *          use gck_attribute_free() to free it
 **/
GckAttribute *
gck_attribute_new (gulong attr_type,
                   const guchar *value,
                   gsize length)
{
	GckAttribute *attr = g_new0 (GckAttribute, 1);
	gck_attribute_init (attr, attr_type, value, length);
	return attr;
}

/**
 * gck_attribute_new_invalid:
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 *
 * Create a new PKCS#11 attribute as 'invalid' or 'not found'
 * state. Specifically this sets the value length to (CK_ULONG)-1
 * as specified in the PKCS#11 specification.
 *
 * Returns: (transfer full): the new attribute; when done with the attribute
 *          use gck_attribute_free() to free it
 **/
GckAttribute *
gck_attribute_new_invalid (gulong attr_type)
{
	GckAttribute *attr = g_new0 (GckAttribute, 1);
	gck_attribute_init_invalid (attr, attr_type);
	return attr;
}

/**
 * gck_attribute_new_empty:
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 *
 * Create a new PKCS#11 attribute with empty data.
 *
 * Returns: (transfer full): the new attribute; when done with the attribute
 *          use gck_attribute_free() to free it
 */
GckAttribute *
gck_attribute_new_empty (gulong attr_type)
{
	GckAttribute *attr = g_new0 (GckAttribute, 1);
	gck_attribute_init_empty (attr, attr_type);
	return attr;
}

/**
 * gck_attribute_new_boolean:
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the boolean value of the attribute
 *
 * Initialize a PKCS#11 attribute to boolean. This will result
 * in a CK_BBOOL attribute from the PKCS#11 specs.
 *
 * Returns: (transfer full): the new attribute; when done with the attribute u
 *          gck_attribute_free() to free it
 **/
GckAttribute *
gck_attribute_new_boolean (gulong attr_type,
                           gboolean value)
{
	GckAttribute *attr = g_new0 (GckAttribute, 1);
	gck_attribute_init_boolean (attr, attr_type, value);
	return attr;
}

/**
 * gck_attribute_new_date:
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the date value of the attribute
 *
 * Initialize a PKCS#11 attribute to a date. This will result
 * in a CK_DATE attribute from the PKCS#11 specs.
 *
 * Returns: (transfer full): the new attribute; when done with the attribute u
 *          gck_attribute_free() to free it
 **/
GckAttribute *
gck_attribute_new_date (gulong attr_type,
                        const GDate *value)
{
	GckAttribute *attr = g_new0 (GckAttribute, 1);
	gck_attribute_init_date (attr, attr_type, value);
	return attr;
}

/**
 * gck_attribute_new_ulong:
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the ulong value of the attribute
 *
 * Initialize a PKCS#11 attribute to a unsigned long. This will result
 * in a `CK_ULONG` attribute from the PKCS#11 specs.
 *
 * Returns: (transfer full): the new attribute; when done with the attribute u
 *          gck_attribute_free() to free it
 **/
GckAttribute *
gck_attribute_new_ulong (gulong attr_type,
                         gulong value)
{
	GckAttribute *attr = g_new0 (GckAttribute, 1);
	gck_attribute_init_ulong (attr, attr_type, value);
	return attr;
}

/**
 * gck_attribute_new_string:
 * @attr_type: the PKCS#11 attribute type to set on the attribute
 * @value: the null-terminated string value of the attribute
 *
 * Initialize a PKCS#11 attribute to a string. This will result
 * in an attribute containing the text, but not the null terminator.
 * The text in the attribute will be of the same encoding as you pass
 * to this function.
 *
 * Returns: (transfer full): the new attribute; when done with the attribute u
 *          gck_attribute_free() to free it
 **/
GckAttribute *
gck_attribute_new_string (gulong attr_type,
                          const gchar *value)
{
	GckAttribute *attr = g_new0 (GckAttribute, 1);
	gck_attribute_init_string (attr, attr_type, value);
	return attr;
}

/**
 * gck_attribute_dup:
 * @attr: the attribute to duplicate
 *
 * Duplicate the PKCS#11 attribute. All value memory is
 * also copied.
 *
 * The @attr must have been allocated or initialized by a Gck function or
 * the results of this function are undefined.
 *
 * Returns: (transfer full): the duplicated attribute; use gck_attribute_free()
 *          to free it
 */
GckAttribute *
gck_attribute_dup (const GckAttribute *attr)
{
	GckAttribute *copy;

	if (!attr)
		return NULL;

	copy = g_new0 (GckAttribute, 1);
	gck_attribute_init_copy (copy, attr);
	return copy;
}

/**
 * gck_attribute_init_copy:
 * @dest: An uninitialized attribute.
 * @src: An attribute to copy.
 *
 * Initialize a PKCS#11 attribute as a copy of another attribute.
 * This copies the value memory as well.
 *
 * When done with the copied attribute you should use
 * [method@Attribute.clear] to free the internal memory.
 */
void
gck_attribute_init_copy (GckAttribute *dest,
                         const GckAttribute *src)
{
	g_return_if_fail (dest != NULL);
	g_return_if_fail (src != NULL);

	dest->type = src->type;
	if (src->length == G_MAXULONG) {
		dest->value = NULL;
		dest->length = G_MAXULONG;
	} else if (src->value == NULL) {
		dest->value = NULL;
		dest->length = 0;
	} else {
		dest->value = value_ref (src->value);
		dest->length = src->length;
	}
}

/**
 * gck_attribute_clear:
 * @attr: Attribute to clear.
 *
 * Clear allocated memory held by a #GckAttribute.
 *
 * This attribute must have been allocated by a Gck library function, or
 * the results of this method are undefined.
 *
 * The type of the attribute will remain set.
 **/
void
gck_attribute_clear (GckAttribute *attr)
{
	g_return_if_fail (attr != NULL);

	if (attr->value != NULL)
		value_unref (attr->value);
	attr->value = NULL;
	attr->length = 0;
}

/**
 * gck_attribute_free:
 * @attr: (type Gck.Attribute): attribute to free
 *
 * Free an attribute and its allocated memory. These is usually
 * used with attributes that are allocated by [ctor@Attribute.new]
 * or a similar function.
 **/
void
gck_attribute_free (gpointer attr)
{
	GckAttribute *a = attr;
	if (attr) {
		gck_attribute_clear (a);
		g_free (a);
	}
}

/**
 * gck_attribute_equal:
 * @attr1: (type Gck.Attribute): first attribute to compare
 * @attr2: (type Gck.Attribute): second attribute to compare
 *
 * Compare two attributes. Useful with <code>GHashTable</code>.
 *
 * Returns: %TRUE if the attributes are equal.
 */
gboolean
gck_attribute_equal (gconstpointer attr1,
                     gconstpointer attr2)
{
	const GckAttribute *aa = attr1;
	const GckAttribute *ab = attr2;

	if (!aa && !ab)
		return TRUE;
	if (!aa || !ab)
		return FALSE;

	if (aa->type != ab->type)
		return FALSE;
	if (aa->length != ab->length)
		return FALSE;
	if (!aa->value && !ab->value)
		return TRUE;
	if (!aa->value || !ab->value)
		return FALSE;
	return memcmp (aa->value, ab->value, aa->length) == 0;
}

/**
 * gck_attribute_hash:
 * @attr: (type Gck.Attribute): attribute to hash
 *
 * Hash an attribute for use in <code>GHashTable</code> keys.
 *
 * Returns: the hash code
 */
guint
gck_attribute_hash (gconstpointer attr)
{
	const GckAttribute *a = attr;
	const signed char *p, *e;
	guint32 h = 5381;

	h ^= _gck_ulong_hash (&a->type);

	if (a->value) {
		for (p = (signed char *)a->value, e = p + a->length; p != e; p++)
			h = (h << 5) + h + *p;
	}

	return h;
}

/**
 * GckAttributes:
 *
 * A set of [struct@Attribute] structures.
 *
 * These attributes contain information about a PKCS11 object. Use
 * [method@Object.get] or [method@Object.set] to set and retrieve attributes on
 * an object.
 */

/**
 * GckAllocator:
 * @data: Memory to allocate or deallocate.
 * @length: New length of memory.
 *
 * An allocator used to allocate data for the attributes in this
 * [struct@Attributes] set.
 *
 * This is a function that acts like g_realloc. Specifically it frees when length is
 * set to zero, it allocates when data is set to %NULL, and it reallocates when both
 * are valid.
 *
 * Returns: The allocated memory, or %NULL when freeing.
 **/

G_DEFINE_BOXED_TYPE (GckAttributes, gck_attributes,
                     gck_attributes_ref, gck_attributes_unref)

/**
 * gck_attributes_new_empty:
 * @first_type: the first empty attribute type
 * @...: the other empty attribute types
 *
 * Creates an GckAttributes array with empty attributes
 *
 * Terminate the argument list with [const@INVALID].
 *
 * Returns: (transfer full): a reference to an empty set of attributes
 **/
GckAttributes *
gck_attributes_new_empty (gulong first_type,
                          ...)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	va_list va;

	va_start (va, first_type);

	while (first_type != GCK_INVALID) {
		gck_builder_add_empty (&builder, first_type);
		first_type = va_arg (va, gulong);
	}

	va_end (va);
	return gck_builder_end (&builder);
}

/**
 * gck_attributes_at:
 * @attrs: The attributes array.
 * @index: The attribute index to retrieve.
 *
 * Get attribute at the specified index in the attribute array.
 *
 * Use [method@Attributes.count] to determine how many attributes are
 * in the array.
 *
 * Returns: (transfer none): the specified attribute
 **/
const GckAttribute *
gck_attributes_at (GckAttributes *attrs,
                   guint index)
{
	g_return_val_if_fail (attrs != NULL, NULL);
	g_return_val_if_fail (index < attrs->count, NULL);
	return attrs->data + index;
}

/**
 * gck_attributes_count:
 * @attrs: The attributes array to count.
 *
 * Get the number of attributes in this attribute array.
 *
 * Return value: The number of contained attributes.
 **/
gulong
gck_attributes_count (GckAttributes *attrs)
{
	g_return_val_if_fail (attrs != NULL, 0);
	return attrs->count;
}

/**
 * gck_attributes_find:
 * @attrs: The attributes array to search.
 * @attr_type: The type of attribute to find.
 *
 * Find an attribute with the specified type in the array.
 *
 * Returns: (transfer none): the first attribute found with the specified type,
 *          or %NULL
 **/
const GckAttribute *
gck_attributes_find (GckAttributes *attrs,
                     gulong attr_type)
{
	g_return_val_if_fail (attrs != NULL, NULL);

	return find_attribute (attrs->data, attrs->count, attr_type);
}

/**
 * gck_attributes_find_boolean:
 * @attrs: The attributes array to search.
 * @attr_type: The type of attribute to find.
 * @value: (out): The resulting gboolean value.
 *
 * Find an attribute with the specified type in the array.
 *
 * The attribute (if found) must be of the right size to store
 * a boolean value (ie: CK_BBOOL). If the attribute is marked invalid
 * then it will be treated as not found.
 *
 * Return value: Whether a value was found or not.
 **/
gboolean
gck_attributes_find_boolean (GckAttributes *attrs, gulong attr_type, gboolean *value)
{
	g_return_val_if_fail (attrs != NULL, FALSE);
	g_return_val_if_fail (value, FALSE);

	return find_attribute_boolean (attrs->data, attrs->count, attr_type, value);
}

/**
 * gck_attributes_find_ulong:
 * @attrs: The attributes array to search.
 * @attr_type: The type of attribute to find.
 * @value: (out): The resulting gulong value.
 *
 * Find an attribute with the specified type in the array.
 *
 * The attribute (if found) must be of the right size to store
 * a unsigned long value (ie: CK_ULONG). If the attribute is marked invalid
 * then it will be treated as not found.
 *
 * Return value: Whether a value was found or not.
 **/
gboolean
gck_attributes_find_ulong (GckAttributes *attrs, gulong attr_type, gulong *value)
{
	g_return_val_if_fail (attrs != NULL, FALSE);
	g_return_val_if_fail (value, FALSE);

	return find_attribute_ulong (attrs->data, attrs->count, attr_type, value);
}

/**
 * gck_attributes_find_string:
 * @attrs: The attributes array to search.
 * @attr_type: The type of attribute to find.
 * @value: (out): The resulting string value.
 *
 * Find an attribute with the specified type in the array.
 *
 * If the attribute is marked invalid then it will be treated as not found.
 * The resulting string will be null-terminated, and must be freed by the caller
 * using g_free().
 *
 * Return value: Whether a value was found or not.
 **/
gboolean
gck_attributes_find_string (GckAttributes *attrs, gulong attr_type, gchar **value)
{
	g_return_val_if_fail (attrs != NULL, FALSE);
	g_return_val_if_fail (value, FALSE);

	return find_attribute_string (attrs->data, attrs->count, attr_type, value);
}

/**
 * gck_attributes_find_date:
 * @attrs: The attributes array to search.
 * @attr_type: The type of attribute to find.
 * @value: (out): The resulting GDate value.
 *
 * Find an attribute with the specified type in the array.
 *
 * The attribute (if found) must be of the right size to store
 * a date value (ie: CK_DATE). If the attribute is marked invalid
 * then it will be treated as not found.
 *
 * Return value: Whether a value was found or not.
 **/
gboolean
gck_attributes_find_date (GckAttributes *attrs, gulong attr_type, GDate *value)
{
	g_return_val_if_fail (attrs != NULL, FALSE);
	g_return_val_if_fail (value, FALSE);

	return find_attribute_date (attrs->data, attrs->count, attr_type, value);
}

/**
 * gck_attributes_ref:
 * @attrs: An attribute array
 *
 * Reference this attributes array.
 *
 * Returns: (transfer full): the attributes
 **/
GckAttributes *
gck_attributes_ref (GckAttributes *attrs)
{
	g_return_val_if_fail (attrs, NULL);
	g_atomic_int_inc (&attrs->refs);
	return attrs;
}

/**
 * gck_attributes_unref:
 * @attrs: (nullable) (type Gck.Attributes) (transfer full): An attribute array
 *
 * Unreference this attribute array.
 *
 * When all outstanding references are gone, the array will be freed.
 */
void
gck_attributes_unref (gpointer attrs)
{
	GckAttributes *attrs_ = attrs;
	const GckAttribute *attr;
	guint i;

	if (!attrs_)
		return;

	if (g_atomic_int_dec_and_test (&attrs_->refs)) {
		for (i = 0; i < attrs_->count; ++i) {
			attr = gck_attributes_at (attrs_, i);
			if (attr->value)
				value_unref (attr->value);
		}
		g_free (attrs_->data);
		g_free (attrs_);
	}
}

/**
 * gck_attributes_contains:
 * @attrs: The attributes to check
 * @match: The attribute to find
 *
 * Check whether the attributes contain a certain attribute.
 *
 * Returns: %TRUE if the attributes contain the attribute.
 */
gboolean
gck_attributes_contains (GckAttributes *attrs,
                         const GckAttribute *match)
{
	const GckAttribute *attr;
	guint i;

	g_return_val_if_fail (attrs != NULL, FALSE);

	for (i = 0; i < attrs->count; ++i) {
		attr = gck_attributes_at (attrs, i);
		if (gck_attribute_equal (attr, match))
			return TRUE;
	}

	return FALSE;
}

CK_ATTRIBUTE_PTR
_gck_builder_prepare_in (GckBuilder *builder,
                         CK_ULONG_PTR n_attrs)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckAttribute *attr;
	guint i;

	g_return_val_if_fail (builder != NULL, NULL);
	g_return_val_if_fail (n_attrs != NULL, NULL);

	if (real->array == NULL) {
		*n_attrs = 0;
		return NULL;
	}

	/* Prepare the attributes to receive their length */

	for (i = 0; i < real->array->len; ++i) {
		attr = &g_array_index (real->array, GckAttribute, i);
		if (attr->value != NULL) {
			value_unref (attr->value);
			attr->value = NULL;
		}
		attr->length = 0;
	}

	*n_attrs = real->array->len;
	return (CK_ATTRIBUTE_PTR)real->array->data;
}

CK_ATTRIBUTE_PTR
_gck_builder_commit_in (GckBuilder *builder,
                        CK_ULONG_PTR n_attrs)
{
	GckRealBuilder *real = (GckRealBuilder *)builder;
	GckAttribute *attr;
	guint i;

	g_return_val_if_fail (builder != NULL, NULL);
	g_return_val_if_fail (n_attrs != NULL, NULL);

	if (real->array == NULL) {
		*n_attrs = 0;
		return NULL;
	}

	/* Allocate each attribute with the length that was set */

	for (i = 0; i < real->array->len; ++i) {
		attr = &g_array_index (real->array, GckAttribute, i);
		if (attr->length != 0 && attr->length != (gulong)-1)
			attr->value = value_blank (attr->length, real->secure);
		else
			attr->value = NULL;
	}

	*n_attrs = real->array->len;
	return (CK_ATTRIBUTE_PTR)real->array->data;
}

CK_ATTRIBUTE_PTR
_gck_attributes_commit_out (GckAttributes *attrs,
                            CK_ULONG_PTR n_attrs)
{
	g_return_val_if_fail (attrs != NULL, NULL);
	g_return_val_if_fail (n_attrs != NULL, NULL);

	*n_attrs = attrs->count;
	return (CK_ATTRIBUTE_PTR)attrs->data;
}

static gboolean
_gck_attribute_is_ulong_of_type (GckAttribute *attr,
                                 gulong attr_type)
{
	if (attr->type != attr_type)
		return FALSE;
	if (attr->length != sizeof (gulong))
		return FALSE;
	if (!attr->value)
		return FALSE;
	return TRUE;
}

static gboolean
_gck_attribute_is_sensitive (GckAttribute *attr)
{
	/*
	 * Don't print any just attribute, since they may contain
	 * sensitive data
	 */

	switch (attr->type) {
	#define X(x) case x: return FALSE;
	X (CKA_CLASS)
	X (CKA_TOKEN)
	X (CKA_PRIVATE)
	X (CKA_LABEL)
	X (CKA_APPLICATION)
	X (CKA_OBJECT_ID)
	X (CKA_CERTIFICATE_TYPE)
	X (CKA_ISSUER)
	X (CKA_SERIAL_NUMBER)
	X (CKA_AC_ISSUER)
	X (CKA_OWNER)
	X (CKA_ATTR_TYPES)
	X (CKA_TRUSTED)
	X (CKA_CERTIFICATE_CATEGORY)
	X (CKA_JAVA_MIDP_SECURITY_DOMAIN)
	X (CKA_URL)
	X (CKA_HASH_OF_SUBJECT_PUBLIC_KEY)
	X (CKA_HASH_OF_ISSUER_PUBLIC_KEY)
	X (CKA_CHECK_VALUE)
	X (CKA_KEY_TYPE)
	X (CKA_SUBJECT)
	X (CKA_ID)
	X (CKA_SENSITIVE)
	X (CKA_ENCRYPT)
	X (CKA_DECRYPT)
	X (CKA_WRAP)
	X (CKA_UNWRAP)
	X (CKA_SIGN)
	X (CKA_SIGN_RECOVER)
	X (CKA_VERIFY)
	X (CKA_VERIFY_RECOVER)
	X (CKA_DERIVE)
	X (CKA_START_DATE)
	X (CKA_END_DATE)
	X (CKA_MODULUS_BITS)
	X (CKA_PRIME_BITS)
	/* X (CKA_SUBPRIME_BITS) */
	/* X (CKA_SUB_PRIME_BITS) */
	X (CKA_VALUE_BITS)
	X (CKA_VALUE_LEN)
	X (CKA_EXTRACTABLE)
	X (CKA_LOCAL)
	X (CKA_NEVER_EXTRACTABLE)
	X (CKA_ALWAYS_SENSITIVE)
	X (CKA_KEY_GEN_MECHANISM)
	X (CKA_MODIFIABLE)
	X (CKA_SECONDARY_AUTH)
	X (CKA_AUTH_PIN_FLAGS)
	X (CKA_ALWAYS_AUTHENTICATE)
	X (CKA_WRAP_WITH_TRUSTED)
	X (CKA_WRAP_TEMPLATE)
	X (CKA_UNWRAP_TEMPLATE)
	X (CKA_HW_FEATURE_TYPE)
	X (CKA_RESET_ON_INIT)
	X (CKA_HAS_RESET)
	X (CKA_PIXEL_X)
	X (CKA_PIXEL_Y)
	X (CKA_RESOLUTION)
	X (CKA_CHAR_ROWS)
	X (CKA_CHAR_COLUMNS)
	X (CKA_COLOR)
	X (CKA_BITS_PER_PIXEL)
	X (CKA_CHAR_SETS)
	X (CKA_ENCODING_METHODS)
	X (CKA_MIME_TYPES)
	X (CKA_MECHANISM_TYPE)
	X (CKA_REQUIRED_CMS_ATTRIBUTES)
	X (CKA_DEFAULT_CMS_ATTRIBUTES)
	X (CKA_SUPPORTED_CMS_ATTRIBUTES)
	X (CKA_ALLOWED_MECHANISMS)
	X (CKA_X_ASSERTION_TYPE)
	X (CKA_X_CERTIFICATE_VALUE)
	X (CKA_X_PURPOSE)
	X (CKA_X_PEER)
	#undef X
	}

	return TRUE;
}

static void
_gck_format_class (GString *output,
                   CK_OBJECT_CLASS klass)
{
	const gchar *string = NULL;

	switch (klass) {
	#define X(x) case x: string = #x; break;
	X (CKO_DATA)
	X (CKO_CERTIFICATE)
	X (CKO_PUBLIC_KEY)
	X (CKO_PRIVATE_KEY)
	X (CKO_SECRET_KEY)
	X (CKO_HW_FEATURE)
	X (CKO_DOMAIN_PARAMETERS)
	X (CKO_MECHANISM)
	X (CKO_X_TRUST_ASSERTION)
	}

	if (string != NULL)
		g_string_append (output, string);
	else
		g_string_append_printf (output, "0x%08lX", klass);
}

static void
_gck_format_assertion_type (GString *output,
                            CK_X_ASSERTION_TYPE type)
{
	const gchar *string = NULL;

	switch (type) {
	#define X(x) case x: string = #x; break;
	X (CKT_X_UNTRUSTED_CERTIFICATE)
	X (CKT_X_PINNED_CERTIFICATE)
	X (CKT_X_ANCHORED_CERTIFICATE)
	#undef X
	}

	if (string != NULL)
		g_string_append (output, string);
	else
		g_string_append_printf (output, "0x%08lX", type);
}

static void
_gck_format_key_type (GString *output,
                      CK_KEY_TYPE type)
{
	const gchar *string = NULL;

	switch (type) {
	#define X(x) case x: string = #x; break;
	X (CKK_RSA)
	X (CKK_DSA)
	X (CKK_DH)
	/* X (CKK_ECDSA) */
	X (CKK_EC)
	X (CKK_X9_42_DH)
	X (CKK_KEA)
	X (CKK_GENERIC_SECRET)
	X (CKK_RC2)
	X (CKK_RC4)
	X (CKK_DES)
	X (CKK_DES2)
	X (CKK_DES3)
	X (CKK_CAST)
	X (CKK_CAST3)
	X (CKK_CAST128)
	X (CKK_RC5)
	X (CKK_IDEA)
	X (CKK_SKIPJACK)
	X (CKK_BATON)
	X (CKK_JUNIPER)
	X (CKK_CDMF)
	X (CKK_AES)
	X (CKK_BLOWFISH)
	X (CKK_TWOFISH)
	#undef X
	}

	if (string != NULL)
		g_string_append (output, string);
	else
		g_string_append_printf (output, "0x%08lX", type);
}

static void
_gck_format_certificate_type (GString *output,
                              CK_CERTIFICATE_TYPE type)
{
	const gchar *string = NULL;

	switch (type) {
	#define X(x) case x: string = #x; break;
	X (CKC_X_509)
	X (CKC_X_509_ATTR_CERT)
	X (CKC_WTLS)
	}

	if (string != NULL)
		g_string_append (output, string);
	else
		g_string_append_printf (output, "0x%08lX", type);
}

static void
_gck_format_attribute_type (GString *output,
                            gulong type)
{
	const gchar *string = NULL;

	switch (type) {
	#define X(x) case x: string = #x; break;
	X (CKA_CLASS)
	X (CKA_TOKEN)
	X (CKA_PRIVATE)
	X (CKA_LABEL)
	X (CKA_APPLICATION)
	X (CKA_VALUE)
	X (CKA_OBJECT_ID)
	X (CKA_CERTIFICATE_TYPE)
	X (CKA_ISSUER)
	X (CKA_SERIAL_NUMBER)
	X (CKA_AC_ISSUER)
	X (CKA_OWNER)
	X (CKA_ATTR_TYPES)
	X (CKA_TRUSTED)
	X (CKA_CERTIFICATE_CATEGORY)
	X (CKA_JAVA_MIDP_SECURITY_DOMAIN)
	X (CKA_URL)
	X (CKA_HASH_OF_SUBJECT_PUBLIC_KEY)
	X (CKA_HASH_OF_ISSUER_PUBLIC_KEY)
	X (CKA_CHECK_VALUE)
	X (CKA_KEY_TYPE)
	X (CKA_SUBJECT)
	X (CKA_ID)
	X (CKA_SENSITIVE)
	X (CKA_ENCRYPT)
	X (CKA_DECRYPT)
	X (CKA_WRAP)
	X (CKA_UNWRAP)
	X (CKA_SIGN)
	X (CKA_SIGN_RECOVER)
	X (CKA_VERIFY)
	X (CKA_VERIFY_RECOVER)
	X (CKA_DERIVE)
	X (CKA_START_DATE)
	X (CKA_END_DATE)
	X (CKA_MODULUS)
	X (CKA_MODULUS_BITS)
	X (CKA_PUBLIC_EXPONENT)
	X (CKA_PRIVATE_EXPONENT)
	X (CKA_PRIME_1)
	X (CKA_PRIME_2)
	X (CKA_EXPONENT_1)
	X (CKA_EXPONENT_2)
	X (CKA_COEFFICIENT)
	X (CKA_PRIME)
	X (CKA_SUBPRIME)
	X (CKA_BASE)
	X (CKA_PRIME_BITS)
	/* X (CKA_SUBPRIME_BITS) */
	/* X (CKA_SUB_PRIME_BITS) */
	X (CKA_VALUE_BITS)
	X (CKA_VALUE_LEN)
	X (CKA_EXTRACTABLE)
	X (CKA_LOCAL)
	X (CKA_NEVER_EXTRACTABLE)
	X (CKA_ALWAYS_SENSITIVE)
	X (CKA_KEY_GEN_MECHANISM)
	X (CKA_MODIFIABLE)
	X (CKA_ECDSA_PARAMS)
	/* X (CKA_EC_PARAMS) */
	X (CKA_EC_POINT)
	X (CKA_SECONDARY_AUTH)
	X (CKA_AUTH_PIN_FLAGS)
	X (CKA_ALWAYS_AUTHENTICATE)
	X (CKA_WRAP_WITH_TRUSTED)
	X (CKA_WRAP_TEMPLATE)
	X (CKA_UNWRAP_TEMPLATE)
	X (CKA_HW_FEATURE_TYPE)
	X (CKA_RESET_ON_INIT)
	X (CKA_HAS_RESET)
	X (CKA_PIXEL_X)
	X (CKA_PIXEL_Y)
	X (CKA_RESOLUTION)
	X (CKA_CHAR_ROWS)
	X (CKA_CHAR_COLUMNS)
	X (CKA_COLOR)
	X (CKA_BITS_PER_PIXEL)
	X (CKA_CHAR_SETS)
	X (CKA_ENCODING_METHODS)
	X (CKA_MIME_TYPES)
	X (CKA_MECHANISM_TYPE)
	X (CKA_REQUIRED_CMS_ATTRIBUTES)
	X (CKA_DEFAULT_CMS_ATTRIBUTES)
	X (CKA_SUPPORTED_CMS_ATTRIBUTES)
	X (CKA_ALLOWED_MECHANISMS)
	X (CKA_X_ASSERTION_TYPE)
	X (CKA_X_CERTIFICATE_VALUE)
	X (CKA_X_PURPOSE)
	X (CKA_X_PEER)
	#undef X
	}

	if (string != NULL)
		g_string_append (output, string);
	else
		g_string_append_printf (output, "CKA_0x%08lX", type);
}

static void
_gck_format_some_bytes (GString *output,
                        gconstpointer bytes,
                        gulong length)
{
	guchar ch;
	const guchar *data = bytes;
	gulong i;

	if (bytes == NULL) {
		g_string_append (output, "NULL");
		return;
	}

	g_string_append_c (output, '\"');
	for (i = 0; i < length && i < 128; i++) {
		ch = data[i];
		if (ch == '\t')
			g_string_append (output, "\\t");
		else if (ch == '\n')
			g_string_append (output, "\\n");
		else if (ch == '\r')
			g_string_append (output, "\\r");
		else if (ch >= 32 && ch < 127)
			g_string_append_c (output, ch);
		else
			g_string_append_printf (output, "\\x%02x", ch);
	}

	if (i < length)
		g_string_append_printf (output, "...");
	g_string_append_c (output, '\"');
}

static void
_gck_format_attributes (GString *output,
                        GckAttributes *attrs)
{
	GckAttribute *attr;
	guint count, i;

	count = attrs->count;
	g_string_append_printf (output, "(%d) [", count);
	for (i = 0; i < count; i++) {
		attr = attrs->data + i;
		if (i > 0)
			g_string_append_c (output, ',');
		g_string_append (output, " { ");
		_gck_format_attribute_type (output, attr->type);
		g_string_append (output, " = ");
		if (attr->length == GCK_INVALID) {
			g_string_append_printf (output, " (-1) INVALID");
		} else if (_gck_attribute_is_ulong_of_type (attr, CKA_CLASS)) {
			_gck_format_class (output, (CK_OBJECT_CLASS) gck_attribute_get_ulong(attr));
		} else if (_gck_attribute_is_ulong_of_type (attr, CKA_X_ASSERTION_TYPE)) {
			_gck_format_assertion_type (output, (CK_X_ASSERTION_TYPE) gck_attribute_get_ulong(attr));
		} else if (_gck_attribute_is_ulong_of_type (attr, CKA_CERTIFICATE_TYPE)) {
			_gck_format_certificate_type (output, (CK_CERTIFICATE_TYPE) gck_attribute_get_ulong(attr));
		} else if (_gck_attribute_is_ulong_of_type (attr, CKA_KEY_TYPE)) {
			_gck_format_key_type (output, (CK_KEY_TYPE) gck_attribute_get_ulong(attr));
		} else if (_gck_attribute_is_sensitive (attr)) {
			g_string_append_printf (output, " (%lu) NOT-PRINTED", attr->length);
		} else {
			g_string_append_printf (output, " (%lu) ", attr->length);
			_gck_format_some_bytes (output, attr->value, attr->length);
		}
		g_string_append (output, " }");
	}
	g_string_append (output, " ]");
}

/**
 * gck_attributes_to_string:
 * @attrs: the attributes
 *
 * Print out attributes to a string in aform that's useful for debugging
 * or logging.
 *
 * The format of the string returned may change in the future.
 *
 * Returns: a newly allocated string
 */
gchar *
gck_attributes_to_string (GckAttributes *attrs)
{
	GString *output = g_string_sized_new (128);
	_gck_format_attributes (output, attrs);
	return g_string_free (output, FALSE);
}

/**
 * gck_attributes_new:
 *
 * Create a new empty `GckAttributes` array.
 *
 * Returns: (transfer full): a reference to the new attributes array;
 *          when done with the array release it with gck_attributes_unref().
 **/
GckAttributes *
gck_attributes_new (void)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	return gck_builder_end (&builder);
}
