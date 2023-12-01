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

#include "gcr-parser.h"

#include "egg/egg-asn1-defs.h"
#include "egg/egg-asn1x.h"
#include "gcr-internal.h"
#include "gcr/gcr-oids.h"
#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/gnutls.h>
#include <gnutls/pkcs12.h>

#define SUCCESS 0

/* -----------------------------------------------------------------------------
 * RSA PRIVATE KEY
 */

static gint
handle_private_key_rsa (GcrParsed *parsed,
			gnutls_privkey_t privkey,
			GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	gnutls_datum_t m = { NULL, 0 };
	gnutls_datum_t e = { NULL, 0 };
	gnutls_datum_t d = { NULL, 0 };
	gnutls_datum_t p = { NULL, 0 };
	gnutls_datum_t q = { NULL, 0 };
	gnutls_datum_t u = { NULL, 0 };

	if (gnutls_privkey_get_pk_algorithm (privkey, NULL) != GNUTLS_PK_RSA)
		return res;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_RSA, data);
	_gcr_parsed_parsing_object (parsed, CKO_PRIVATE_KEY);
	_gcr_parsed_set_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_RSA);
	_gcr_parsed_set_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	res = GCR_ERROR_FAILURE;

	if (gnutls_privkey_export_rsa_raw (privkey,
					   &m, &e, &d, &p, &q, &u,
					   NULL, NULL) < 0)
		goto done;

	_gcr_parsed_set_attribute (parsed, CKA_MODULUS, m.data, m.size);
	_gcr_parsed_set_attribute (parsed, CKA_PUBLIC_EXPONENT, e.data, e.size);
	_gcr_parsed_set_attribute (parsed, CKA_PRIVATE_EXPONENT, d.data, d.size);
	_gcr_parsed_set_attribute (parsed, CKA_PRIME_1, p.data, p.size);
	_gcr_parsed_set_attribute (parsed, CKA_PRIME_2, q.data, q.size);
	_gcr_parsed_set_attribute (parsed, CKA_COEFFICIENT, u.data, u.size);

	res = SUCCESS;

 done:
	gnutls_free (m.data);
	gnutls_free (e.data);
	gnutls_free (d.data);
	gnutls_free (p.data);
	gnutls_free (q.data);
	gnutls_free (u.data);

	return res;
}

gint
_gcr_parser_parse_der_private_key_rsa (GcrParser *self,
				       GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	GcrParsed *parsed;
	gnutls_datum_t der;
	gnutls_privkey_t privkey = NULL;

	parsed = _gcr_parser_push_parsed (self, TRUE);

	der.data = (unsigned char *) g_bytes_get_data (data, NULL);
	der.size = g_bytes_get_size (data);

	if (gnutls_privkey_init (&privkey) < 0 ||
	    gnutls_privkey_import_x509_raw (privkey,
					    &der,
					    GNUTLS_X509_FMT_DER,
					    NULL,
					    0) < 0)
		goto done;

	res = handle_private_key_rsa (parsed, privkey, data);
	if (res != SUCCESS)
		goto done;

	_gcr_parser_fire_parsed (self, parsed);

 done:
	gnutls_privkey_deinit (privkey);

	if (res == GCR_ERROR_FAILURE)
		g_message ("invalid RSA key");

	_gcr_parser_pop_parsed (self, parsed);
	return res;
}

/* -----------------------------------------------------------------------------
 * DSA PRIVATE KEY
 */

static gint
handle_private_key_dsa (GcrParsed *parsed,
			gnutls_privkey_t privkey,
			GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	gnutls_datum_t p = { NULL, 0 };
	gnutls_datum_t q = { NULL, 0 };
	gnutls_datum_t g = { NULL, 0 };
	gnutls_datum_t x = { NULL, 0 };

	if (gnutls_privkey_get_pk_algorithm (privkey, NULL) != GNUTLS_PK_DSA)
		return res;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_DSA, data);
	_gcr_parsed_parsing_object (parsed, CKO_PRIVATE_KEY);
	_gcr_parsed_set_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_DSA);
	_gcr_parsed_set_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	res = GCR_ERROR_FAILURE;

	if (gnutls_privkey_export_dsa_raw (privkey, &p, &q, &g, NULL, &x) < 0)
		goto done;

	_gcr_parsed_set_attribute (parsed, CKA_PRIME, p.data, p.size);
	_gcr_parsed_set_attribute (parsed, CKA_SUBPRIME, q.data, q.size);
	_gcr_parsed_set_attribute (parsed, CKA_BASE, g.data, g.size);
	_gcr_parsed_set_attribute (parsed, CKA_VALUE, x.data, x.size);

	res = SUCCESS;
 done:
	gnutls_free (p.data);
	gnutls_free (q.data);
	gnutls_free (g.data);
	gnutls_free (x.data);

	return res;
}

