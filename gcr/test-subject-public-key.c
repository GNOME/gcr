/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
   Copyright (C) 2010 Collabora Ltd

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

#include "gcr/gcr.h"
#include "gcr/gcr-subject-public-key.h"

#include "gck/gck-mock.h"
#include "gck/gck-test.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-testing.h"

#include <glib.h>

#include <errno.h>

typedef struct {
	const gchar *name;
	const gchar *basename;
	guint key_size;
} TestFixture;

typedef struct {
	GBytes *crt_data;
	GckAttributes *crt_attrs;
	GBytes *key_data;
	GckAttributes *prv_attrs;
	GBytes *spk_data;
	GckAttributes *pub_attrs;
} TestAttributes;

static void
on_parser_parsed (GcrParser *parser,
                  gpointer user_data)
{
	GckAttributes **attrs = user_data;
	g_assert (*attrs == NULL);
	*attrs = gcr_parser_get_parsed_attributes (parser);
	g_assert (*attrs != NULL);
	gck_attributes_ref (*attrs);
}

static GckAttributes *
parse_attributes (GBytes *data,
                  GcrDataFormat format)
{
	GcrParser *parser;
	GckAttributes *attrs = NULL;
	GError *error = NULL;

	parser = gcr_parser_new ();
	gcr_parser_format_disable (parser, GCR_FORMAT_ALL);
	gcr_parser_format_enable (parser, format);
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), &attrs);
	gcr_parser_parse_bytes (parser, data, &error);
	g_assert_no_error (error);
	g_object_unref (parser);

	g_assert (attrs);
	return attrs;
}

static void
setup_attributes (TestAttributes *test,
                  gconstpointer data)
{
	const TestFixture *fixture = data;
	GError *error = NULL;
	gchar *contents;
	gchar *filename;
	gsize length;
	gulong klass;

	filename = g_strdup_printf (SRCDIR "/gcr/fixtures/%s.crt", fixture->basename);
	g_file_get_contents (filename, &contents, &length, &error);
	g_assert_no_error (error);
	test->crt_data = g_bytes_new_take (contents, length);
	test->crt_attrs = parse_attributes (test->crt_data, GCR_FORMAT_DER_CERTIFICATE_X509);
	g_assert (gck_attributes_find_ulong (test->crt_attrs, CKA_CLASS, &klass));
	gck_assert_cmpulong (klass, ==, CKO_CERTIFICATE);
	g_free (filename);

	filename = g_strdup_printf (SRCDIR "/gcr/fixtures/%s.key", fixture->basename);
	g_file_get_contents (filename, &contents, &length, &error);
	g_assert_no_error (error);
	test->key_data = g_bytes_new_take (contents, length);
	test->prv_attrs = parse_attributes (test->key_data, GCR_FORMAT_ALL);
	g_assert (gck_attributes_find_ulong (test->prv_attrs, CKA_CLASS, &klass));
	gck_assert_cmpulong (klass, ==, CKO_PRIVATE_KEY);
	g_free (filename);

	filename = g_strdup_printf (SRCDIR "/gcr/fixtures/%s.spk", fixture->basename);
	g_file_get_contents (filename, &contents, &length, &error);
	g_assert_no_error (error);
	test->spk_data = g_bytes_new_take (contents, length);
	test->pub_attrs = parse_attributes (test->spk_data, GCR_FORMAT_DER_SUBJECT_PUBLIC_KEY);
	g_assert (gck_attributes_find_ulong (test->pub_attrs, CKA_CLASS, &klass));
	gck_assert_cmpulong (klass, ==, CKO_PUBLIC_KEY);
	g_free (filename);
}

static void
teardown_attributes (TestAttributes *test,
                     gconstpointer unused)
{
	g_bytes_unref (test->crt_data);
	g_bytes_unref (test->key_data);
	g_bytes_unref (test->spk_data);
	gck_attributes_unref (test->crt_attrs);
	gck_attributes_unref (test->prv_attrs);
	gck_attributes_unref (test->pub_attrs);
}

