/*
 * gnome-keyring
 *
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "gcr-openssh.h"
#include "gcr-internal.h"
#include "gcr-types.h"

#include "gcr/gcr-oids.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-buffer.h"
#include "egg/egg-decimal.h"

#include <p11-kit/pkcs11.h>

#include <string.h>

typedef struct {
	GcrOpensshPubCallback callback;
	gpointer user_data;
} OpensshPubClosure;

static void
skip_spaces (const gchar ** line,
             gsize *n_line)
{
	while (*n_line > 0 && (*line)[0] == ' ') {
		(*line)++;
		(*n_line)--;
	}
}

static gboolean
next_word (const gchar **line,
           gsize *n_line,
           const gchar **word,
           gsize *n_word)
{
	const gchar *beg;
	const gchar *end;
	const gchar *at;
	gboolean quotes;

	skip_spaces (line, n_line);

	if (!*n_line) {
		*word = NULL;
		*n_word = 0;
		return FALSE;
	}

	beg = at = *line;
	end = beg + *n_line;
	quotes = FALSE;

	do {
		switch (*at) {
		case '"':
			quotes = !quotes;
			at++;
			break;
		case ' ':
			if (!quotes)
				end = at;
			else
				at++;
			break;
		default:
			at++;
			break;
		}
	} while (at < end);

	*word = beg;
	*n_word = end - beg;
	(*line) += *n_word;
	(*n_line) -= *n_word;
	return TRUE;
}

static gboolean
match_word (const gchar *word,
            gsize n_word,
            const gchar *matches)
{
	gsize len = strlen (matches);
	if (len != n_word)
		return FALSE;
	return memcmp (word, matches, n_word) == 0;
}

static gulong
keytype_to_algo (const gchar *algo,
                 gsize length)
{
	if (!algo)
		return G_MAXULONG;
	else if (match_word (algo, length, "ssh-rsa"))
		return CKK_RSA;
	else if (match_word (algo, length, "ssh-dss"))
		return CKK_DSA;
	else if (length >= 6 && strncmp (algo, "ecdsa-", 6) == 0)
		return CKK_ECDSA;
	return G_MAXULONG;
}

static gboolean
read_decimal_mpi (const gchar *decimal,
                  gsize n_decimal,
                  GckBuilder *builder,
                  gulong attribute_type)
{
	gpointer data;
	gsize n_data;

	data = egg_decimal_decode (decimal, n_decimal, &n_data);
	if (data == NULL)
		return FALSE;

	gck_builder_add_data (builder, attribute_type, data, n_data);
	g_free (data);
	return TRUE;
}

static gint
atoin (const char *p, gint digits)
{
	gint ret = 0, base = 1;
	while(--digits >= 0) {
		if (p[digits] < '0' || p[digits] > '9')
			return -1;
		ret += (p[digits] - '0') * base;
		base *= 10;
	}
	return ret;
}

static GcrDataError
parse_v1_public_line (const gchar *line,
                      gsize length,
                      GBytes *backing,
                      GcrOpensshPubCallback callback,
                      gpointer user_data)
{
	const gchar *word_bits, *word_exponent, *word_modulus, *word_options, *outer;
	gsize len_bits, len_exponent, len_modulus, len_options, n_outer;
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	gchar *label, *options;
	GBytes *bytes;
	gint bits;

	g_assert (line);

	outer = line;
	n_outer = length;
	options = NULL;
	label = NULL;

	/* Eat space at the front */
	skip_spaces (&line, &length);

	/* Blank line or comment */
	if (length == 0 || line[0] == '#')
		return GCR_ERROR_UNRECOGNIZED;

	/*
	 * If the line starts with a digit, then no options:
	 *
	 * 2048 35 25213680043....93533757 Label
	 *
	 * If the line doesn't start with a digit, then have options:
	 *
	 * option,option 2048 35 25213680043....93533757 Label
	 */
	if (g_ascii_isdigit (line[0])) {
		word_options = NULL;
		len_options = 0;
	} else {
		if (!next_word (&line, &length, &word_options, &len_options))
			return GCR_ERROR_UNRECOGNIZED;
	}

	if (!next_word (&line, &length, &word_bits, &len_bits) ||
	    !next_word (&line, &length, &word_exponent, &len_exponent) ||
	    !next_word (&line, &length, &word_modulus, &len_modulus))
		return GCR_ERROR_UNRECOGNIZED;

	bits = atoin (word_bits, len_bits);
	if (bits <= 0)
		return GCR_ERROR_UNRECOGNIZED;

	if (!read_decimal_mpi (word_exponent, len_exponent, &builder, CKA_PUBLIC_EXPONENT) ||
	    !read_decimal_mpi (word_modulus, len_modulus, &builder, CKA_MODULUS)) {
		gck_builder_clear (&builder);
		return GCR_ERROR_UNRECOGNIZED;
	}

	gck_builder_add_ulong (&builder, CKA_KEY_TYPE, CKK_RSA);
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_PUBLIC_KEY);

	skip_spaces (&line, &length);
	if (length > 0) {
		label = g_strndup (line, length);
		g_strstrip (label);
		gck_builder_add_string (&builder, CKA_LABEL, label);
	}

	if (word_options)
		options = g_strndup (word_options, len_options);

	attrs = gck_builder_end (&builder);

	if (callback != NULL) {
		bytes = g_bytes_new_with_free_func (outer, n_outer,
		                                    (GDestroyNotify)g_bytes_unref,
		                                    g_bytes_ref (backing));
		(callback) (attrs, label, options, bytes, user_data);
		g_bytes_unref (bytes);
	}

	gck_attributes_unref (attrs);
	g_free (options);
	g_free (label);
	return GCR_SUCCESS;
}

