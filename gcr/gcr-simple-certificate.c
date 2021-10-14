/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
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
 */

#include "config.h"

#include "gcr-certificate.h"
#include "gcr-comparable.h"
#include "gcr-internal.h"
#include "gcr-simple-certificate.h"
#include "gcr-parser.h"

#include <string.h>

/**
 * SECTION:gcr-simple-certificate
 * @title: GcrSimpleCertificate
 * @short_description: A certificate loaded from a memory buffer
 *
 * An implementation of #GcrCertificate which loads a certificate from DER
 * data already located in memory.
 *
 * To create a #GcrSimpleCertificate object use the
 * gcr_simple_certificate_new() or gcr_simple_certificate_new_static()
 * functions.
 */

/**
 * GcrSimpleCertificate:
 *
 * A #GcrCertificate which represents a certificate already in memory.
 */

/**
 * GcrSimpleCertificateClass:
 * @parent_class: The parent class
 *
 * The class for #GcrSimpleCertificate.
 */

struct _GcrSimpleCertificate
{
	GObject parent_instance;

	GBytes *bytes;
};

/* Forward declarations */
static void gcr_simple_certificate_iface_init (GcrCertificateIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrSimpleCertificate, gcr_simple_certificate, G_TYPE_OBJECT,
	GCR_CERTIFICATE_MIXIN_IMPLEMENT_COMPARABLE ();
	G_IMPLEMENT_INTERFACE (GCR_TYPE_CERTIFICATE, gcr_simple_certificate_iface_init);
);

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
gcr_simple_certificate_init (GcrSimpleCertificate *self)
{
}

static void
gcr_simple_certificate_real_finalize (GObject *obj)
{
	GcrSimpleCertificate *self = GCR_SIMPLE_CERTIFICATE (obj);

	g_clear_pointer (&self->bytes, g_bytes_unref);

	G_OBJECT_CLASS (gcr_simple_certificate_parent_class)->finalize (obj);
}

static void
gcr_simple_certificate_class_init (GcrSimpleCertificateClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = gcr_simple_certificate_real_finalize;
	gobject_class->get_property = gcr_certificate_mixin_get_property;

	gcr_certificate_mixin_class_init (gobject_class);
	_gcr_initialize_library ();
}

static const guchar *
gcr_simple_certificate_get_der_data (GcrCertificate *cert,
                                     gsize *n_data)
{
	GcrSimpleCertificate *self = GCR_SIMPLE_CERTIFICATE (cert);

	g_return_val_if_fail (GCR_IS_CERTIFICATE (self), NULL);
	g_return_val_if_fail (n_data, NULL);
	g_return_val_if_fail (self->bytes, NULL);

	/* This is called when we're not a base class */
	return g_bytes_get_data (self->bytes, n_data);
}

static void
gcr_simple_certificate_iface_init (GcrCertificateIface *iface)
{
	iface->get_der_data = gcr_simple_certificate_get_der_data;
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gcr_simple_certificate_new:
 * @bytes: (transfer full): a #GBytes containing the raw DER certificate data
 *
 * Create a new #GcrSimpleCertificate for the raw DER data. The @bytes memory is
 * then owned by the returned object, use g_bytes_ref() before calling this
 * function if you need to keep the #GBytes.
 *
 * Returns: (transfer full) (type Gcr.SimpleCertificate): a new #GcrSimpleCertificate
 */
GcrCertificate *
gcr_simple_certificate_new (GBytes *bytes)
{
	GcrSimpleCertificate *cert;

	g_return_val_if_fail (bytes != NULL, NULL);

	cert = g_object_new (GCR_TYPE_SIMPLE_CERTIFICATE, NULL);
	cert->bytes = bytes;

	return GCR_CERTIFICATE (cert);

}

static void
on_parser_parsed (GcrParser *parser,
                  gpointer user_data)
{
	GcrSimpleCertificate *self = user_data;
	GckAttributes *attributes;
	const GckAttribute *attr;

	attributes = gcr_parser_get_parsed_attributes (parser);
	attr = gck_attributes_find (attributes, CKA_VALUE);
	self->bytes = g_bytes_new (attr->value, attr->length);
}

GcrCertificate *
gcr_simple_certificate_new_from_file (GFile         *file,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
	GcrSimpleCertificate *cert;
	GcrParser *parser;
	GBytes *bytes;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (!error || !*error, NULL);


	bytes = g_file_load_bytes (file, cancellable, NULL, error);
	if (!bytes) {
		return NULL;
	}

	parser = gcr_parser_new ();
	cert = g_object_new (GCR_TYPE_SIMPLE_CERTIFICATE, NULL);
	g_signal_connect (parser, "parsed", G_CALLBACK (on_parser_parsed), cert);
	if (!gcr_parser_parse_bytes (parser, bytes, error)) {
		g_bytes_unref (bytes);
		g_object_unref (parser);
		g_object_unref (cert);
		return NULL;
	}

	g_bytes_unref (bytes);
	g_object_unref (parser);
	return GCR_CERTIFICATE (cert);
}

void
gcr_simple_certificate_new_from_file_async (GFile               *file,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{

}

GcrCertificate *
gcr_simple_certificate_new_from_file_finish (GAsyncResult  *res,
                                             GError       **error)
{
	return NULL;
}