static void
perform_for_attributes (TestAttributes *test,
                        GckAttributes *attrs)
{
	GNode *info;
	GBytes *data;

	info = _gcr_subject_public_key_for_attributes (attrs);
	g_assert (info != NULL);

	data = egg_asn1x_encode (info, NULL);
	egg_assert_cmpbytes (data, ==, g_bytes_get_data (test->spk_data, NULL),
	                               g_bytes_get_size (test->spk_data));

	g_bytes_unref (data);
	egg_asn1x_destroy (info);

}

static void
test_for_cert_attributes (TestAttributes *test,
                          gconstpointer unused)
{
	perform_for_attributes (test, test->crt_attrs);
}

static void
test_for_private_key_attributes (TestAttributes *test,
                                 gconstpointer unused)
{
	perform_for_attributes (test, test->prv_attrs);
}

static void
test_for_public_key_attributes (TestAttributes *test,
                                gconstpointer unused)
{
	perform_for_attributes (test, test->pub_attrs);
}

static void
perform_calculate_size (TestAttributes *test,
                        GckAttributes *attrs,
                        const TestFixture *fixture)
{
	GNode *info;
	guint size;

	info = _gcr_subject_public_key_for_attributes (attrs);
	g_assert (info != NULL);

	/* TODO: until encoding, we don't have readable attributes */
	g_bytes_unref (egg_asn1x_encode (info, NULL));

	size = _gcr_subject_public_key_calculate_size (info);
	g_assert_cmpuint (size, ==, fixture->key_size);

	egg_asn1x_destroy (info);

}

static void
test_certificate_calculate_size (TestAttributes *test,
                                 gconstpointer fixture)
{
	perform_calculate_size (test, test->crt_attrs, fixture);
}

static void
test_public_key_calculate_size (TestAttributes *test,
                                gconstpointer fixture)
{
	perform_calculate_size (test, test->pub_attrs, fixture);
}

static void
test_private_key_calculate_size (TestAttributes *test,
                                 gconstpointer fixture)
{
	perform_calculate_size (test, test->prv_attrs, fixture);
}

typedef struct {
	CK_FUNCTION_LIST funcs;
	GckModule *module;
	GckSession *session;
} TestModule;

static void
setup_module (TestModule *test,
               gconstpointer unused)
{
	CK_FUNCTION_LIST_PTR f;
	GError *error = NULL;
	GckSlot *slot;
	CK_RV rv;

	rv = gck_mock_C_GetFunctionList (&f);
	gck_assert_cmprv (rv, ==, CKR_OK);
	memcpy (&test->funcs, f, sizeof (test->funcs));

	/* Open a session */
	rv = (test->funcs.C_Initialize) (NULL);
	gck_assert_cmprv (rv, ==, CKR_OK);

	test->module = gck_module_new (&test->funcs);
	g_object_add_weak_pointer (G_OBJECT (test->module), (gpointer *)&test->module);

	slot = gck_slot_from_handle (test->module, GCK_MOCK_SLOT_ONE_ID);
	test->session = gck_session_open (slot, GCK_SESSION_READ_ONLY, NULL, NULL, &error);
	g_assert_no_error (error);
	g_object_add_weak_pointer (G_OBJECT (test->session), (gpointer *)&test->session);

	g_object_unref (slot);
}

static void
teardown_module (TestModule *test,
                 gconstpointer fixture)
{
	CK_RV rv;

	g_object_unref (test->session);
	g_object_unref (test->module);

	egg_test_wait_for_gtask_thread (test->session || test->module);

	g_assert (test->session == NULL);
	g_assert (test->module == NULL);

	rv = (test->funcs.C_Finalize) (NULL);
	gck_assert_cmprv (rv, ==, CKR_OK);
}