static gboolean
read_buffer_mpi_to_der (EggBuffer *buffer,
                        gsize *offset,
                        GckBuilder *builder,
                        gulong attribute_type)
{
	const guchar *data, *data_value;
	GBytes *der_data = NULL;
	gsize len, data_len;
	GNode *asn = NULL;
	gboolean rv = FALSE;

	if (!egg_buffer_get_byte_array (buffer, *offset, offset, &data, &len))
		return FALSE;

	asn = egg_asn1x_create (pk_asn1_tab, "ECPoint");
	if (!asn)
		return FALSE;

	egg_asn1x_set_string_as_raw (asn, (guchar *)data, len, NULL);
	der_data = egg_asn1x_encode (asn, g_realloc);
	if (!der_data)
		goto out;

	data_value = g_bytes_get_data (der_data, &data_len);
	gck_builder_add_data (builder, attribute_type, data_value, data_len);
	rv = TRUE;
out:
	g_bytes_unref (der_data);
	egg_asn1x_destroy (asn);
	return rv;
}

static gboolean
read_buffer_mpi (EggBuffer *buffer,
                 gsize *offset,
                 GckBuilder *builder,
                 gulong attribute_type)
{
	const guchar *data;
	gsize len;

	if (!egg_buffer_get_byte_array (buffer, *offset, offset, &data, &len))
		return FALSE;

	gck_builder_add_data (builder, attribute_type, data, len);
	return TRUE;
}

static gboolean
read_v2_public_dsa (EggBuffer *buffer,
                    gsize *offset,
                    GckBuilder *builder)
{
	if (!read_buffer_mpi (buffer, offset, builder, CKA_PRIME) ||
	    !read_buffer_mpi (buffer, offset, builder, CKA_SUBPRIME) ||
	    !read_buffer_mpi (buffer, offset, builder, CKA_BASE) ||
	    !read_buffer_mpi (buffer, offset, builder, CKA_VALUE)) {
		return FALSE;
	}

	gck_builder_add_ulong (builder, CKA_KEY_TYPE, CKK_DSA);
	gck_builder_add_ulong (builder, CKA_CLASS, CKO_PUBLIC_KEY);

	return TRUE;
}

