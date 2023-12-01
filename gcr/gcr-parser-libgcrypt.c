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

#include "gcr-internal.h"
#include "gcr/gcr-oids.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-asn1x.h"
#include "egg/egg-openssl.h"
#include "egg/egg-secure-memory.h"
#include "egg/egg-symkey.h"
#include <gcrypt.h>

EGG_SECURE_DECLARE (parser_libgcrypt);

#define SUCCESS 0

/* -----------------------------------------------------------------------------
 * RSA PRIVATE KEY
 */

gint
_gcr_parser_parse_der_private_key_rsa (GcrParser *self,
				       GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	GNode *asn = NULL;
	gulong version;
	GcrParsed *parsed;

	parsed = _gcr_parser_push_parsed (self, TRUE);

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "RSAPrivateKey", data);
	if (!asn)
		goto done;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_RSA, data);
	_gcr_parsed_parsing_object (parsed, CKO_PRIVATE_KEY);
	_gcr_parsed_set_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_RSA);
	_gcr_parsed_set_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	res = GCR_ERROR_FAILURE;

	if (!egg_asn1x_get_integer_as_ulong (egg_asn1x_node (asn, "version", NULL), &version))
		goto done;

	/* We only support simple version */
	if (version != 0) {
		res = GCR_ERROR_UNRECOGNIZED;
		g_message ("unsupported version of RSA key: %lu", version);
		goto done;
	}

	if (!_gcr_parsed_set_asn1_number (parsed, asn, "modulus", CKA_MODULUS) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "publicExponent", CKA_PUBLIC_EXPONENT) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "privateExponent", CKA_PRIVATE_EXPONENT) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "prime1", CKA_PRIME_1) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "prime2", CKA_PRIME_2) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "coefficient", CKA_COEFFICIENT))
		goto done;

	_gcr_parser_fire_parsed (self, parsed);
	res = SUCCESS;

 done:
	egg_asn1x_destroy (asn);
	if (res == GCR_ERROR_FAILURE)
		g_message ("invalid RSA key");

	_gcr_parser_pop_parsed (self, parsed);
	return res;
}

/* -----------------------------------------------------------------------------
 * DSA PRIVATE KEY
 */

gint
_gcr_parser_parse_der_private_key_dsa (GcrParser *self,
				       GBytes *data)
{
	gint ret = GCR_ERROR_UNRECOGNIZED;
	GNode *asn = NULL;
	GcrParsed *parsed;

	parsed = _gcr_parser_push_parsed (self, TRUE);

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "DSAPrivateKey", data);
	if (!asn)
		goto done;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_DSA, data);
	_gcr_parsed_parsing_object (parsed, CKO_PRIVATE_KEY);
	_gcr_parsed_set_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_DSA);
	_gcr_parsed_set_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	ret = GCR_ERROR_FAILURE;

	if (!_gcr_parsed_set_asn1_number (parsed, asn, "p", CKA_PRIME) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "q", CKA_SUBPRIME) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "g", CKA_BASE) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn, "priv", CKA_VALUE))
		goto done;

	_gcr_parser_fire_parsed (self, parsed);
	ret = SUCCESS;

 done:
	egg_asn1x_destroy (asn);
	if (ret == GCR_ERROR_FAILURE)
		g_message ("invalid DSA key");

	_gcr_parser_pop_parsed (self, parsed);
	return ret;
}