typedef struct {
	TestAttributes at;
	TestModule mo;
	GckObject *crt_object;
	GckObject *pub_object;
	GckObject *prv_object;
} TestLoading;

static void
setup_loading (TestLoading *test,
               gconstpointer fixture)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	const gchar *id = "test-id";
	gulong handle;

	setup_attributes (&test->at, fixture);
	setup_module (&test->mo, NULL);

	gck_builder_add_all (&builder, test->at.crt_attrs);
	gck_builder_add_string (&builder, CKA_ID, id);
	handle = gck_mock_module_add_object (gck_builder_end (&builder));
	test->crt_object = gck_object_from_handle (test->mo.session, handle);
	g_object_add_weak_pointer (G_OBJECT (test->crt_object), (gpointer *)&test->crt_object);

	gck_builder_add_all (&builder, test->at.pub_attrs);
	gck_builder_add_string (&builder, CKA_ID, id);
	handle = gck_mock_module_add_object (gck_builder_end (&builder));
	test->pub_object = gck_object_from_handle (test->mo.session, handle);
	g_object_add_weak_pointer (G_OBJECT (test->pub_object), (gpointer *)&test->pub_object);

	gck_builder_add_all (&builder, test->at.prv_attrs);
	gck_builder_add_string (&builder, CKA_ID, id);
	handle = gck_mock_module_add_object (gck_builder_end (&builder));
	test->prv_object = gck_object_from_handle (test->mo.session, handle);
	g_object_add_weak_pointer (G_OBJECT (test->prv_object), (gpointer *)&test->prv_object);
}

static void
teardown_loading (TestLoading *test,
                  gconstpointer fixture)
{
	g_object_unref (test->crt_object);
	g_object_unref (test->prv_object);
	g_object_unref (test->pub_object);

	egg_test_wait_for_gtask_thread (test->crt_object || test->prv_object || test->pub_object);

	g_assert (test->crt_object == NULL);
	g_assert (test->prv_object == NULL);
	g_assert (test->pub_object == NULL);

	teardown_module (&test->mo, NULL);
	teardown_attributes (&test->at, fixture);
}

static void
perform_load (TestLoading *test,
              GckObject *object)
{
	GError *error = NULL;
	GBytes *data;
	GNode *info;

	info = _gcr_subject_public_key_load (object, NULL, &error);
	g_assert_no_error (error);
	g_assert (info != NULL);

	data = egg_asn1x_encode (info, NULL);
	egg_assert_cmpbytes (data, ==, g_bytes_get_data (test->at.spk_data, NULL),
	                               g_bytes_get_size (test->at.spk_data));

	g_bytes_unref (data);
	egg_asn1x_destroy (info);
}

static void
test_certificate_load (TestLoading *test,
                       gconstpointer unused)
{
	perform_load (test, test->crt_object);
}

static void
test_public_key_load (TestLoading *test,
                      gconstpointer unused)
{
	perform_load (test, test->pub_object);
}

static void
test_private_key_load (TestLoading *test,
                       gconstpointer unused)
{
	perform_load (test, test->prv_object);
}

static void
on_async_result (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GAsyncResult **ret = user_data;
	g_assert (ret != NULL);
	g_assert (*ret == NULL);
	g_assert (G_IS_ASYNC_RESULT (result));
	*ret = g_object_ref (result);
	egg_test_wait_stop ();
}

