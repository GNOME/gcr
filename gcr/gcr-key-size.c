/*
 * Copyright (C) 2010 Stefan Walter
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "gcr-key-size.h"
#include "gcr-oids.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"

static guint
calculate_rsa_key_size (EggBytes *data)
{
	GNode *asn;
	EggBytes *content;
	guint key_size;

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "RSAPublicKey", data);
	g_return_val_if_fail (asn, 0);

	content = egg_asn1x_get_raw_value (egg_asn1x_node (asn, "modulus", NULL));
	if (!content)
		g_return_val_if_reached (0);

	egg_asn1x_destroy (asn);

	/* Removes the complement */
	key_size = (egg_bytes_get_size (content) / 2) * 2 * 8;

	egg_bytes_unref (content);
	return key_size;
}

static guint
calculate_dsa_params_size (EggBytes *data)
{
	GNode *asn;
	EggBytes *content;
	guint key_size;

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "DSAParameters", data);
	g_return_val_if_fail (asn, 0);

	content = egg_asn1x_get_raw_value (egg_asn1x_node (asn, "p", NULL));
	if (!content)
		g_return_val_if_reached (0);

	egg_asn1x_destroy (asn);

	/* Removes the complement */
	key_size = (egg_bytes_get_size (content) / 2) * 2 * 8;

	egg_bytes_unref (content);
	return key_size;
}

guint
_gcr_key_size_calculate (GNode *subject_public_key)
{
	EggBytes *key;
	guint key_size = 0, n_bits;
	GQuark oid;

	/* Figure out the algorithm */
	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (subject_public_key,
	                                                  "algorithm", "algorithm", NULL));
	g_return_val_if_fail (oid, 0);

	/* RSA keys are stored in the main subjectPublicKey field */
	if (oid == GCR_OID_PKIX1_RSA) {

		/* A bit string so we cannot process in place */
		key = egg_asn1x_get_bits_as_raw (egg_asn1x_node (subject_public_key, "subjectPublicKey", NULL), &n_bits);
		g_return_val_if_fail (key != NULL, 0);
		key_size = calculate_rsa_key_size (key);
		egg_bytes_unref (key);

	/* The DSA key size is discovered by the prime in params */
	} else if (oid == GCR_OID_PKIX1_DSA) {
		key = egg_asn1x_get_raw_element (egg_asn1x_node (subject_public_key, "algorithm", "parameters", NULL));
		key_size = calculate_dsa_params_size (key);
		egg_bytes_unref (key);

	} else {
		g_message ("unsupported key algorithm in certificate: %s", g_quark_to_string (oid));
	}

	return key_size;
}