gint
_gcr_parser_parse_der_private_key_dsa_parts (GcrParser *self,
					     GBytes *keydata,
					     GNode *params)
{
	gint ret = GCR_ERROR_UNRECOGNIZED;
	GNode *asn_params = NULL;
	GNode *asn_key = NULL;
	GcrParsed *parsed;

	parsed = _gcr_parser_push_parsed (self, TRUE);

	asn_params = egg_asn1x_get_any_as (params, pk_asn1_tab, "DSAParameters");
	asn_key = egg_asn1x_create_and_decode (pk_asn1_tab, "DSAPrivatePart", keydata);
	if (!asn_params || !asn_key)
		goto done;

	_gcr_parsed_parsing_object (parsed, CKO_PRIVATE_KEY);
	_gcr_parsed_set_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_DSA);
	_gcr_parsed_set_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	ret = GCR_ERROR_FAILURE;

	if (!_gcr_parsed_set_asn1_number (parsed, asn_params, "p", CKA_PRIME) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn_params, "q", CKA_SUBPRIME) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn_params, "g", CKA_BASE) ||
	    !_gcr_parsed_set_asn1_number (parsed, asn_key, NULL, CKA_VALUE))
		goto done;

	_gcr_parser_fire_parsed (self, parsed);
	ret = SUCCESS;

 done:
	egg_asn1x_destroy (asn_key);
	egg_asn1x_destroy (asn_params);
	if (ret == GCR_ERROR_FAILURE)
		g_message ("invalid DSA key");

	_gcr_parser_pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * EC PRIVATE KEY
 */

gint
_gcr_parser_parse_der_private_key_ec (GcrParser *self,
				      GBytes *data)
{
	gint ret = GCR_ERROR_UNRECOGNIZED;
	GNode *asn = NULL;
	GBytes *value = NULL;
	GBytes *pub = NULL;
	GNode *asn_q = NULL;
	GcrParsed *parsed;
	guint bits;
	gulong version;

	parsed = _gcr_parser_push_parsed (self, TRUE);

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "ECPrivateKey", data);
	if (!asn)
		goto done;

	if (!egg_asn1x_get_integer_as_ulong (egg_asn1x_node (asn, "version", NULL), &version))
		goto done;

	/* We only support simple version */
	if (version != 1) {
		g_message ("unsupported version of EC key: %lu", version);
		goto done;
	}

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_EC, data);
	_gcr_parsed_parsing_object (parsed, CKO_PRIVATE_KEY);
	_gcr_parsed_set_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_EC);
	_gcr_parsed_set_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	ret = GCR_ERROR_FAILURE;

	if (!_gcr_parsed_set_asn1_element (parsed, asn, "parameters", CKA_EC_PARAMS))
		goto done;

	value = egg_asn1x_get_string_as_usg (egg_asn1x_node (asn, "privateKey", NULL), egg_secure_realloc);
	if (!value)
		goto done;

	_gcr_parsed_set_attribute_bytes (parsed, CKA_VALUE, value);

	pub = egg_asn1x_get_bits_as_raw (egg_asn1x_node (asn, "publicKey", NULL), &bits);
	if (!pub || bits != 8 * g_bytes_get_size (pub))
		goto done;
	asn_q = egg_asn1x_create (pk_asn1_tab, "ECPoint");
	if (!asn_q)
		goto done;
	egg_asn1x_set_string_as_bytes (asn_q, pub);

	if (!_gcr_parsed_set_asn1_structure (parsed, asn_q, CKA_EC_POINT))
		goto done;

	_gcr_parser_fire_parsed (self, parsed);
	ret = SUCCESS;

 done:
	if (pub)
		g_bytes_unref (pub);
	if (value)
		g_bytes_unref (value);
	egg_asn1x_destroy (asn);
	egg_asn1x_destroy (asn_q);
	if (ret == GCR_ERROR_FAILURE)
		g_message ("invalid EC key");

	_gcr_parser_pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * PKCS8
 */