static void
perform_load_async (TestLoading *test,
                    GckObject *object)
{
	GAsyncResult *result = NULL;
	GError *error = NULL;
	GBytes *data;
	GNode *info;

	_gcr_subject_public_key_load_async (object, NULL, on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	g_assert (result != NULL);
	info = _gcr_subject_public_key_load_finish (result, &error);
	g_assert_no_error (error);
	g_assert (info != NULL);
	g_object_unref (result);

	data = egg_asn1x_encode (info, NULL);
	egg_assert_cmpbytes (data, ==, g_bytes_get_data (test->at.spk_data, NULL),
	                               g_bytes_get_size (test->at.spk_data));

	g_bytes_unref (data);
	egg_asn1x_destroy (info);
}

static void
test_certificate_load_async (TestLoading *test,
                             gconstpointer unused)
{
	perform_load_async (test, test->crt_object);
}

static void
test_public_key_load_async (TestLoading *test,
                            gconstpointer unused)
{
	perform_load_async (test, test->pub_object);
}

static void
test_private_key_load_async (TestLoading *test,
                             gconstpointer unused)
{
	perform_load_async (test, test->prv_object);
}

enum { PROP_ATTRIBUTES = 1 };
typedef struct { GckObject parent; GckAttributes *attrs; } MockObject;
typedef struct { GckObjectClass parent; } MockObjectClass;

GType mock_object_get_type (void) G_GNUC_CONST;
static void mock_object_cache_init (GckObjectCacheInterface *iface);
G_DEFINE_TYPE_WITH_CODE (MockObject, mock_object, GCK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCK_TYPE_OBJECT_CACHE, mock_object_cache_init)
);

static void
mock_object_init (MockObject *self)
{

}

static void
mock_object_get_property (GObject *obj,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	g_assert (prop_id == PROP_ATTRIBUTES);
	g_value_set_boxed (value, ((MockObject *)obj)->attrs);
}

static void
mock_object_set_property (GObject *obj,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	g_assert (prop_id == PROP_ATTRIBUTES);
	((MockObject *)obj)->attrs = g_value_dup_boxed (value);
}

static void
mock_object_finalize (GObject *obj)
{
	MockObject *self = (MockObject *)obj;
	gck_attributes_unref (self->attrs);
	G_OBJECT_CLASS (mock_object_parent_class)->finalize (obj);
}

static void
mock_object_class_init (MockObjectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = mock_object_get_property;
	gobject_class->set_property = mock_object_set_property;
	gobject_class->finalize = mock_object_finalize;

	g_object_class_override_property (gobject_class, PROP_ATTRIBUTES, "attributes");
}

static void
mock_object_fill (GckObjectCache *object,
                  GckAttributes *attrs)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	MockObject *self = (MockObject *)object;

	gck_builder_add_all (&builder, self->attrs);
	gck_builder_set_all (&builder, attrs);

	gck_attributes_unref (self->attrs);
	self->attrs = gck_builder_end (&builder);
}

static void
mock_object_cache_init (GckObjectCacheInterface *iface)
{
	iface->default_types = NULL;
	iface->n_default_types = 0;
	iface->fill = mock_object_fill;
}

static void
perform_load_already (TestLoading *test,
                      GckAttributes *attributes)
{
	const gulong INVALID = 0xFFF00FF; /* invalid handle, should not be used */
	GckObject *object;
	GError *error = NULL;
	GBytes *data;
	GNode *info;

	object = g_object_new (mock_object_get_type (),
	                       "module", test->mo.module,
	                       "session", test->mo.session,
	                       "handle", INVALID,
	                       "attributes", attributes,
	                       NULL);

	info = _gcr_subject_public_key_load (object, NULL, &error);
	g_assert_no_error (error);
	g_assert (info != NULL);

	data = egg_asn1x_encode (info, NULL);
	egg_assert_cmpbytes (data, ==, g_bytes_get_data (test->at.spk_data, NULL),
	                               g_bytes_get_size (test->at.spk_data));

	g_bytes_unref (data);
	egg_asn1x_destroy (info);
	g_object_unref (object);
}

static void
test_certificate_load_already (TestLoading *test,
                               gconstpointer unused)
{
	perform_load_already (test, test->at.crt_attrs);
}

static void
test_public_key_load_already (TestLoading *test,
                              gconstpointer unused)
{
	perform_load_already (test, test->at.pub_attrs);
}