gint
_gcr_parser_parse_der_private_key_dsa (GcrParser *self,
				       GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	GcrParsed *parsed;
	gnutls_datum_t der;
	gnutls_privkey_t privkey = NULL;

	parsed = _gcr_parser_push_parsed (self, TRUE);

	der.data = (unsigned char *)g_bytes_get_data (data, NULL);
	der.size = g_bytes_get_size (data);

	if (gnutls_privkey_init (&privkey) < 0 ||
	    gnutls_privkey_import_x509_raw (privkey,
					    &der,
					    GNUTLS_X509_FMT_DER,
					    NULL,
					    0) < 0)
		goto done;

	res = handle_private_key_dsa (parsed, privkey, data);
	if (res != SUCCESS)
		goto done;

	_gcr_parser_fire_parsed (self, parsed);

 done:
	gnutls_privkey_deinit (privkey);

	if (res == GCR_ERROR_FAILURE)
		g_message ("invalid DSA key");

	_gcr_parser_pop_parsed (self, parsed);
	return res;
}

gint
_gcr_parser_parse_der_private_key_dsa_parts (GcrParser *self,
					     GBytes *keydata,
					     GNode *params)
{
	/* This cannot be supported by GnuTLS */
	return GCR_ERROR_UNRECOGNIZED;
}

/* -----------------------------------------------------------------------------
 * EC PRIVATE KEY
 */

static gint
handle_private_key_ec (GcrParsed *parsed,
		       gnutls_privkey_t privkey,
		       GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	gnutls_ecc_curve_t curve;
	gnutls_datum_t x = { NULL, 0 };
	gnutls_datum_t y = { NULL, 0 };
	gnutls_datum_t k = { NULL, 0 };
	GByteArray *array = NULL;
	GNode *asn = NULL;
	GNode *asn_q = NULL;
	guint8 c;

	if (gnutls_privkey_get_pk_algorithm (privkey, NULL) != GNUTLS_PK_EC)
		return res;

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "ECPrivateKey", data);
	if (!asn)
		goto done;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_EC, data);
	_gcr_parsed_parsing_object (parsed, CKO_PRIVATE_KEY);
	_gcr_parsed_set_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_EC);
	_gcr_parsed_set_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	res = GCR_ERROR_FAILURE;

	if (!_gcr_parsed_set_asn1_element (parsed, asn, "parameters", CKA_EC_PARAMS))
		goto done;

	if (gnutls_privkey_export_ecc_raw (privkey, &curve, &x, &y, &k) < 0)
		goto done;

	_gcr_parsed_set_attribute (parsed, CKA_VALUE, k.data, k.size);

	array = g_byte_array_new ();
	if (!array)
		goto done;

	c = 0x04;
	g_byte_array_append (array, &c, 1);
	g_byte_array_append (array, x.data, x.size);
	g_byte_array_append (array, y.data, y.size);

	asn_q = egg_asn1x_create (pk_asn1_tab, "ECPoint");
	if (!asn_q)
		goto done;

	egg_asn1x_take_string_as_bytes (asn_q, g_byte_array_free_to_bytes (array));
	array = NULL;

	if (!_gcr_parsed_set_asn1_structure (parsed, asn_q, CKA_EC_POINT))
		goto done;

	res = SUCCESS;

 done:
	gnutls_free (x.data);
	gnutls_free (y.data);
	gnutls_free (k.data);
	if (array)
		g_byte_array_free (array, TRUE);
	egg_asn1x_destroy (asn);
	egg_asn1x_destroy (asn_q);

	return res;
}