gint
_gcr_parser_parse_der_pkcs8_encrypted (GcrParser *self,
				       GBytes *data)
{
	PasswordState pstate = PASSWORD_STATE_INIT;
	GNode *asn = NULL;
	gcry_cipher_hd_t cih = NULL;
	gcry_error_t gcry;
	gint ret, r;
	GQuark scheme;
	guchar *crypted = NULL;
	GNode *params = NULL;
	GBytes *cbytes;
	gsize n_crypted;
	const gchar *password;
	GcrParsed *parsed;
	gint l;

	parsed = _gcr_parser_push_parsed (self, FALSE);
	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "pkcs-8-EncryptedPrivateKeyInfo", data);
	if (!asn)
		goto done;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PKCS8_ENCRYPTED, data);
	ret = GCR_ERROR_FAILURE;

	/* Figure out the type of encryption */
	scheme = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, "encryptionAlgorithm", "algorithm", NULL));
	if (!scheme)
		goto done;

	params = egg_asn1x_node (asn, "encryptionAlgorithm", "parameters", NULL);

	/* Loop to try different passwords */
	for (;;) {

		g_assert (cih == NULL);

		r = _gcr_enum_next_password (self, &pstate, &password);
		if (r != SUCCESS) {
			ret = r;
			break;
		}

		/* Parse the encryption stuff into a cipher. */
		if (!egg_symkey_read_cipher (scheme, password, -1, params, &cih))
			break;

		crypted = egg_asn1x_get_string_as_raw (egg_asn1x_node (asn, "encryptedData", NULL), egg_secure_realloc, &n_crypted);
		if (!crypted)
			break;

		gcry = gcry_cipher_decrypt (cih, crypted, n_crypted, NULL, 0);
		gcry_cipher_close (cih);
		cih = NULL;

		if (gcry != 0) {
			g_warning ("couldn't decrypt pkcs8 data: %s", gcry_strerror (gcry));
			break;
		}

		/* Unpad the DER data */
		l = egg_asn1x_element_length (crypted, n_crypted);
		if (l > 0)
			n_crypted = l;

		cbytes = g_bytes_new_with_free_func (crypted, n_crypted,
						     egg_secure_free, crypted);
		crypted = NULL;

		/* Try to parse the resulting key */
		r = _gcr_parser_parse_der_pkcs8_plain (self, cbytes);
		g_bytes_unref (cbytes);

		if (r != GCR_ERROR_UNRECOGNIZED) {
			ret = r;
			break;
		}

		/* We assume unrecognized data, is a bad encryption key */
	}

 done:
	if (cih)
		gcry_cipher_close (cih);
	egg_asn1x_destroy (asn);
	egg_secure_free (crypted);

	_gcr_parser_pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * PKCS12
 */

static gchar *
parse_pkcs12_bag_friendly_name (GNode *asn)
{
	guint count, i;
	GQuark oid;
	GNode *node;
	GNode *asn_str;
	gchar *result;

	if (asn == NULL)
		return NULL;

	count = egg_asn1x_count (asn);
	for (i = 1; i <= count; i++) {
		oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, i, "type", NULL));
		if (oid == GCR_OID_PKCS9_ATTRIBUTE_FRIENDLY) {
			node = egg_asn1x_node (asn, i, "values", 1, NULL);
			if (node != NULL) {
				asn_str = egg_asn1x_get_any_as_string (node, EGG_ASN1X_BMP_STRING);
				if (asn_str) {
					result = egg_asn1x_get_bmpstring_as_utf8 (asn_str);
					egg_asn1x_destroy (asn_str);
					return result;
				}
			}
		}
	}

	return NULL;
}

static gint
handle_pkcs12_bag (GcrParser *self,
                   GBytes *data)
{
	GNode *asn = NULL;
	gint ret, r;
	guint count = 0;
	GQuark oid;
	GNode *value;
	GBytes *element = NULL;
	gchar *friendly;
	guint i;
	GcrParsed *parsed;

	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode_full (pkix_asn1_tab, "pkcs-12-SafeContents",
	                                        data, EGG_ASN1X_NO_STRICT);
	if (!asn)
		goto done;

	ret = GCR_ERROR_FAILURE;

	/* Get the number of elements in this bag */
	count = egg_asn1x_count (asn);

	/*
	 * Now inside each bag are multiple elements. Who comes up
	 * with this stuff?
	 */
	for (i = 1; i <= count; i++) {

		oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, i, "bagId", NULL));
		if (!oid)
			goto done;

		value = egg_asn1x_node (asn, i, "bagValue", NULL);
		if (!value)
			goto done;

		element = egg_asn1x_get_element_raw (value);
		parsed = _gcr_parser_push_parsed (self, FALSE);

		friendly = parse_pkcs12_bag_friendly_name (egg_asn1x_node (asn, i, "bagAttributes", NULL));
		if (friendly != NULL) {
			_gcr_parsed_set_label (parsed, friendly);
			g_free (friendly);
		}

		/* A normal unencrypted key */
		if (oid == GCR_OID_PKCS12_BAG_PKCS8_KEY) {
			r = _gcr_parser_parse_der_pkcs8_plain (self, element);

			/* A properly encrypted key */
		} else if (oid == GCR_OID_PKCS12_BAG_PKCS8_ENCRYPTED_KEY) {
			r = _gcr_parser_parse_der_pkcs8_encrypted (self, element);

			/* A certificate */
		} else if (oid == GCR_OID_PKCS12_BAG_CERTIFICATE) {
			r = _gcr_parser_handle_pkcs12_cert_bag (self, element);

			/* TODO: GCR_OID_PKCS12_BAG_CRL */
		} else {
			r = GCR_ERROR_UNRECOGNIZED;
		}

		if (element != NULL)
			g_bytes_unref (element);

		_gcr_parser_pop_parsed (self, parsed);

		if (r == GCR_ERROR_FAILURE ||
		    r == GCR_ERROR_CANCELLED ||
		    r == GCR_ERROR_LOCKED) {
			ret = r;
			goto done;
		}
	}

	ret = SUCCESS;

 done:
	egg_asn1x_destroy (asn);
	return ret;
}

