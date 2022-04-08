/*
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"
#include "gcr-certificate-basics-widget.h"

#ifndef GCR_DISABLE_DEPRECATED

/* These are all stubs for GcrCertificateWidget */

GType
gcr_certificate_basics_widget_get_type (void)
{
	return gcr_certificate_widget_get_type ();
}

/**
 * gcr_certificate_basics_widget_new: (skip)
 *
 * Create new certificate viewer.
 *
 * Deprecated: Since 2.30
 *
 * Returns: (transfer full) (type GcrUi.CertificateWidget): a new certificate widget
 */
GcrCertificateBasicsWidget *
gcr_certificate_basics_widget_new (GcrCertificate *cert)
{
	return gcr_certificate_widget_new (cert);
}

/**
 * gcr_certificate_basics_widget_get_certificate: (skip)
 *
 * Get certificate on viewer.
 *
 * Deprecated: Since 2.30
 *
 * Returns: (transfer none): the certificate
 */
GcrCertificate *
gcr_certificate_basics_widget_get_certificate (GcrCertificateBasicsWidget *basics)
{
	return gcr_certificate_widget_get_certificate (basics);
}

/**
 * gcr_certificate_basics_widget_set_certificate: (skip)
 * @basics: the certificate widget
 * @cert: the certificate
 *
 * Set certificate on viewer.
 *
 * Deprecated: Since 2.30
 */
void
gcr_certificate_basics_widget_set_certificate (GcrCertificateBasicsWidget *basics,
                                                GcrCertificate *cert)
{
	return gcr_certificate_widget_set_certificate (basics, cert);
}

#endif /* GCR_DISABLE_DEPRECATED */