gint
_gcr_parser_parse_der_private_key_ec (GcrParser *self,
				      GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	GcrParsed *parsed;
	gnutls_datum_t der;
	gnutls_privkey_t privkey = NULL;

	parsed = _gcr_parser_push_parsed (self, TRUE);

	der.data = (unsigned char *)g_bytes_get_data (data, NULL);
	der.size = g_bytes_get_size (data);

	if (gnutls_privkey_init (&privkey) < 0 ||
	    gnutls_privkey_import_x509_raw (privkey,
					    &der,
					    GNUTLS_X509_FMT_DER,
					    NULL,
					    0) < 0)
		goto done;

	res = handle_private_key_ec (parsed, privkey, data);
	if (res != SUCCESS)
		goto done;

	_gcr_parser_fire_parsed (self, parsed);

 done:
	gnutls_privkey_deinit (privkey);

	if (res == GCR_ERROR_FAILURE)
		g_message ("invalid EC key");

	_gcr_parser_pop_parsed (self, parsed);
	return res;
}

/* -----------------------------------------------------------------------------
 * PKCS8
 */

static gint
handle_private_key (GcrParsed *parsed,
		    gnutls_privkey_t privkey,
		    GBytes *data)
{
	gint res;

	switch (gnutls_privkey_get_pk_algorithm (privkey, NULL)) {
	case GNUTLS_PK_RSA:
		res = handle_private_key_rsa (parsed, privkey, data);
		break;
	case GNUTLS_PK_DSA:
		res = handle_private_key_dsa (parsed, privkey, data);
		break;
	case GNUTLS_PK_EC:
		res = handle_private_key_ec (parsed, privkey, data);
		break;
	default:
		g_message ("invalid or unsupported key type in PKCS#8 key");
		res = GCR_ERROR_UNRECOGNIZED;
		break;
	}

	return res;
}

gint
_gcr_parser_parse_der_pkcs8_encrypted (GcrParser *self,
				       GBytes *data)
{
	PasswordState pstate = PASSWORD_STATE_INIT;
	gint ret, r;
	const gchar *password;
	GcrParsed *parsed, *plain_parsed, *privkey_parsed;
	gnutls_datum_t der;
	gnutls_privkey_t privkey = NULL;
	GNode *asn = NULL;

	parsed = _gcr_parser_push_parsed (self, FALSE);
	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "pkcs-8-EncryptedPrivateKeyInfo", data);
	if (!asn)
		goto done;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PKCS8_ENCRYPTED, data);

	der.data = (unsigned char *)g_bytes_get_data (data, NULL);
	der.size = g_bytes_get_size (data);

	if (gnutls_privkey_init (&privkey) < 0)
		goto done;

	/* Loop to try different passwords */
	for (;;) {
		r = _gcr_enum_next_password (self, &pstate, &password);
		if (r != SUCCESS) {
			ret = r;
			break;
		}

		if (gnutls_privkey_import_x509_raw (privkey,
						    &der,
						    GNUTLS_X509_FMT_DER,
						    password,
						    0) < 0)
			r = GCR_ERROR_UNRECOGNIZED;

		if (r == SUCCESS) {
			plain_parsed = _gcr_parser_push_parsed (self, TRUE);
			_gcr_parsed_parsing_block (plain_parsed,
						   GCR_FORMAT_DER_PKCS8_PLAIN,
						   data);
			privkey_parsed = _gcr_parser_push_parsed (self, TRUE);
			r = handle_private_key (privkey_parsed, privkey, data);
			_gcr_parser_pop_parsed (self, privkey_parsed);
			_gcr_parser_pop_parsed (self, plain_parsed);
		}

		if (r != GCR_ERROR_UNRECOGNIZED) {
			ret = r;
			break;
		}

		/* We assume unrecognized data, is a bad encryption key */
	}

 done:
	egg_asn1x_destroy (asn);
	gnutls_privkey_deinit (privkey);

	_gcr_parser_pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * PKCS12
 */