static gboolean
read_v2_public_rsa (EggBuffer *buffer,
                    gsize *offset,
                    GckBuilder *builder)
{
	if (!read_buffer_mpi (buffer, offset, builder, CKA_PUBLIC_EXPONENT) ||
	    !read_buffer_mpi (buffer, offset, builder, CKA_MODULUS)) {
		return FALSE;
	}

	gck_builder_add_ulong (builder, CKA_KEY_TYPE, CKK_RSA);
	gck_builder_add_ulong (builder, CKA_CLASS, CKO_PUBLIC_KEY);

	return TRUE;
}

static gboolean
read_v2_public_ecdsa (EggBuffer *buffer,
                      gsize *offset,
                      GckBuilder *builder)
{
	gconstpointer data;
	GBytes *bytes;
	GNode *asn;
	GNode *node;
	gchar *curve;
	GQuark oid;
	gsize len;

	/* The named curve */
	if (!egg_buffer_get_string (buffer, *offset, offset,
	                            &curve, (EggBufferAllocator)g_realloc))
		return FALSE;

	if (g_strcmp0 (curve, "nistp256") == 0) {
		oid = GCR_OID_EC_SECP256R1;
	} else if (g_strcmp0 (curve, "nistp384") == 0) {
		oid = GCR_OID_EC_SECP384R1;
	} else if (g_strcmp0 (curve, "nistp521") == 0) {
		oid = GCR_OID_EC_SECP521R1;
	} else {
		g_free (curve);
		g_message ("unknown or unsupported curve in ssh public key");
		return FALSE;
	}

	g_free (curve);

	asn = egg_asn1x_create (pk_asn1_tab, "ECParameters");
	g_return_val_if_fail (asn != NULL, FALSE);

	node = egg_asn1x_node (asn, "namedCurve", NULL);
	if (!egg_asn1x_set_choice (asn, node))
		g_return_val_if_reached (FALSE);

	if (!egg_asn1x_set_oid_as_quark (node, oid))
		g_return_val_if_reached (FALSE);

	bytes = egg_asn1x_encode (asn, g_realloc);
	g_return_val_if_fail (bytes != NULL, FALSE);
	egg_asn1x_destroy (asn);

	data = g_bytes_get_data (bytes, &len);
	gck_builder_add_data (builder, CKA_EC_PARAMS, data, len);
	g_bytes_unref (bytes);

	/* need to convert to DER encoded OCTET STRING */
	if (!read_buffer_mpi_to_der (buffer, offset, builder, CKA_EC_POINT))
		return FALSE;

	gck_builder_add_ulong (builder, CKA_KEY_TYPE, CKK_ECDSA);
	gck_builder_add_ulong (builder, CKA_CLASS, CKO_PUBLIC_KEY);

	return TRUE;
}