static void
test_private_key_load_already (TestLoading *test,
                               gconstpointer unused)
{
	perform_load_already (test, test->at.prv_attrs);
}

static void
perform_load_partial (TestLoading *test,
                      GckObject *original,
                      GckAttributes *attributes)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *partial;
	GckObject *object;
	GError *error = NULL;
	GBytes *data;
	GNode *info;
	guint i;

	for (i = 0; i < gck_attributes_count (attributes); i += 2)
		gck_builder_add_attribute (&builder, gck_attributes_at (attributes, i));
	partial = gck_builder_end (&builder);

	object = g_object_new (mock_object_get_type (),
	                       "module", test->mo.module,
	                       "session", test->mo.session,
	                       "handle", gck_object_get_handle (original),
	                       "attributes", partial,
	                       NULL);
	gck_attributes_unref (partial);

	info = _gcr_subject_public_key_load (object, NULL, &error);
	g_assert_no_error (error);
	g_assert (info != NULL);

	data = egg_asn1x_encode (info, NULL);
	egg_assert_cmpbytes (data, ==, g_bytes_get_data (test->at.spk_data, NULL),
	                               g_bytes_get_size (test->at.spk_data));

	g_bytes_unref (data);
	egg_asn1x_destroy (info);
	g_object_unref (object);
}

static void
test_certificate_load_partial (TestLoading *test,
                               gconstpointer unused)
{
	perform_load_partial (test, test->crt_object, test->at.crt_attrs);
}

static void
test_public_key_load_partial (TestLoading *test,
                              gconstpointer unused)
{
	perform_load_partial (test, test->pub_object, test->at.pub_attrs);
}

static void
test_private_key_load_partial (TestLoading *test,
                               gconstpointer unused)
{
	perform_load_partial (test, test->prv_object, test->at.prv_attrs);
}

static void
test_load_failure_lookup (TestModule *test,
                          gconstpointer fixture)
{
	const gulong INVALID = 0xFFF00FF; /* invalid handle, should fail */
	GckObject *object;
	GError *error = NULL;
	GNode *info;

	object = g_object_new (mock_object_get_type (),
	                       "module", test->module,
	                       "session", test->session,
	                       "handle", INVALID,
	                       NULL);

	info = _gcr_subject_public_key_load (object, NULL, &error);
	g_assert_error (error, GCK_ERROR, CKR_OBJECT_HANDLE_INVALID);
	g_assert (info == NULL);
	g_error_free (error);

	g_object_unref (object);
}

static void
test_load_failure_build (TestModule *test,
                         gconstpointer fixture)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attributes;
	const gulong INVALID = 0xFFF00FF; /* invalid handle, shouldn't be used */
	GckObject *object;
	GError *error = NULL;
	GNode *info;

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_CERTIFICATE);
	gck_builder_add_ulong (&builder, CKA_CERTIFICATE_TYPE, CKC_X_509);
	gck_builder_add_string (&builder, CKA_VALUE, "invalid value");
	attributes = gck_builder_end (&builder);

	object = g_object_new (mock_object_get_type (),
	                       "module", test->module,
	                       "session", test->session,
	                       "handle", INVALID,
	                       "attributes", attributes,
	                       NULL);

	gck_attributes_unref (attributes);

	info = _gcr_subject_public_key_load (object, NULL, &error);
	g_assert_error (error, GCK_ERROR, CKR_TEMPLATE_INCONSISTENT);
	g_assert (info == NULL);
	g_error_free (error);

	g_object_unref (object);
}

static const TestFixture FIXTURES[] = {
	{ "rsa", "client", 2048 },
	{ "dsa", "generic-dsa", 1024 },
	{ "ec", "ecc-strong", 521 },
};

static GPtrArray *test_names = NULL;

static const gchar *
test_name (const gchar *format,
           const gchar *basename)
{
	gchar *name = g_strdup_printf (format, basename);
	g_ptr_array_add (test_names, name);
	return name;
}