static gint
handle_pkcs12_bag (GcrParser *self,
                   gnutls_pkcs12_bag_t bag)
{
	gint res = GCR_ERROR_FAILURE;
	guint count = 0;
	GBytes *element = NULL;
	gchar *friendly;
	guint i;
	GcrParsed *parsed;
	gint ret;

	ret = gnutls_pkcs12_bag_get_count (bag);
	if (ret < 0)
		return res;

	count = ret;

	/*
	 * Now inside each bag are multiple elements. Who comes up
	 * with this stuff?
	 */
	for (i = 0; i < count; i++) {
		gnutls_datum_t data;

		parsed = _gcr_parser_push_parsed (self, FALSE);

		ret = gnutls_pkcs12_bag_get_friendly_name (bag, i, &friendly);
		if (ret == 0 && friendly != NULL)
			_gcr_parsed_set_label (parsed, friendly);

		gnutls_pkcs12_bag_get_data (bag, i, &data);

		switch (gnutls_pkcs12_bag_get_type (bag, i)) {
		case GNUTLS_BAG_PKCS8_KEY:
			element = g_bytes_new_static (data.data, data.size);
			if (!element) {
				ret = GCR_ERROR_FAILURE;
				break;
			}
			ret = _gcr_parser_parse_der_pkcs8_plain (self, element);
			break;
		case GNUTLS_BAG_PKCS8_ENCRYPTED_KEY:
			element = g_bytes_new_static (data.data, data.size);
			if (!element) {
				ret = GCR_ERROR_FAILURE;
				break;
			}
			ret = _gcr_parser_parse_der_pkcs8_encrypted (self, element);
			break;
		case GNUTLS_BAG_CERTIFICATE:
			element = g_bytes_new_static (data.data, data.size);
			if (!element) {
				ret = GCR_ERROR_FAILURE;
				break;
			}
			ret = _gcr_parser_handle_pkcs12_cert_bag (self, element);
			break;
		default:
			ret = GCR_ERROR_UNRECOGNIZED;
			break;
		}

		if (element != NULL)
			g_bytes_unref (element);

		_gcr_parser_pop_parsed (self, parsed);

		if (ret == GCR_ERROR_FAILURE ||
		    ret == GCR_ERROR_CANCELLED ||
		    ret == GCR_ERROR_LOCKED) {
			return ret;
		}
	}

	return SUCCESS;
}

static gint
handle_pkcs12_encrypted_bag (GcrParser *self,
                             gnutls_pkcs12_bag_t bag)
{
	PasswordState pstate = PASSWORD_STATE_INIT;
	const gchar *password;
	gint ret = GCR_ERROR_FAILURE, r;

	/* Loop to try different passwords */
	for (;;) {
		r = _gcr_enum_next_password (self, &pstate, &password);
		if (r != SUCCESS) {
			ret = r;
			break;
		}

		if (gnutls_pkcs12_bag_decrypt (bag, password) < 0)
			r = GCR_ERROR_UNRECOGNIZED;

		/* Try to parse the resulting key */
		if (r == SUCCESS)
			r = handle_pkcs12_bag (self, bag);

		if (r != GCR_ERROR_UNRECOGNIZED) {
			ret = r;
			break;
		}

		/* We assume unrecognized data is a bad encryption key */
	}

	return ret;
}

static gint
handle_pkcs12_safe (GcrParser *self,
                    gnutls_pkcs12_t pkcs12)
{
	gint res = GCR_ERROR_FAILURE;
	gint ret;
	gnutls_pkcs12_bag_t bag = NULL;
	guint i;

	/*
	 * Inside each PKCS12 safe there are multiple bags.
	 */
	for (i = 0; TRUE; ++i) {
		ret = gnutls_pkcs12_bag_init (&bag);
		if (ret < 0)
			goto done;

		ret = gnutls_pkcs12_get_bag (pkcs12, i, bag);
		if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			break;
		else if (ret < 0)
			goto done;

		if (gnutls_pkcs12_bag_get_type (bag, 0) == GNUTLS_BAG_ENCRYPTED)
			ret = handle_pkcs12_encrypted_bag (self, bag);
		else
			ret = handle_pkcs12_bag (self, bag);

		gnutls_pkcs12_bag_deinit (bag);
		bag = NULL;

		if (ret == GCR_ERROR_FAILURE ||
		    ret == GCR_ERROR_CANCELLED ||
		    ret == GCR_ERROR_LOCKED) {
			res = ret;
			goto done;
		}
	}

	res = SUCCESS;

 done:
	gnutls_pkcs12_bag_deinit (bag);
	return res;
}