static gboolean
read_v2_public_key (gulong algo,
                    gconstpointer data,
                    gsize n_data,
                    GckBuilder *builder)
{
	EggBuffer buffer;
	gboolean ret;
	gsize offset;
	gchar *stype;
	int alg;

	egg_buffer_init_static (&buffer, data, n_data);
	offset = 0;

	/* The string algorithm */
	if (!egg_buffer_get_string (&buffer, offset, &offset,
	                            &stype, (EggBufferAllocator)g_realloc))
		return FALSE;

	alg = keytype_to_algo (stype, stype ? strlen (stype) : 0);
	g_free (stype);

	if (alg != algo) {
		g_message ("invalid or mis-matched algorithm in ssh public key: %s", stype);
		egg_buffer_uninit (&buffer);
		return FALSE;
	}

	switch (algo) {
	case CKK_RSA:
		ret = read_v2_public_rsa (&buffer, &offset, builder);
		break;
	case CKK_DSA:
		ret = read_v2_public_dsa (&buffer, &offset, builder);
		break;
	case CKK_ECDSA:
		ret = read_v2_public_ecdsa (&buffer, &offset, builder);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	egg_buffer_uninit (&buffer);
	return ret;
}

static gboolean
decode_v2_public_key (gulong algo,
                      const gchar *data,
                      gsize n_data,
                      GckBuilder *builder)
{
	gpointer decoded;
	gsize n_decoded;
	gboolean ret;
	guint save;
	gint state;

	/* Decode the base64 key */
	save = state = 0;
	decoded = g_malloc (n_data * 3 / 4);
	n_decoded = g_base64_decode_step ((gchar*)data, n_data, decoded, &state, &save);

	if (!n_decoded) {
		g_free (decoded);
		return FALSE;
	}

	/* Parse the actual key */
	ret = read_v2_public_key (algo, decoded, n_decoded, builder);

	g_free (decoded);

	return ret;
}

static GcrDataError
parse_v2_public_line (const gchar *line,
                      gsize length,
                      GBytes *backing,
                      GcrOpensshPubCallback callback,
                      gpointer user_data)
{
	const gchar *word_options, *word_algo, *word_key;
	gsize len_options, len_algo, len_key;
	GckBuilder builder = GCK_BUILDER_INIT;
	GckAttributes *attrs;
	gchar *options;
	gchar *label = NULL;
	const gchar *outer = line;
	gsize n_outer = length;
	GBytes *bytes;
	gulong algo;

	g_assert (line);

	/* Eat space at the front */
	skip_spaces (&line, &length);

	/* Blank line or comment */
	if (length == 0 || line[0] == '#')
		return GCR_ERROR_UNRECOGNIZED;

	if (!next_word (&line, &length, &word_algo, &len_algo))
		return GCR_ERROR_UNRECOGNIZED;

	/*
	 * If the first word is not the algorithm, then we have options:
	 *
	 * option,option ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAI...EAz8Ji= Label here
	 *
	 * If the first word is the algorithm, then we have no options:
	 *
	 * ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAI...EAz8Ji= Label here
	 */
	algo = keytype_to_algo (word_algo, len_algo);
	if (algo == G_MAXULONG) {
		word_options = word_algo;
		len_options = len_algo;
		if (!next_word (&line, &length, &word_algo, &len_algo))
			return GCR_ERROR_UNRECOGNIZED;
		algo = keytype_to_algo (word_algo, len_algo);
		if (algo == G_MAXULONG)
			return GCR_ERROR_UNRECOGNIZED;
	} else {
		word_options = NULL;
		len_options = 0;
	}

	/* Must have at least two words */
	if (!next_word (&line, &length, &word_key, &len_key))
		return GCR_ERROR_FAILURE;

	if (!decode_v2_public_key (algo, word_key, len_key, &builder)) {
		gck_builder_clear (&builder);
		return GCR_ERROR_FAILURE;
	}

	if (word_options)
		options = g_strndup (word_options, len_options);
	else
		options = NULL;

	/* The remainder of the line is the label */
	skip_spaces (&line, &length);
	if (length > 0) {
		label = g_strndup (line, length);
		g_strstrip (label);
		gck_builder_add_string (&builder, CKA_LABEL, label);
	}

	attrs = gck_builder_end (&builder);

	if (callback != NULL) {
		bytes = g_bytes_new_with_free_func (outer, n_outer,
		                                    (GDestroyNotify)g_bytes_unref,
		                                    g_bytes_ref (backing));
		(callback) (attrs, label, options, bytes, user_data);
		g_bytes_unref (bytes);
	}

	gck_attributes_unref (attrs);
	g_free (options);
	g_free (label);
	return GCR_SUCCESS;
}

guint
_gcr_openssh_pub_parse (GBytes *data,
                        GcrOpensshPubCallback callback,
                        gpointer user_data)
{
	const gchar *line;
	const gchar *end;
	gsize length;
	gboolean last;
	GcrDataError res;
	guint num_parsed;

	g_return_val_if_fail (data != NULL, FALSE);

	line = g_bytes_get_data (data, NULL);
	length = g_bytes_get_size (data);
	last = FALSE;
	num_parsed = 0;

	for (;;) {
		end  = memchr (line, '\n', length);
		if (end == NULL) {
			end = line + length;
			last = TRUE;
		}

		if (line != end) {
			res = parse_v2_public_line (line, end - line, data, callback, user_data);
			if (res == GCR_ERROR_UNRECOGNIZED)
				res = parse_v1_public_line (line, end - line, data, callback, user_data);
			if (res == GCR_SUCCESS)
				num_parsed++;
		}

		if (last)
			break;

		end++;
		length -= (end - line);
		line = end;
	}

	return num_parsed;
}