int
main (int argc, char **argv)
{
	const TestFixture *fixture;
	gint ret;
	guint i;

	g_test_init (&argc, &argv, NULL);

	test_names = g_ptr_array_new_with_free_func (g_free);

	for (i = 0; i < G_N_ELEMENTS (FIXTURES); i++) {
		fixture = &FIXTURES[i];

		g_test_add (test_name ("/gcr/subject-public-key/%s/cert-attributes", fixture->name), TestAttributes, fixture,
		            setup_attributes, test_for_cert_attributes, teardown_attributes);
		g_test_add (test_name ("/gcr/subject-public-key/%s/public-key-attributes", fixture->name), TestAttributes, fixture,
		            setup_attributes, test_for_public_key_attributes, teardown_attributes);
		g_test_add (test_name ("/gcr/subject-public-key/%s/private-key-attributes", fixture->name), TestAttributes, fixture,
		            setup_attributes, test_for_private_key_attributes, teardown_attributes);

		g_test_add (test_name ("/gcr/subject-public-key/%s/certificate-size", fixture->name), TestAttributes, fixture,
		            setup_attributes, test_certificate_calculate_size, teardown_attributes);
		g_test_add (test_name ("/gcr/subject-public-key/%s/public-key-size", fixture->name), TestAttributes, fixture,
		            setup_attributes, test_public_key_calculate_size, teardown_attributes);
		g_test_add (test_name ("/gcr/subject-public-key/%s/private-key-size", fixture->name), TestAttributes, fixture,
		            setup_attributes, test_private_key_calculate_size, teardown_attributes);

		g_test_add (test_name ("/gcr/subject-public-key/%s/certificate-load", fixture->name), TestLoading, fixture,
		            setup_loading, test_certificate_load, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/public-key-load", fixture->name), TestLoading, fixture,
		            setup_loading, test_public_key_load, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/private-key-load", fixture->name), TestLoading, fixture,
		            setup_loading, test_private_key_load, teardown_loading);

		g_test_add (test_name ("/gcr/subject-public-key/%s/certificate-load-async", fixture->name), TestLoading, fixture,
		            setup_loading, test_certificate_load_async, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/public-key-load-async", fixture->name), TestLoading, fixture,
		            setup_loading, test_public_key_load_async, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/private-key-load-async", fixture->name), TestLoading, fixture,
		            setup_loading, test_private_key_load_async, teardown_loading);

		g_test_add (test_name ("/gcr/subject-public-key/%s/certificate-load-already", fixture->name), TestLoading, fixture,
		            setup_loading, test_certificate_load_already, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/public-key-load-already", fixture->name), TestLoading, fixture,
		            setup_loading, test_public_key_load_already, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/private-key-load-already", fixture->name), TestLoading, fixture,
		            setup_loading, test_private_key_load_already, teardown_loading);

		g_test_add (test_name ("/gcr/subject-public-key/%s/certificate-load-partial", fixture->name), TestLoading, fixture,
		            setup_loading, test_certificate_load_partial, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/public-key-load-partial", fixture->name), TestLoading, fixture,
		            setup_loading, test_public_key_load_partial, teardown_loading);
		g_test_add (test_name ("/gcr/subject-public-key/%s/private-key-load-partial", fixture->name), TestLoading, fixture,
		            setup_loading, test_private_key_load_partial, teardown_loading);
	}

	g_test_add ("/gcr/subject-public-key/load-failure-lookup", TestModule, NULL,
	            setup_module, test_load_failure_lookup, teardown_module);
	g_test_add ("/gcr/subject-public-key/load-failure-build", TestModule, NULL,
	            setup_module, test_load_failure_build, teardown_module);

	ret = egg_tests_run_with_loop ();

	g_ptr_array_free (test_names, TRUE);
	return ret;
}