static gint
handle_pkcs12_encrypted_bag (GcrParser *self,
                             GNode *bag)
{
	PasswordState pstate = PASSWORD_STATE_INIT;
	GNode *asn = NULL;
	gcry_cipher_hd_t cih = NULL;
	gcry_error_t gcry;
	guchar *crypted = NULL;
	GNode *params = NULL;
	gsize n_crypted;
	const gchar *password;
	GBytes *cbytes;
	GQuark scheme;
	gint ret, r;
	gint l;

	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_get_any_as_full (bag, pkix_asn1_tab, "pkcs-7-EncryptedData",
	                                 EGG_ASN1X_NO_STRICT);
	if (!asn)
		goto done;

	ret = GCR_ERROR_FAILURE;

	/* Check the encryption schema OID */
	scheme = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, "encryptedContentInfo", "contentEncryptionAlgorithm", "algorithm", NULL));
	if (!scheme)
		goto done;

	params = egg_asn1x_node (asn, "encryptedContentInfo", "contentEncryptionAlgorithm", "parameters", NULL);
	if (!params)
		goto done;

	/* Loop to try different passwords */
	for (;;) {

		g_assert (cih == NULL);

		r = _gcr_enum_next_password (self, &pstate, &password);
		if (r != SUCCESS) {
			ret = r;
			goto done;
		}

		/* Parse the encryption stuff into a cipher. */
		if (!egg_symkey_read_cipher (scheme, password, -1, params, &cih)) {
			ret = GCR_ERROR_FAILURE;
			goto done;
		}

		crypted = egg_asn1x_get_string_as_raw (egg_asn1x_node (asn, "encryptedContentInfo", "encryptedContent", NULL),
		                                       egg_secure_realloc, &n_crypted);
		if (!crypted)
			goto done;

		gcry = gcry_cipher_decrypt (cih, crypted, n_crypted, NULL, 0);
		gcry_cipher_close (cih);
		cih = NULL;

		if (gcry != 0) {
			g_warning ("couldn't decrypt pkcs12 data: %s", gcry_strerror (gcry));
			goto done;
		}

		/* Unpad the DER data */
		l = egg_asn1x_element_length (crypted, n_crypted);
		if (l > 0)
			n_crypted = l;

		cbytes = g_bytes_new_with_free_func (crypted, n_crypted, egg_secure_free, crypted);
		crypted = NULL;

		/* Try to parse the resulting key */
		r = handle_pkcs12_bag (self, cbytes);
		g_bytes_unref (cbytes);

		if (r != GCR_ERROR_UNRECOGNIZED) {
			ret = r;
			break;
		}

		/* We assume unrecognized data is a bad encryption key */
	}

 done:
	if (cih)
		gcry_cipher_close (cih);
	egg_asn1x_destroy (asn);
	egg_secure_free (crypted);
	return ret;
}

