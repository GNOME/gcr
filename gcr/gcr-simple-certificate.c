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
#include "gcr-internal.h"
#include "gcr-simple-certificate.h"

#include <string.h>

/**
 * GcrSimpleCertificate:
 *
 * An implementation of [iface@Certificate] which loads a certificate from DER
 * data already located in memory.
 *
 * To create an object, use the [ctor@SimpleCertificate.new] or
 * [ctor@SimpleCertificate.new_static] functions.
 */

struct _GcrSimpleCertificatePrivate {
	GBytes *bytes;
};

/* Forward declarations */
static void gcr_simple_certificate_iface_init (GcrCertificateIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrSimpleCertificate, gcr_simple_certificate, G_TYPE_OBJECT,
	G_ADD_PRIVATE (GcrSimpleCertificate);
	G_IMPLEMENT_INTERFACE (GCR_TYPE_CERTIFICATE, gcr_simple_certificate_iface_init);
);

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
gcr_simple_certificate_init (GcrSimpleCertificate *self)
{
	self->pv = gcr_simple_certificate_get_instance_private (self);
}

static void
gcr_simple_certificate_real_finalize (GObject *obj)
{
	GcrSimpleCertificate *self = GCR_SIMPLE_CERTIFICATE (obj);

	g_clear_pointer (&self->pv->bytes, g_bytes_unref);

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
	g_return_val_if_fail (self->pv->bytes, NULL);

	/* This is called when we're not a base class */
	return g_bytes_get_data (self->pv->bytes, n_data);
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
 * @data: (array length=n_data): the raw DER certificate data
 * @n_data: The length of @data
 *
 * Create a new #GcrSimpleCertificate for the raw DER data. The @data memory is
 * copied so you can dispose of it after this function returns.
 *
 * Returns: (transfer full) (type Gcr.SimpleCertificate): a new #GcrSimpleCertificate
 */
GcrCertificate *
gcr_simple_certificate_new (const guchar *data,
                            gsize n_data)
{
	GcrSimpleCertificate *cert;

	g_return_val_if_fail (data, NULL);
	g_return_val_if_fail (n_data, NULL);

	cert = g_object_new (GCR_TYPE_SIMPLE_CERTIFICATE, NULL);
	cert->pv->bytes = g_bytes_new (data, n_data);
	return GCR_CERTIFICATE (cert);
}

/**
 * gcr_simple_certificate_new_static: (skip)
 * @data: (array length=n_data): The raw DER certificate data
 * @n_data: The length of @data
 *
 * Create a new #GcrSimpleCertificate for the raw DER data. The @data memory is
 * not copied and must persist until the #GcrSimpleCertificate object is
 * destroyed.
 *
 * Returns: (transfer full) (type Gcr.SimpleCertificate): a new #GcrSimpleCertificate
 */
GcrCertificate *
gcr_simple_certificate_new_static (const guchar *data,
                                   gsize n_data)
{
	GcrSimpleCertificate *cert;

	g_return_val_if_fail (data, NULL);
	g_return_val_if_fail (n_data, NULL);

	cert = g_object_new (GCR_TYPE_SIMPLE_CERTIFICATE, NULL);
	cert->pv->bytes = g_bytes_new_static (data, n_data);
	return GCR_CERTIFICATE (cert);
}