static gint
verify_pkcs12_safe (GcrParser *self,
                    gnutls_pkcs12_t pkcs12)
{
	PasswordState pstate = PASSWORD_STATE_INIT;
	const gchar *password;
	gint ret;

	ret = GCR_ERROR_FAILURE;

	/* Loop to try different passwords */
	for (;;) {
		ret = _gcr_enum_next_password (self, &pstate, &password);
		if (ret != SUCCESS)
			goto done;

		if (gnutls_pkcs12_verify_mac (pkcs12, password) < 0)
			goto done;
	}

	ret = SUCCESS;
 done:
	return ret;
}

gint
_gcr_parser_parse_der_pkcs12 (GcrParser *self,
			      GBytes *data)
{
	gint ret;
	GcrParsed *parsed;
	gnutls_datum_t der;
	gnutls_pkcs12_t pkcs12 = NULL;
	GNode *asn = NULL;
	GNode *mac_data;

	parsed = _gcr_parser_push_parsed (self, FALSE);
	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode_full (pkix_asn1_tab, "pkcs-12-PFX",
	                                        data, EGG_ASN1X_NO_STRICT);
	if (!asn)
		goto done;

	der.data = (unsigned char *)g_bytes_get_data (data, NULL);
	der.size = g_bytes_get_size (data);

	if (gnutls_pkcs12_init (&pkcs12) < 0 ||
	    gnutls_pkcs12_import (pkcs12,
				  &der,
				  GNUTLS_X509_FMT_DER,
				  0) < 0)
		goto done;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PKCS12, data);

	/*
	 * The MAC is optional (and outside the encryption no less). I wonder
	 * what the designers (ha) of PKCS#12 were trying to achieve
	 */
	mac_data = egg_asn1x_node (asn, "macData", NULL);
	ret = mac_data == NULL ? SUCCESS : verify_pkcs12_safe (self, pkcs12);
	if (ret == SUCCESS)
		ret = handle_pkcs12_safe (self, pkcs12);

 done:
	gnutls_pkcs12_deinit (pkcs12);
	egg_asn1x_destroy (asn);
	_gcr_parser_pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * ARMOR PARSING
 */

gint
_gcr_parser_handle_encrypted_pem (GcrParser *self,
				  gint format_id,
				  gint want_format,
				  GHashTable *headers,
				  GBytes *data,
				  GBytes *outer)
{
	PasswordState pstate = PASSWORD_STATE_INIT;
	const gchar *password;
	const gchar *val;
	gint res, r;
	gnutls_datum_t pem;
	gnutls_privkey_t privkey = NULL;
	GcrParsed *parsed, *privkey_parsed;

	parsed = _gcr_parser_push_parsed (self, TRUE);
	res = GCR_ERROR_UNRECOGNIZED;

	g_assert (GCR_IS_PARSER (self));
	g_assert (headers);

	if (gnutls_privkey_init (&privkey) < 0)
		return res;

	pem.data = (unsigned char *)g_bytes_get_data (outer, NULL);
	pem.size = g_bytes_get_size (outer);

	val = g_hash_table_lookup (headers, "DEK-Info");
	if (!val) {
		g_message ("missing encryption header");
		return GCR_ERROR_FAILURE;
	}

	for (;;) {
		res = _gcr_enum_next_password (self, &pstate, &password);
		if (res != SUCCESS)
			break;

		r = gnutls_privkey_import_x509_raw (privkey,
						    &pem,
						    GNUTLS_X509_FMT_PEM,
						    password,
						    0);
		if (r < 0)
			res = GCR_ERROR_UNRECOGNIZED;

		if (res == SUCCESS) {
			privkey_parsed = _gcr_parser_push_parsed (self, TRUE);
			res = handle_private_key (privkey_parsed, privkey, data);
			_gcr_parser_pop_parsed (self, privkey_parsed);
		}

		/* Unrecognized is a bad password */
		if (res != GCR_ERROR_UNRECOGNIZED)
			break;
	}

	gnutls_privkey_deinit (privkey);
	_gcr_parser_pop_parsed (self, parsed);
	return res;
}