static gint
handle_pkcs12_safe (GcrParser *self,
                    GBytes *data)
{
	GNode *asn = NULL;
	GNode *asn_content = NULL;
	gint ret, r;
	GNode *bag;
	GBytes *content;
	GQuark oid;
	guint i;
	GNode *node;

	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode_full (pkix_asn1_tab, "pkcs-12-AuthenticatedSafe",
	                                        data, EGG_ASN1X_NO_STRICT);
	if (!asn)
		goto done;

	ret = GCR_ERROR_FAILURE;

	/*
	 * Inside each PKCS12 safe there are multiple bags.
	 */
	for (i = 0; TRUE; ++i) {
		node = egg_asn1x_node (asn, i + 1, "contentType", NULL);

		/* All done? no more bags */
		if (!node)
			break;

		oid = egg_asn1x_get_oid_as_quark (node);

		bag = egg_asn1x_node (asn, i + 1, "content", NULL);
		if (!bag)
			goto done;

		/* A non encrypted bag, just parse */
		if (oid == GCR_OID_PKCS7_DATA) {

			egg_asn1x_destroy (asn_content);
			asn_content = egg_asn1x_get_any_as_full (bag, pkix_asn1_tab,
			                                         "pkcs-7-Data", EGG_ASN1X_NO_STRICT);
			if (!asn_content)
				goto done;

			content = egg_asn1x_get_string_as_bytes (asn_content);
			if (!content)
				goto done;

			r = handle_pkcs12_bag (self, content);
			g_bytes_unref (content);

			/* Encrypted data first needs decryption */
		} else if (oid == GCR_OID_PKCS7_ENCRYPTED_DATA) {
			r = handle_pkcs12_encrypted_bag (self, bag);

			/* Hmmmm, not sure what this is */
		} else {
			g_warning ("unrecognized type of safe content in pkcs12: %s", g_quark_to_string (oid));
			r = GCR_ERROR_UNRECOGNIZED;
		}

		if (r == GCR_ERROR_FAILURE ||
		    r == GCR_ERROR_CANCELLED ||
		    r == GCR_ERROR_LOCKED) {
			ret = r;
			goto done;
		}
	}

	ret = SUCCESS;

 done:
	egg_asn1x_destroy (asn);
	egg_asn1x_destroy (asn_content);
	return ret;
}

static gint
verify_pkcs12_safe (GcrParser *self,
                    GNode *asn,
                    GBytes *content)
{
	PasswordState pstate = PASSWORD_STATE_INIT;
	const gchar *password;
	gcry_md_hd_t mdh = NULL;
	const guchar *mac_digest;
	gsize mac_len;
	guchar *digest = NULL;
	gsize n_digest;
	GQuark algorithm;
	GNode *mac_data;
	int ret, r;

	ret = GCR_ERROR_FAILURE;

	/*
	 * The MAC is optional (and outside the encryption no less). I wonder
	 * what the designers (ha) of PKCS#12 were trying to achieve
	 */

	mac_data = egg_asn1x_node (asn, "macData", NULL);
	if (mac_data == NULL)
		return SUCCESS;

	algorithm = egg_asn1x_get_oid_as_quark (egg_asn1x_node (mac_data, "mac",
								"digestAlgorithm", "algorithm", NULL));
	if (!algorithm)
		goto done;

	digest = egg_asn1x_get_string_as_raw (egg_asn1x_node (mac_data, "mac", "digest", NULL), NULL, &n_digest);
	if (!digest)
		goto done;

	/* Loop to try different passwords */
	for (;;) {
		g_assert (mdh == NULL);

		r = _gcr_enum_next_password (self, &pstate, &password);
		if (r != SUCCESS) {
			ret = r;
			goto done;
		}

		/* Parse the encryption stuff into a cipher. */
		if (!egg_symkey_read_mac (algorithm, password, -1, mac_data, &mdh, &mac_len)) {
			ret = GCR_ERROR_FAILURE;
			goto done;
		}

		/* If not the right length, then that's really broken */
		if (mac_len != n_digest) {
			r = GCR_ERROR_FAILURE;

		} else {
			gcry_md_write (mdh, g_bytes_get_data (content, NULL), g_bytes_get_size (content));
			mac_digest = gcry_md_read (mdh, 0);
			g_return_val_if_fail (mac_digest, GCR_ERROR_FAILURE);
			r = memcmp (mac_digest, digest, n_digest) == 0 ? SUCCESS : GCR_ERROR_LOCKED;
		}

		gcry_md_close (mdh);
		mdh = NULL;

		if (r != GCR_ERROR_LOCKED) {
			ret = r;
			break;
		}
	}

 done:
	if (mdh)
		gcry_md_close (mdh);
	g_free (digest);
	return ret;

}

gint
_gcr_parser_parse_der_pkcs12 (GcrParser *self,
			      GBytes *data)
{
	GNode *asn = NULL;
	gint ret;
	GNode *content = NULL;
	GBytes *string = NULL;
	GQuark oid;
	GcrParsed *parsed;

	parsed = _gcr_parser_push_parsed (self, FALSE);
	ret = GCR_ERROR_UNRECOGNIZED;

	/*
	 * Because PKCS#12 files, the bags specifically, are notorious for
	 * being crappily constructed and are often break rules such as DER
	 * sorting order etc.. we parse the DER in a non-strict fashion.
	 *
	 * The rules in DER are designed for X.509 certificates, so there is
	 * only one way to represent a given certificate (although they fail
	 * at that as well). But with PKCS#12 we don't have such high
	 * requirements, and we can slack off on our validation.
	 */

	asn = egg_asn1x_create_and_decode_full (pkix_asn1_tab, "pkcs-12-PFX",
	                                        data, EGG_ASN1X_NO_STRICT);
	if (!asn)
		goto done;

	_gcr_parsed_parsing_block (parsed, GCR_FORMAT_DER_PKCS12, data);

	oid = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, "authSafe", "contentType", NULL));
	if (!oid)
		goto done;

	/* Outer most one must just be plain data */
	if (oid != GCR_OID_PKCS7_DATA) {
		g_message ("unsupported safe content type in pkcs12: %s", g_quark_to_string (oid));
		goto done;
	}

	content = egg_asn1x_get_any_as (egg_asn1x_node (asn, "authSafe", "content", NULL),
	                                pkix_asn1_tab, "pkcs-7-Data");
	if (!content)
		goto done;

	string = egg_asn1x_get_string_as_bytes (content);
	if (!string)
		goto done;

	ret = verify_pkcs12_safe (self, asn, string);
	if (ret == SUCCESS)
		ret = handle_pkcs12_safe (self, string);

 done:
	if (content)
		egg_asn1x_destroy (content);
	if (string)
		g_bytes_unref (string);
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
	guchar *decrypted;
	gsize n_decrypted;
	const gchar *val;
	GBytes *dbytes;
	gint res;
	gint l;

	g_assert (GCR_IS_PARSER (self));
	g_assert (headers);

	val = g_hash_table_lookup (headers, "DEK-Info");
	if (!val) {
		g_message ("missing encryption header");
		return GCR_ERROR_FAILURE;
	}

	for (;;) {

		res = _gcr_enum_next_password (self, &pstate, &password);
		if (res != SUCCESS)
			break;

		/* Decrypt, this will result in garble if invalid password */
		decrypted = egg_openssl_decrypt_block (val, password, -1, data, &n_decrypted);
		if (!decrypted) {
			res = GCR_ERROR_FAILURE;
			break;
		}

		/* Unpad the DER data */
		l = egg_asn1x_element_length (decrypted, n_decrypted);
		if (l > 0)
			n_decrypted = l;

		dbytes = g_bytes_new_with_free_func (decrypted, n_decrypted,
						     egg_secure_free, decrypted);
		decrypted = NULL;

		/* Try to parse */
		res = _gcr_parser_handle_plain_pem (self, format_id, want_format, dbytes);
		g_bytes_unref (dbytes);

		/* Unrecognized is a bad password */
		if (res != GCR_ERROR_UNRECOGNIZED)
			break;
	}

	return res;
}
