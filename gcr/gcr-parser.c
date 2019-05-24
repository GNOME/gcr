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

#include "gck/gck.h"

#include "gcr-internal.h"
#include "gcr-openpgp.h"
#include "gcr-openssh.h"
#include "gcr-parser.h"
#include "gcr-record.h"
#include "gcr-types.h"

#include "gcr/gcr-marshal.h"
#include "gcr/gcr-oids.h"

#include "egg/egg-armor.h"
#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"
#include "egg/egg-dn.h"
#include "egg/egg-openssl.h"
#include "egg/egg-secure-memory.h"
#include "egg/egg-symkey.h"

#include <glib/gi18n-lib.h>

#include <stdlib.h>
#include <gcrypt.h>

/**
 * SECTION:gcr-parser
 * @title: GcrParser
 * @short_description: Parser for certificate and key files
 *
 * A #GcrParser can parse various certificate and key files such as OpenSSL
 * PEM files, DER encoded certifictes, PKCS\#8 keys and so on. Each various
 * format is identified by a value in the #GcrDataFormat enumeration.
 *
 * In order to parse data, a new parser is created with gcr_parser_new() and
 * then the #GcrParser::authenticate and #GcrParser::parsed signals should be
 * connected to. Data is then fed to the parser via gcr_parser_parse_data()
 * or gcr_parser_parse_stream().
 *
 * During the #GcrParser::parsed signal the attributes that make up the currently
 * parsed item can be retrieved using the gcr_parser_get_parsed_attributes()
 * function.
 */

/**
 * GcrParser:
 *
 * A parser for parsing various types of files or data.
 */

/**
 * GcrParsed:
 *
 * A parsed item parsed by a #GcrParser.
 */

/**
 * GcrParserClass:
 * @parent_class: The parent class
 * @authenticate: The default handler for the authenticate signal.
 * @parsed: The default handler for the parsed signal.
 *
 * The class for #GcrParser
 */

/**
 * GCR_DATA_ERROR:
 *
 * A domain for data errors with codes from #GcrDataError
 */

/**
 * GcrDataError:
 * @GCR_ERROR_FAILURE: Failed to parse or serialize the data
 * @GCR_ERROR_UNRECOGNIZED: The data was unrecognized or unsupported
 * @GCR_ERROR_CANCELLED: The operation was cancelled
 * @GCR_ERROR_LOCKED: The data was encrypted or locked and could not be unlocked.
 *
 * Values responding to error codes for parsing and serializing data.
 */

enum {
	PROP_0,
	PROP_PARSED_LABEL,
	PROP_PARSED_ATTRIBUTES,
	PROP_PARSED_DESCRIPTION
};

enum {
	AUTHENTICATE,
	PARSED,
	LAST_SIGNAL
};

#define SUCCESS 0

static guint signals[LAST_SIGNAL] = { 0 };

struct _GcrParsed {
	gint refs;
	GckBuilder builder;
	GckAttributes *attrs;
	const gchar *description;
	gchar *label;
	GBytes *data;
	gboolean sensitive;
	GcrDataFormat format;
	gchar *filename;
	struct _GcrParsed *next;
};

struct _GcrParserPrivate {
	GTree *specific_formats;
	gboolean normal_formats;
	GPtrArray *passwords;
	GcrParsed *parsed;
	gchar *filename;
};

G_DEFINE_TYPE_WITH_PRIVATE (GcrParser, gcr_parser, G_TYPE_OBJECT);

typedef struct {
	gint ask_state;
	gint seen;
} PasswordState;

#define PASSWORD_STATE_INIT { 0, 0 }

typedef struct _ParserFormat {
	gint format_id;
	gint (*function) (GcrParser *self, GBytes *data);
} ParserFormat;

/* Forward declarations */
static const ParserFormat parser_normal[];
static const ParserFormat parser_formats[];
static ParserFormat* parser_format_lookup (gint format_id);

EGG_SECURE_DECLARE (parser);

/* -----------------------------------------------------------------------------
 * QUARK DEFINITIONS
 */

/*
 * PEM STRINGS
 * The xxxxx in: ----- BEGIN xxxxx ------
 */

static GQuark PEM_CERTIFICATE;
static GQuark PEM_RSA_PRIVATE_KEY;
static GQuark PEM_DSA_PRIVATE_KEY;
static GQuark PEM_EC_PRIVATE_KEY;
static GQuark PEM_ANY_PRIVATE_KEY;
static GQuark PEM_ENCRYPTED_PRIVATE_KEY;
static GQuark PEM_PRIVATE_KEY;
static GQuark PEM_PKCS7;
static GQuark PEM_PKCS12;
static GQuark PEM_CERTIFICATE_REQUEST;
static GQuark PEM_PUBLIC_KEY;

static GQuark ARMOR_PGP_PUBLIC_KEY_BLOCK;
static GQuark ARMOR_PGP_PRIVATE_KEY_BLOCK;

static void
init_quarks (void)
{
	static volatile gsize quarks_inited = 0;

	if (g_once_init_enter (&quarks_inited)) {

		#define QUARK(name, value) \
			name = g_quark_from_static_string(value)

		QUARK (PEM_CERTIFICATE, "CERTIFICATE");
		QUARK (PEM_PRIVATE_KEY, "PRIVATE KEY");
		QUARK (PEM_RSA_PRIVATE_KEY, "RSA PRIVATE KEY");
		QUARK (PEM_DSA_PRIVATE_KEY, "DSA PRIVATE KEY");
		QUARK (PEM_EC_PRIVATE_KEY, "EC PRIVATE KEY");
		QUARK (PEM_ANY_PRIVATE_KEY, "ANY PRIVATE KEY");
		QUARK (PEM_ENCRYPTED_PRIVATE_KEY, "ENCRYPTED PRIVATE KEY");
		QUARK (PEM_PKCS7, "PKCS7");
		QUARK (PEM_PKCS12, "PKCS12");
		QUARK (PEM_CERTIFICATE_REQUEST, "CERTIFICATE REQUEST");
		QUARK (PEM_PUBLIC_KEY, "PUBLIC KEY");

		QUARK (ARMOR_PGP_PRIVATE_KEY_BLOCK, "PGP PRIVATE KEY BLOCK");
		QUARK (ARMOR_PGP_PUBLIC_KEY_BLOCK, "PGP PUBLIC KEY BLOCK");

		#undef QUARK

		g_once_init_leave (&quarks_inited, 1);
	}
}

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static void
parsed_attribute (GcrParsed *parsed,
                  CK_ATTRIBUTE_TYPE type,
                  gconstpointer data,
                  gsize n_data)
{
	g_assert (parsed != NULL);
	gck_builder_add_data (&parsed->builder, type, data, n_data);
}

static void
parsed_attribute_bytes (GcrParsed *parsed,
                        CK_ATTRIBUTE_TYPE type,
                        GBytes *data)
{
	g_assert (parsed != NULL);
	gck_builder_add_data (&parsed->builder, type,
	                      g_bytes_get_data (data, NULL),
	                      g_bytes_get_size (data));
}

static gboolean
parsed_asn1_number (GcrParsed *parsed,
                    GNode *asn,
                    const gchar *part,
                    CK_ATTRIBUTE_TYPE type)
{
	GBytes *value;

	g_assert (asn);
	g_assert (parsed);

	value = egg_asn1x_get_integer_as_usg (egg_asn1x_node (asn, part, NULL));
	if (value == NULL)
		return FALSE;

	parsed_attribute_bytes (parsed, type, value);
	g_bytes_unref (value);
	return TRUE;
}

static gboolean
parsed_asn1_element (GcrParsed *parsed,
                     GNode *asn,
                     const gchar *part,
                     CK_ATTRIBUTE_TYPE type)
{
	GBytes *value;

	g_assert (asn);
	g_assert (parsed);

	value = egg_asn1x_get_element_raw (egg_asn1x_node (asn, part, NULL));
	if (value == NULL)
		return FALSE;

	parsed_attribute_bytes (parsed, type, value);
	g_bytes_unref (value);
	return TRUE;
}

static gboolean
parsed_asn1_structure (GcrParsed *parsed,
                      GNode *asn,
                      CK_ATTRIBUTE_TYPE type)
{
	GBytes *value;

	g_assert (asn);
	g_assert (parsed);

	value = egg_asn1x_encode (asn, g_realloc);
	if (value == NULL)
		return FALSE;

	parsed_attribute_bytes (parsed, type, value);
	g_bytes_unref (value);
	return TRUE;
}

static void
parsed_ulong_attribute (GcrParsed *parsed,
                        CK_ATTRIBUTE_TYPE type,
                        gulong value)
{
	g_assert (parsed != NULL);
	gck_builder_add_ulong (&parsed->builder, type, value);
}

static void
parsed_boolean_attribute (GcrParsed *parsed,
                          CK_ATTRIBUTE_TYPE type,
                          gboolean value)
{
	g_assert (parsed != NULL);
	gck_builder_add_boolean (&parsed->builder, type, value);
}


static void
parsing_block (GcrParsed *parsed,
               gint format,
               GBytes *data)
{
	g_assert (parsed != NULL);
	g_assert (data != NULL);
	g_assert (format != 0);
	g_assert (parsed->data == NULL);

	parsed->format = format;
	parsed->data = g_bytes_ref (data);
}

static void
parsed_description (GcrParsed *parsed,
                    CK_OBJECT_CLASS klass)
{
	g_assert (parsed != NULL);
	switch (klass) {
	case CKO_PRIVATE_KEY:
		parsed->description = _("Private Key");
		break;
	case CKO_CERTIFICATE:
		parsed->description = _("Certificate");
		break;
	case CKO_PUBLIC_KEY:
		parsed->description = _("Public Key");
		break;
	case CKO_GCR_GNUPG_RECORDS:
		parsed->description = _("PGP Key");
		break;
	case CKO_GCR_CERTIFICATE_REQUEST:
		parsed->description = _("Certificate Request");
		break;
	default:
		parsed->description = NULL;
		break;
	}
}

static void
parsing_object (GcrParsed *parsed,
                CK_OBJECT_CLASS klass)
{
	g_assert (parsed != NULL);

	gck_builder_clear (&parsed->builder);
	if (parsed->sensitive)
		gck_builder_init_full (&parsed->builder, GCK_BUILDER_SECURE_MEMORY);
	else
		gck_builder_init_full (&parsed->builder, GCK_BUILDER_NONE);
	gck_builder_add_ulong (&parsed->builder, CKA_CLASS, klass);
	parsed_description (parsed, klass);
}

static void
parsed_attributes (GcrParsed *parsed,
                   GckAttributes *attrs)
{
	gulong klass;

	g_assert (parsed != NULL);
	g_assert (attrs != NULL);

	if (gck_attributes_find_ulong (attrs, CKA_CLASS, &klass))
		parsed_description (parsed, klass);
	gck_builder_add_all (&parsed->builder, attrs);
}

static void
parsed_label (GcrParsed *parsed,
              const gchar *label)
{
	g_assert (parsed != NULL);
	g_assert (parsed->label == NULL);
	parsed->label = g_strdup (label);
}

static GcrParsed *
push_parsed (GcrParser *self,
             gboolean sensitive)
{
	GcrParsed *parsed = g_new0 (GcrParsed, 1);
	parsed->refs = 0;
	parsed->sensitive = sensitive;
	parsed->next = self->pv->parsed;
	parsed->filename = g_strdup (gcr_parser_get_filename (self));
	self->pv->parsed = parsed;
	return parsed;
}

static void
_gcr_parsed_free (GcrParsed *parsed)
{
	gck_builder_clear (&parsed->builder);
	if (parsed->attrs)
		gck_attributes_unref (parsed->attrs);
	if (parsed->data)
		g_bytes_unref (parsed->data);
	g_free (parsed->label);
	g_free (parsed->filename);
	g_free (parsed);
}

static void
pop_parsed (GcrParser *self,
            GcrParsed *parsed)
{
	g_assert (parsed == self->pv->parsed);
	self->pv->parsed = parsed->next;
	_gcr_parsed_free (parsed);
}

static gint
enum_next_password (GcrParser *self, PasswordState *state, const gchar **password)
{
	gboolean result;

	/*
	 * Next passes we look through all the passwords that the parser
	 * has seen so far. This is because different parts of a encrypted
	 * container (such as PKCS#12) often use the same password even
	 * if with different algorithms.
	 *
	 * If we didn't do this and the user chooses enters a password,
	 * but doesn't save it, they would get prompted for the same thing
	 * over and over, dumb.
	 */

	/* Look in our list of passwords */
	if (state->seen < self->pv->passwords->len) {
		g_assert (state->seen >= 0);
		*password = g_ptr_array_index (self->pv->passwords, state->seen);
		++state->seen;
		return SUCCESS;
	}

	/* Fire off all the parsed property signals so anyone watching can update their state */
	g_object_notify (G_OBJECT (self), "parsed-description");
	g_object_notify (G_OBJECT (self), "parsed-attributes");
	g_object_notify (G_OBJECT (self), "parsed-label");

	g_signal_emit (self, signals[AUTHENTICATE], 0, state->ask_state, &result);
	++state->ask_state;

	if (!result)
		return GCR_ERROR_CANCELLED;

	/* Return any passwords added */
	if (state->seen < self->pv->passwords->len) {
		g_assert (state->seen >= 0);
		*password = g_ptr_array_index (self->pv->passwords, state->seen);
		++state->seen;
		return SUCCESS;
	}

	return GCR_ERROR_LOCKED;
}

static void
parsed_fire (GcrParser *self,
             GcrParsed *parsed)
{
	g_assert (GCR_IS_PARSER (self));
	g_assert (parsed != NULL);
	g_assert (parsed == self->pv->parsed);
	g_assert (parsed->attrs == NULL);

	parsed->attrs = gck_attributes_ref_sink (gck_builder_end (&parsed->builder));

	g_object_notify (G_OBJECT (self), "parsed-description");
	g_object_notify (G_OBJECT (self), "parsed-attributes");
	g_object_notify (G_OBJECT (self), "parsed-label");

	g_signal_emit (self, signals[PARSED], 0);
}

/* -----------------------------------------------------------------------------
 * RSA PRIVATE KEY
 */

static gint
parse_der_private_key_rsa (GcrParser *self,
                           GBytes *data)
{
	gint res = GCR_ERROR_UNRECOGNIZED;
	GNode *asn = NULL;
	gulong version;
	GcrParsed *parsed;

	parsed = push_parsed (self, TRUE);

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "RSAPrivateKey", data);
	if (!asn)
		goto done;

	parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_RSA, data);
	parsing_object (parsed, CKO_PRIVATE_KEY);
	parsed_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_RSA);
	parsed_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	res = GCR_ERROR_FAILURE;

	if (!egg_asn1x_get_integer_as_ulong (egg_asn1x_node (asn, "version", NULL), &version))
		goto done;

	/* We only support simple version */
	if (version != 0) {
		res = GCR_ERROR_UNRECOGNIZED;
		g_message ("unsupported version of RSA key: %lu", version);
		goto done;
	}

	if (!parsed_asn1_number (parsed, asn, "modulus", CKA_MODULUS) ||
	    !parsed_asn1_number (parsed, asn, "publicExponent", CKA_PUBLIC_EXPONENT) ||
	    !parsed_asn1_number (parsed, asn, "privateExponent", CKA_PRIVATE_EXPONENT) ||
	    !parsed_asn1_number (parsed, asn, "prime1", CKA_PRIME_1) ||
	    !parsed_asn1_number (parsed, asn, "prime2", CKA_PRIME_2) ||
	    !parsed_asn1_number (parsed, asn, "coefficient", CKA_COEFFICIENT))
		goto done;

	parsed_fire (self, parsed);
	res = SUCCESS;

done:
	egg_asn1x_destroy (asn);
	if (res == GCR_ERROR_FAILURE)
		g_message ("invalid RSA key");

	pop_parsed (self, parsed);
	return res;
}

/* -----------------------------------------------------------------------------
 * DSA PRIVATE KEY
 */

static gint
parse_der_private_key_dsa (GcrParser *self,
                           GBytes *data)
{
	gint ret = GCR_ERROR_UNRECOGNIZED;
	GNode *asn = NULL;
	GcrParsed *parsed;

	parsed = push_parsed (self, TRUE);

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "DSAPrivateKey", data);
	if (!asn)
		goto done;

	parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_DSA, data);
	parsing_object (parsed, CKO_PRIVATE_KEY);
	parsed_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_DSA);
	parsed_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	ret = GCR_ERROR_FAILURE;

	if (!parsed_asn1_number (parsed, asn, "p", CKA_PRIME) ||
	    !parsed_asn1_number (parsed, asn, "q", CKA_SUBPRIME) ||
	    !parsed_asn1_number (parsed, asn, "g", CKA_BASE) ||
	    !parsed_asn1_number (parsed, asn, "priv", CKA_VALUE))
		goto done;

	parsed_fire (self, parsed);
	ret = SUCCESS;

done:
	egg_asn1x_destroy (asn);
	if (ret == GCR_ERROR_FAILURE)
		g_message ("invalid DSA key");

	pop_parsed (self, parsed);
	return ret;
}

static gint
parse_der_private_key_dsa_parts (GcrParser *self,
                                 GBytes *keydata,
                                 GNode *params)
{
	gint ret = GCR_ERROR_UNRECOGNIZED;
	GNode *asn_params = NULL;
	GNode *asn_key = NULL;
	GcrParsed *parsed;

	parsed = push_parsed (self, TRUE);

	asn_params = egg_asn1x_get_any_as (params, pk_asn1_tab, "DSAParameters");
	asn_key = egg_asn1x_create_and_decode (pk_asn1_tab, "DSAPrivatePart", keydata);
	if (!asn_params || !asn_key)
		goto done;

	parsing_object (parsed, CKO_PRIVATE_KEY);
	parsed_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_DSA);
	parsed_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	ret = GCR_ERROR_FAILURE;

	if (!parsed_asn1_number (parsed, asn_params, "p", CKA_PRIME) ||
	    !parsed_asn1_number (parsed, asn_params, "q", CKA_SUBPRIME) ||
	    !parsed_asn1_number (parsed, asn_params, "g", CKA_BASE) ||
	    !parsed_asn1_number (parsed, asn_key, NULL, CKA_VALUE))
		goto done;

	parsed_fire (self, parsed);
	ret = SUCCESS;

done:
	egg_asn1x_destroy (asn_key);
	egg_asn1x_destroy (asn_params);
	if (ret == GCR_ERROR_FAILURE)
		g_message ("invalid DSA key");

	pop_parsed (self, parsed);
	return ret;
}
/* -----------------------------------------------------------------------------
 * EC PRIVATE KEY
 */

static gint
parse_der_private_key_ec (GcrParser *self,
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

	parsed = push_parsed (self, TRUE);

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

	parsing_block (parsed, GCR_FORMAT_DER_PRIVATE_KEY_EC, data);
	parsing_object (parsed, CKO_PRIVATE_KEY);
	parsed_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_EC);
	parsed_boolean_attribute (parsed, CKA_PRIVATE, CK_TRUE);
	ret = GCR_ERROR_FAILURE;

	if (!parsed_asn1_element (parsed, asn, "parameters", CKA_EC_PARAMS))
		goto done;

	value = egg_asn1x_get_string_as_usg (egg_asn1x_node (asn, "privateKey", NULL), egg_secure_realloc);
	if (!value)
		goto done;

	parsed_attribute_bytes (parsed, CKA_VALUE, value);

	pub = egg_asn1x_get_bits_as_raw (egg_asn1x_node (asn, "publicKey", NULL), &bits);
	if (!pub || bits != 8 * g_bytes_get_size (pub))
		goto done;
	asn_q = egg_asn1x_create (pk_asn1_tab, "ECPoint");
	if (!asn_q)
		goto done;
	egg_asn1x_set_string_as_bytes (asn_q, pub);

	if (!parsed_asn1_structure (parsed, asn_q, CKA_EC_POINT))
		goto done;

	parsed_fire (self, parsed);
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

	pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * PRIVATE KEY
 */

static gint
parse_der_private_key (GcrParser *self,
                       GBytes *data)
{
	gint res;

	res = parse_der_private_key_rsa (self, data);
	if (res == GCR_ERROR_UNRECOGNIZED)
		res = parse_der_private_key_dsa (self, data);
	if (res == GCR_ERROR_UNRECOGNIZED)
		res = parse_der_private_key_ec (self, data);

	return res;
}

/* -----------------------------------------------------------------------------
 * SUBJECT PUBLIC KEY
 */

static gint
handle_subject_public_key_rsa (GcrParser *self,
                               GcrParsed *parsed,
                               GBytes *key,
                               GNode *params)
{
	gint res = GCR_ERROR_FAILURE;
	GNode *asn = NULL;

	asn = egg_asn1x_create_and_decode (pk_asn1_tab, "RSAPublicKey", key);
	if (!asn)
		goto done;

	parsing_object (parsed, CKO_PUBLIC_KEY);
	parsed_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_RSA);

	if (!parsed_asn1_number (parsed, asn, "modulus", CKA_MODULUS) ||
	    !parsed_asn1_number (parsed, asn, "publicExponent", CKA_PUBLIC_EXPONENT))
		goto done;

	res = SUCCESS;

done:
	egg_asn1x_destroy (asn);
	return res;
}

static gint
handle_subject_public_key_dsa (GcrParser *self,
                               GcrParsed *parsed,
                               GBytes *key,
                               GNode *params)
{
	gint res = GCR_ERROR_FAILURE;
	GNode *key_asn = NULL;
	GNode *param_asn = NULL;

	key_asn = egg_asn1x_create_and_decode (pk_asn1_tab, "DSAPublicPart", key);
	param_asn = egg_asn1x_get_any_as (params, pk_asn1_tab, "DSAParameters");

	if (!key_asn || !param_asn)
		goto done;

	parsing_object (parsed, CKO_PUBLIC_KEY);
	parsed_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_DSA);

	if (!parsed_asn1_number (parsed, param_asn, "p", CKA_PRIME) ||
	    !parsed_asn1_number (parsed, param_asn, "q", CKA_SUBPRIME) ||
	    !parsed_asn1_number (parsed, param_asn, "g", CKA_BASE) ||
	    !parsed_asn1_number (parsed, key_asn, NULL, CKA_VALUE))
		goto done;

	res = SUCCESS;

done:
	egg_asn1x_destroy (key_asn);
	egg_asn1x_destroy (param_asn);
	return res;
}

static gint
handle_subject_public_key_ec (GcrParser *self,
                              GcrParsed *parsed,
                              GBytes *key,
                              GNode *params)
{
	gint ret = GCR_ERROR_FAILURE;
	GBytes *bytes = NULL;
	GNode *asn = NULL;

	parsing_object (parsed, CKO_PUBLIC_KEY);
	parsed_ulong_attribute (parsed, CKA_KEY_TYPE, CKK_EC);

	bytes = egg_asn1x_encode (params, g_realloc);
	parsed_attribute_bytes (parsed, CKA_EC_PARAMS, bytes);
	g_bytes_unref (bytes);

	asn = egg_asn1x_create (pk_asn1_tab, "ECPoint");
	if (!asn)
		goto done;
	egg_asn1x_set_string_as_bytes (asn, key);
	parsed_asn1_structure (parsed, asn, CKA_EC_POINT);
	ret = SUCCESS;
done:
	egg_asn1x_destroy (asn);
	return ret;
}

static gint
parse_der_subject_public_key (GcrParser *self,
                              GBytes *data)
{
	GcrParsed *parsed;
	GNode *params;
	GBytes *key;
	GNode *asn = NULL;
	GNode *node;
	GQuark oid;
	guint bits;
	gint ret;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SubjectPublicKeyInfo", data);
	if (asn == NULL)
		return GCR_ERROR_UNRECOGNIZED;

	parsed = push_parsed (self, TRUE);
	parsing_block (parsed, GCR_FORMAT_DER_SUBJECT_PUBLIC_KEY, data);

	node = egg_asn1x_node (asn, "algorithm", "algorithm", NULL);
	oid = egg_asn1x_get_oid_as_quark (node);

	params = egg_asn1x_node (asn, "algorithm", "parameters", NULL);

	node = egg_asn1x_node (asn, "subjectPublicKey", NULL);
	key = egg_asn1x_get_bits_as_raw (node, &bits);

	if (oid == GCR_OID_PKIX1_RSA)
		ret = handle_subject_public_key_rsa (self, parsed, key, params);

	else if (oid == GCR_OID_PKIX1_DSA)
		ret = handle_subject_public_key_dsa (self, parsed, key, params);

	else if (oid == GCR_OID_PKIX1_EC)
		ret = handle_subject_public_key_ec (self, parsed, key, params);

	else
		ret = GCR_ERROR_UNRECOGNIZED;

	g_bytes_unref (key);

	if (ret == SUCCESS)
		parsed_fire (self, parsed);

	pop_parsed (self, parsed);

	egg_asn1x_destroy (asn);
	return ret;
}

/* -----------------------------------------------------------------------------
 * PKCS8
 */

static gint
parse_der_pkcs8_plain (GcrParser *self,
                       GBytes *data)
{
	gint ret;
	CK_KEY_TYPE key_type;
	GQuark key_algo;
	GBytes *keydata = NULL;
	GNode *params = NULL;
	GNode *asn = NULL;
	GcrParsed *parsed;

	parsed = push_parsed (self, TRUE);
	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "pkcs-8-PrivateKeyInfo", data);
	if (!asn)
		goto done;

	parsing_block (parsed, GCR_FORMAT_DER_PKCS8_PLAIN, data);
	ret = GCR_ERROR_FAILURE;
	key_type = GCK_INVALID;

	key_algo = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, "privateKeyAlgorithm", "algorithm", NULL));
	if (!key_algo)
		goto done;
	else if (key_algo == GCR_OID_PKIX1_RSA)
		key_type = CKK_RSA;
	else if (key_algo == GCR_OID_PKIX1_DSA)
		key_type = CKK_DSA;
	else if (key_algo == GCR_OID_PKIX1_EC)
		key_type = CKK_EC;

	if (key_type == GCK_INVALID) {
  		ret = GCR_ERROR_UNRECOGNIZED;
  		goto done;
  	}

	keydata = egg_asn1x_get_string_as_bytes (egg_asn1x_node (asn, "privateKey", NULL));
	if (!keydata)
		goto done;

	params = egg_asn1x_node (asn, "privateKeyAlgorithm", "parameters", NULL);

	ret = SUCCESS;

done:
	if (ret == SUCCESS) {
		switch (key_type) {
		case CKK_RSA:
			ret = parse_der_private_key_rsa (self, keydata);
			break;
		case CKK_DSA:
			/* Try the normal sane format */
			ret = parse_der_private_key_dsa (self, keydata);

			/* Otherwise try the two part format that everyone seems to like */
			if (ret == GCR_ERROR_UNRECOGNIZED && params)
				ret = parse_der_private_key_dsa_parts (self, keydata, params);
			break;
		case CKK_EC:
			ret = parse_der_private_key_ec (self, keydata);
			break;

		default:
			g_message ("invalid or unsupported key type in PKCS#8 key");
			ret = GCR_ERROR_UNRECOGNIZED;
			break;
		};

	} else if (ret == GCR_ERROR_FAILURE) {
		g_message ("invalid PKCS#8 key");
	}

	if (keydata)
		g_bytes_unref (keydata);
	egg_asn1x_destroy (asn);
	pop_parsed (self, parsed);
	return ret;
}

static gint
parse_der_pkcs8_encrypted (GcrParser *self,
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

	parsed = push_parsed (self, FALSE);
	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "pkcs-8-EncryptedPrivateKeyInfo", data);
	if (!asn)
		goto done;

	parsing_block (parsed, GCR_FORMAT_DER_PKCS8_ENCRYPTED, data);
	ret = GCR_ERROR_FAILURE;

	/* Figure out the type of encryption */
	scheme = egg_asn1x_get_oid_as_quark (egg_asn1x_node (asn, "encryptionAlgorithm", "algorithm", NULL));
	if (!scheme)
		goto done;

	params = egg_asn1x_node (asn, "encryptionAlgorithm", "parameters", NULL);

	/* Loop to try different passwords */
	for (;;) {

		g_assert (cih == NULL);

		r = enum_next_password (self, &pstate, &password);
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
		r = parse_der_pkcs8_plain (self, cbytes);
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

	pop_parsed (self, parsed);
	return ret;
}

static gint
parse_der_pkcs8 (GcrParser *self,
                 GBytes *data)
{
	gint ret;

	ret = parse_der_pkcs8_plain (self, data);
	if (ret == GCR_ERROR_UNRECOGNIZED)
		ret = parse_der_pkcs8_encrypted (self, data);

	return ret;
}

/* -----------------------------------------------------------------------------
 * CERTIFICATE
 */

static gint
parse_der_certificate (GcrParser *self,
                       GBytes *data)
{
	gchar *name = NULL;
	GcrParsed *parsed;
	GNode *node;
	GNode *asn;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "Certificate", data);
	if (asn == NULL)
		return GCR_ERROR_UNRECOGNIZED;

	parsed = push_parsed (self, FALSE);

	parsing_block (parsed, GCR_FORMAT_DER_CERTIFICATE_X509, data);
	parsing_object (parsed, CKO_CERTIFICATE);
	parsed_ulong_attribute (parsed, CKA_CERTIFICATE_TYPE, CKC_X_509);

	node = egg_asn1x_node (asn, "tbsCertificate", NULL);
	g_return_val_if_fail (node != NULL, GCR_ERROR_FAILURE);

	if (gcr_parser_get_parsed_label (self) == NULL)
		name = egg_dn_read_part (egg_asn1x_node (node, "subject", "rdnSequence", NULL), "CN");

	if (name != NULL) {
		parsed_label (parsed, name);
		g_free (name);
	}

	parsed_attribute_bytes (parsed, CKA_VALUE, data);
	parsed_asn1_element (parsed, node, "subject", CKA_SUBJECT);
	parsed_asn1_element (parsed, node, "issuer", CKA_ISSUER);
	parsed_asn1_number (parsed, node, "serialNumber", CKA_SERIAL_NUMBER);
	parsed_fire (self, parsed);

	egg_asn1x_destroy (asn);

	pop_parsed (self, parsed);
	return SUCCESS;
}

/* -----------------------------------------------------------------------------
 * PKCS7
 */

static gint
handle_pkcs7_signed_data (GcrParser *self,
                          GNode *content)
{
	GNode *asn = NULL;
	GNode *node;
	gint ret;
	GBytes *certificate;
	int i;

	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_get_any_as (content, pkix_asn1_tab, "pkcs-7-SignedData");
	if (!asn)
		goto done;

	for (i = 0; TRUE; ++i) {

		node = egg_asn1x_node (asn, "certificates", i + 1, NULL);

		/* No more certificates? */
		if (node == NULL)
			break;

		certificate = egg_asn1x_get_element_raw (node);
		ret = parse_der_certificate (self, certificate);
		g_bytes_unref (certificate);

		if (ret != SUCCESS)
			goto done;
	}

	/* TODO: Parse out all the CRLs */

	ret = SUCCESS;

done:
	egg_asn1x_destroy (asn);
	return ret;
}

static gint
parse_der_pkcs7 (GcrParser *self,
                 GBytes *data)
{
	GNode *asn = NULL;
	GNode *node;
	gint ret;
	GNode *content = NULL;
	GQuark oid;
	GcrParsed *parsed;

	parsed = push_parsed (self, FALSE);
	ret = GCR_ERROR_UNRECOGNIZED;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "pkcs-7-ContentInfo", data);
	if (!asn)
		goto done;

	parsing_block (parsed, GCR_FORMAT_DER_PKCS7, data);
	ret = GCR_ERROR_FAILURE;

	node = egg_asn1x_node (asn, "contentType", NULL);
	if (!node)
		goto done;

	oid = egg_asn1x_get_oid_as_quark (node);
	g_return_val_if_fail (oid, GCR_ERROR_FAILURE);

	/* Outer most one must just be plain data */
	if (oid != GCR_OID_PKCS7_SIGNED_DATA) {
		g_message ("unsupported outer content type in pkcs7: %s", g_quark_to_string (oid));
		goto done;
	}

	content = egg_asn1x_node (asn, "content", NULL);
	if (!content)
		goto done;

	ret = handle_pkcs7_signed_data (self, content);

done:
	egg_asn1x_destroy (asn);
	pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * PKCS12
 */

static gint
handle_pkcs12_cert_bag (GcrParser *self,
                        GBytes *data)
{
	GNode *asn = NULL;
	GNode *asn_content = NULL;
	guchar *certificate = NULL;
	GNode *element = NULL;
	gsize n_certificate;
	GBytes *bytes;
	gint ret;

	ret = GCR_ERROR_UNRECOGNIZED;
	asn = egg_asn1x_create_and_decode_full (pkix_asn1_tab, "pkcs-12-CertBag",
	                                        data, EGG_ASN1X_NO_STRICT);
	if (!asn)
		goto done;

	ret = GCR_ERROR_FAILURE;

	element = egg_asn1x_node (asn, "certValue", NULL);
	if (!element)
		goto done;

	asn_content = egg_asn1x_get_any_as (element, pkix_asn1_tab, "pkcs-7-Data");
	if (!asn_content)
		goto done;

	certificate = egg_asn1x_get_string_as_raw (asn_content, NULL, &n_certificate);
	if (!certificate)
		goto done;

	bytes = g_bytes_new_take (certificate, n_certificate);
	ret = parse_der_certificate (self, bytes);
	g_bytes_unref (bytes);

done:
	egg_asn1x_destroy (asn_content);
	egg_asn1x_destroy (asn);
	return ret;
}

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
				asn_str = egg_asn1x_get_any_as (node, pkix_asn1_tab, "BMPString");
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
		parsed = push_parsed (self, FALSE);

		friendly = parse_pkcs12_bag_friendly_name (egg_asn1x_node (asn, i, "bagAttributes", NULL));
		if (friendly != NULL) {
			parsed_label (parsed, friendly);
			g_free (friendly);
		}

		/* A normal unencrypted key */
		if (oid == GCR_OID_PKCS12_BAG_PKCS8_KEY) {
			r = parse_der_pkcs8_plain (self, element);

		/* A properly encrypted key */
		} else if (oid == GCR_OID_PKCS12_BAG_PKCS8_ENCRYPTED_KEY) {
			r = parse_der_pkcs8_encrypted (self, element);

		/* A certificate */
		} else if (oid == GCR_OID_PKCS12_BAG_CERTIFICATE) {
			r = handle_pkcs12_cert_bag (self, element);

		/* TODO: GCR_OID_PKCS12_BAG_CRL */
		} else {
			r = GCR_ERROR_UNRECOGNIZED;
		}

		if (element != NULL)
			g_bytes_unref (element);

		pop_parsed (self, parsed);

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

		r = enum_next_password (self, &pstate, &password);
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

		r = enum_next_password (self, &pstate, &password);
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

static gint
parse_der_pkcs12 (GcrParser *self,
                  GBytes *data)
{
	GNode *asn = NULL;
	gint ret;
	GNode *content = NULL;
	GBytes *string = NULL;
	GQuark oid;
	GcrParsed *parsed;

	parsed = push_parsed (self, FALSE);
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

	parsing_block (parsed, GCR_FORMAT_DER_PKCS12, data);

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
	pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * CERTIFICATE REQUESTS
 */

static gint
parse_der_pkcs10 (GcrParser *self,
                  GBytes *data)
{
	GNode *asn = NULL;
	GNode *node;
	GcrParsed *parsed;
	gchar *name = NULL;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "pkcs-10-CertificationRequest", data);
	if (!asn)
		return GCR_ERROR_UNRECOGNIZED;

	parsed = push_parsed (self, FALSE);
	parsing_block (parsed, GCR_FORMAT_DER_PKCS10, data);

	parsing_object (parsed, CKO_GCR_CERTIFICATE_REQUEST);
	parsed_ulong_attribute (parsed, CKA_GCR_CERTIFICATE_REQUEST_TYPE, CKQ_GCR_PKCS10);

	node = egg_asn1x_node (asn, "certificationRequestInfo", NULL);
	g_return_val_if_fail (node != NULL, GCR_ERROR_FAILURE);

	if (gcr_parser_get_parsed_label (self) == NULL)
		name = egg_dn_read_part (egg_asn1x_node (node, "subject", "rdnSequence", NULL), "CN");

	if (name != NULL) {
		parsed_label (parsed, name);
		g_free (name);
	}

	parsed_attribute_bytes (parsed, CKA_VALUE, data);
	parsed_asn1_element (parsed, node, "subject", CKA_SUBJECT);
	parsed_fire (self, parsed);

	egg_asn1x_destroy (asn);

	pop_parsed (self, parsed);
	return SUCCESS;
}

static gint
parse_der_spkac (GcrParser *self,
                 GBytes *data)
{
	GNode *asn = NULL;
	GcrParsed *parsed;

	asn = egg_asn1x_create_and_decode (pkix_asn1_tab, "SignedPublicKeyAndChallenge", data);
	if (!asn)
		return GCR_ERROR_UNRECOGNIZED;

	parsed = push_parsed (self, FALSE);
	parsing_block (parsed, GCR_FORMAT_DER_SPKAC, data);

	parsing_object (parsed, CKO_GCR_CERTIFICATE_REQUEST);
	parsed_ulong_attribute (parsed, CKA_GCR_CERTIFICATE_REQUEST_TYPE, CKQ_GCR_SPKAC);

	parsed_attribute_bytes (parsed, CKA_VALUE, data);
	parsed_fire (self, parsed);

	egg_asn1x_destroy (asn);

	pop_parsed (self, parsed);
	return SUCCESS;
}

static gint
parse_base64_spkac (GcrParser *self,
                    GBytes *dat)
{
	const gchar *PREFIX = "SPKAC=";
	const gsize PREFIX_LEN = 6;

	GcrParsed *parsed;
	guchar *spkac;
	gsize n_spkac;
	const guchar *data;
	GBytes *bytes;
	gsize n_data;
	gint ret;
	data = g_bytes_get_data (dat, &n_data);

	if (n_data > PREFIX_LEN && memcmp (PREFIX, data, PREFIX_LEN))
		return GCR_ERROR_UNRECOGNIZED;

	parsed = push_parsed (self, FALSE);
	parsing_block (parsed, GCR_FORMAT_DER_SPKAC, dat);

	data += PREFIX_LEN;
	n_data -= PREFIX_LEN;

	spkac = g_base64_decode ((const gchar *)data, &n_spkac);
	if (spkac != NULL) {
		bytes = g_bytes_new_take (spkac, n_spkac);
		ret = parse_der_spkac (self, bytes);
		g_bytes_unref (bytes);
	} else {
		ret = GCR_ERROR_FAILURE;
	}

	pop_parsed (self, parsed);
	return ret;
}

/* -----------------------------------------------------------------------------
 * OPENPGP
 */

static void
on_openpgp_packet (GPtrArray *records,
                   GBytes *outer,
                   gpointer user_data)
{
	GcrParser *self = GCR_PARSER (user_data);
	GcrParsed *parsed;
	gchar *string;

	/*
	 * If it's an openpgp packet that doesn't contain a key, then
	 * just ignore it here.
	 */
	if (records->len == 0)
		return;

	parsed = push_parsed (self, FALSE);

	/* All we can do is the packet bounds */
	parsing_block (parsed, GCR_FORMAT_OPENPGP_PACKET, outer);
	parsing_object (parsed, CKO_GCR_GNUPG_RECORDS);
	string = _gcr_records_format (records);
	parsed_attribute (parsed, CKA_VALUE, string, strlen (string));
	parsed_fire (self, parsed);
	pop_parsed (self, parsed);

	g_free (string);
}

static gint
parse_openpgp_packets (GcrParser *self,
                       GBytes *data)
{
	gint num_parsed;

	num_parsed = _gcr_openpgp_parse (data,
	                                 GCR_OPENPGP_PARSE_KEYS |
	                                 GCR_OPENPGP_PARSE_ATTRIBUTES |
	                                 GCR_OPENPGP_PARSE_SIGNATURES,
	                                 on_openpgp_packet, self);

	if (num_parsed == 0)
		return GCR_ERROR_UNRECOGNIZED;
	return SUCCESS;
}

/* -----------------------------------------------------------------------------
 * ARMOR PARSING
 */

static gboolean
formats_for_armor_type (GQuark armor_type,
                        gint *inner_format,
                        gint *outer_format)
{
	gint dummy;
	if (!inner_format)
		inner_format = &dummy;
	if (!outer_format)
		outer_format = &dummy;

	if (armor_type == PEM_RSA_PRIVATE_KEY) {
		*inner_format = GCR_FORMAT_DER_PRIVATE_KEY_RSA;
		*outer_format = GCR_FORMAT_PEM_PRIVATE_KEY_RSA;
	} else if (armor_type == PEM_DSA_PRIVATE_KEY) {
		*inner_format = GCR_FORMAT_DER_PRIVATE_KEY_DSA;
		*outer_format = GCR_FORMAT_PEM_PRIVATE_KEY_DSA;
	} else if (armor_type == PEM_EC_PRIVATE_KEY) {
		*inner_format = GCR_FORMAT_DER_PRIVATE_KEY_EC;
		*outer_format = GCR_FORMAT_PEM_PRIVATE_KEY_EC;
	} else if (armor_type == PEM_ANY_PRIVATE_KEY) {
		*inner_format = GCR_FORMAT_DER_PRIVATE_KEY;
		*outer_format = GCR_FORMAT_PEM_PRIVATE_KEY;
	} else if (armor_type == PEM_PRIVATE_KEY) {
		*inner_format = GCR_FORMAT_DER_PKCS8_PLAIN;
		*outer_format = GCR_FORMAT_PEM_PKCS8_PLAIN;
	} else if (armor_type == PEM_ENCRYPTED_PRIVATE_KEY) {
		*inner_format = GCR_FORMAT_DER_PKCS8_ENCRYPTED;
		*outer_format = GCR_FORMAT_PEM_PKCS8_ENCRYPTED;
	} else if (armor_type == PEM_CERTIFICATE) {
		*inner_format = GCR_FORMAT_DER_CERTIFICATE_X509;
		*outer_format = GCR_FORMAT_PEM_CERTIFICATE_X509;
	} else if (armor_type == PEM_PKCS7) {
		*inner_format = GCR_FORMAT_DER_PKCS7;
		*outer_format = GCR_FORMAT_PEM_PKCS7;
	} else if (armor_type == PEM_CERTIFICATE_REQUEST) {
		*inner_format = GCR_FORMAT_DER_PKCS10;
		*outer_format = GCR_FORMAT_PEM_PKCS10;
	} else if (armor_type == PEM_PKCS12) {
		*inner_format = GCR_FORMAT_DER_PKCS12;
		*outer_format = GCR_FORMAT_PEM_PKCS12;
	} else if (armor_type == PEM_PUBLIC_KEY) {
		*inner_format = GCR_FORMAT_DER_SUBJECT_PUBLIC_KEY;
		*outer_format = GCR_FORMAT_PEM_PUBLIC_KEY;
	} else if (armor_type == ARMOR_PGP_PRIVATE_KEY_BLOCK) {
		*inner_format = GCR_FORMAT_OPENPGP_PACKET;
		*outer_format = GCR_FORMAT_OPENPGP_ARMOR;
	} else if (armor_type == ARMOR_PGP_PUBLIC_KEY_BLOCK) {
		*inner_format = GCR_FORMAT_OPENPGP_PACKET;
		*outer_format = GCR_FORMAT_OPENPGP_ARMOR;
	} else {
		return FALSE;
	}

	return TRUE;
}

static gint
handle_plain_pem (GcrParser *self,
                  gint format_id,
                  gint want_format,
                  GBytes *data)
{
	ParserFormat *format;

	if (want_format != 0 && want_format != format_id)
		return GCR_ERROR_UNRECOGNIZED;

	format = parser_format_lookup (format_id);
	if (format == NULL)
		return GCR_ERROR_UNRECOGNIZED;

	return (format->function) (self, data);
}

static gint
handle_encrypted_pem (GcrParser *self,
                      gint format_id,
                      gint want_format,
                      GHashTable *headers,
                      GBytes *data)
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

		res = enum_next_password (self, &pstate, &password);
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
		res = handle_plain_pem (self, format_id, want_format, dbytes);
		g_bytes_unref (dbytes);

		/* Unrecognized is a bad password */
		if (res != GCR_ERROR_UNRECOGNIZED)
			break;
	}

	return res;
}

typedef struct {
	GcrParser *parser;
	gint result;
	gint want_format;
} HandlePemArgs;

static void
handle_pem_data (GQuark type,
                 GBytes *data,
                 GBytes *outer,
                 GHashTable *headers,
                 gpointer user_data)
{
	HandlePemArgs *args = (HandlePemArgs*)user_data;
	gint res = GCR_ERROR_FAILURE;
	gboolean encrypted = FALSE;
	const gchar *val;
	gint inner_format;
	gint outer_format;
	GcrParsed *parsed;

	/* Something already failed to parse */
	if (args->result == GCR_ERROR_FAILURE)
		return;

	if (!formats_for_armor_type (type, &inner_format, &outer_format))
		return;

	parsed = push_parsed (args->parser, FALSE);

	/* Fill in information necessary for prompting */
	parsing_block (parsed, outer_format, outer);

	/* See if it's encrypted PEM all openssl like*/
	if (headers) {
		val = g_hash_table_lookup (headers, "Proc-Type");
		if (val && strcmp (val, "4,ENCRYPTED") == 0)
			encrypted = TRUE;
	}

	if (encrypted)
		res = handle_encrypted_pem (args->parser, inner_format,
		                            args->want_format, headers,
		                            data);
	else
		res = handle_plain_pem (args->parser, inner_format,
		                        args->want_format, data);

	pop_parsed (args->parser, parsed);

	if (res != GCR_ERROR_UNRECOGNIZED) {
		if (args->result == GCR_ERROR_UNRECOGNIZED)
			args->result = res;
		else if (res > args->result)
			args->result = res;
	}
}

static gint
handle_pem_format (GcrParser *self,
                   gint subformat,
                   GBytes *data)
{
	HandlePemArgs ctx = { self, GCR_ERROR_UNRECOGNIZED, subformat };
	guint found;

	if (g_bytes_get_size (data) == 0)
		return GCR_ERROR_UNRECOGNIZED;

	found = egg_armor_parse (data, handle_pem_data, &ctx);

	if (found == 0)
		return GCR_ERROR_UNRECOGNIZED;

	return ctx.result;
}


static gint
parse_pem (GcrParser *self,
           GBytes *data)
{
	return handle_pem_format (self, 0, data);
}

static gint
parse_pem_private_key_rsa (GcrParser *self,
                           GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PRIVATE_KEY_RSA, data);
}

static gint
parse_pem_private_key_dsa (GcrParser *self,
                           GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PRIVATE_KEY_DSA, data);
}

static gint
parse_pem_private_key_ec (GcrParser *self,
		          GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PRIVATE_KEY_EC, data);
}

static gint
parse_pem_public_key (GcrParser *self,
                      GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_SUBJECT_PUBLIC_KEY, data);
}

static gint
parse_pem_certificate (GcrParser *self,
                       GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_CERTIFICATE_X509, data);
}

static gint
parse_pem_pkcs8_plain (GcrParser *self,
                       GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PKCS8_PLAIN, data);
}

static gint
parse_pem_pkcs8_encrypted (GcrParser *self,
                           GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PKCS8_ENCRYPTED, data);
}

static gint
parse_pem_pkcs7 (GcrParser *self,
                 GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PKCS7, data);
}

static gint
parse_pem_pkcs10 (GcrParser *self,
                  GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PKCS10, data);
}

static gint
parse_pem_pkcs12 (GcrParser *self,
                  GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_DER_PKCS12, data);
}

static gint
parse_openpgp_armor (GcrParser *self,
                     GBytes *data)
{
	return handle_pem_format (self, GCR_FORMAT_OPENPGP_PACKET, data);
}

/* -----------------------------------------------------------------------------
 * OPENSSH
 */

static void
on_openssh_public_key_parsed (GckAttributes *attrs,
                              const gchar *label,
                              const gchar *options,
                              GBytes *outer,
                              gpointer user_data)
{
	GcrParser *self = GCR_PARSER (user_data);
	GcrParsed *parsed;

	parsed = push_parsed (self, FALSE);
	parsing_block (parsed, GCR_FORMAT_OPENSSH_PUBLIC, outer);
	parsed_attributes (parsed, attrs);
	parsed_label (parsed, label);
	parsed_fire (self, parsed);
	pop_parsed (self, parsed);
}

static gint
parse_openssh_public (GcrParser *self,
                      GBytes *data)
{
	guint num_parsed;

	num_parsed = _gcr_openssh_pub_parse (data, on_openssh_public_key_parsed, self);

	if (num_parsed == 0)
		return GCR_ERROR_UNRECOGNIZED;
	return SUCCESS;
}

/* -----------------------------------------------------------------------------
 * FORMATS
 */

/**
 * GcrDataFormat:
 * @GCR_FORMAT_ALL: Represents all the formats, when enabling or disabling
 * @GCR_FORMAT_INVALID: Not a valid format
 * @GCR_FORMAT_DER_PRIVATE_KEY: DER encoded private key
 * @GCR_FORMAT_DER_PRIVATE_KEY_RSA: DER encoded RSA private key
 * @GCR_FORMAT_DER_PRIVATE_KEY_DSA: DER encoded DSA private key
 * @GCR_FORMAT_DER_PRIVATE_KEY_EC: DER encoded EC private key
 * @GCR_FORMAT_DER_SUBJECT_PUBLIC_KEY: DER encoded SubjectPublicKeyInfo
 * @GCR_FORMAT_DER_CERTIFICATE_X509: DER encoded X.509 certificate
 * @GCR_FORMAT_DER_PKCS7: DER encoded PKCS\#7 container file which can contain certificates
 * @GCR_FORMAT_DER_PKCS8: DER encoded PKCS\#8 file which can contain a key
 * @GCR_FORMAT_DER_PKCS8_PLAIN: Unencrypted DER encoded PKCS\#8 file which can contain a key
 * @GCR_FORMAT_DER_PKCS8_ENCRYPTED: Encrypted DER encoded PKCS\#8 file which can contain a key
 * @GCR_FORMAT_DER_PKCS10: DER encoded PKCS\#10 certificate request file
 * @GCR_FORMAT_DER_PKCS12: DER encoded PKCS\#12 file which can contain certificates and/or keys
 * @GCR_FORMAT_OPENSSH_PUBLIC: OpenSSH v1 or v2 public key
 * @GCR_FORMAT_OPENPGP_PACKET: OpenPGP key packet(s)
 * @GCR_FORMAT_OPENPGP_ARMOR: OpenPGP public or private key armor encoded data
 * @GCR_FORMAT_PEM: An OpenSSL style PEM file with unspecified contents
 * @GCR_FORMAT_PEM_PRIVATE_KEY: An OpenSSL style PEM file with a private key
 * @GCR_FORMAT_PEM_PRIVATE_KEY_RSA: An OpenSSL style PEM file with a private RSA key
 * @GCR_FORMAT_PEM_PRIVATE_KEY_DSA: An OpenSSL style PEM file with a private DSA key
 * @GCR_FORMAT_PEM_PRIVATE_KEY_EC: An OpenSSL style PEM file with a private EC key
 * @GCR_FORMAT_PEM_CERTIFICATE_X509: An OpenSSL style PEM file with an X.509 certificate
 * @GCR_FORMAT_PEM_PKCS7: An OpenSSL style PEM file containing PKCS\#7
 * @GCR_FORMAT_PEM_PKCS8_PLAIN: Unencrypted OpenSSL style PEM file containing PKCS\#8
 * @GCR_FORMAT_PEM_PKCS8_ENCRYPTED: Encrypted OpenSSL style PEM file containing PKCS\#8
 * @GCR_FORMAT_PEM_PKCS10: An OpenSSL style PEM file containing PKCS\#10
 * @GCR_FORMAT_PEM_PKCS12: An OpenSSL style PEM file containing PKCS\#12
 * @GCR_FORMAT_PEM_PUBLIC_KEY: An OpenSSL style PEM file containing a SubjectPublicKeyInfo
 * @GCR_FORMAT_DER_SPKAC: DER encoded SPKAC as generated by HTML5 keygen element
 * @GCR_FORMAT_BASE64_SPKAC: OpenSSL style SPKAC data
 *
 * The various format identifiers.
 */

/*
 * In order of parsing when no formats specified. We put formats earlier
 * if the parser can quickly detect whether GCR_ERROR_UNRECOGNIZED or not
 */

static const ParserFormat parser_normal[] = {
	{ GCR_FORMAT_PEM, parse_pem },
	{ GCR_FORMAT_BASE64_SPKAC, parse_base64_spkac },
	{ GCR_FORMAT_DER_PRIVATE_KEY_RSA, parse_der_private_key_rsa },
	{ GCR_FORMAT_DER_PRIVATE_KEY_DSA, parse_der_private_key_dsa },
	{ GCR_FORMAT_DER_PRIVATE_KEY_EC, parse_der_private_key_ec },
	{ GCR_FORMAT_DER_SUBJECT_PUBLIC_KEY, parse_der_subject_public_key },
	{ GCR_FORMAT_DER_CERTIFICATE_X509, parse_der_certificate },
	{ GCR_FORMAT_DER_PKCS7, parse_der_pkcs7 },
	{ GCR_FORMAT_DER_PKCS8_PLAIN, parse_der_pkcs8_plain },
	{ GCR_FORMAT_DER_PKCS8_ENCRYPTED, parse_der_pkcs8_encrypted },
	{ GCR_FORMAT_DER_PKCS12, parse_der_pkcs12 },
	{ GCR_FORMAT_OPENSSH_PUBLIC, parse_openssh_public },
	{ GCR_FORMAT_OPENPGP_PACKET, parse_openpgp_packets },
	{ GCR_FORMAT_OPENPGP_ARMOR, parse_openpgp_armor },
	{ GCR_FORMAT_DER_PKCS10, parse_der_pkcs10 },
	{ GCR_FORMAT_DER_SPKAC, parse_der_spkac },
};

/* Must be in format_id numeric order */
static const ParserFormat parser_formats[] = {
	{ GCR_FORMAT_DER_PRIVATE_KEY, parse_der_private_key },
	{ GCR_FORMAT_DER_PRIVATE_KEY_RSA, parse_der_private_key_rsa },
	{ GCR_FORMAT_DER_PRIVATE_KEY_DSA, parse_der_private_key_dsa },
	{ GCR_FORMAT_DER_PRIVATE_KEY_EC, parse_der_private_key_ec },
	{ GCR_FORMAT_DER_SUBJECT_PUBLIC_KEY, parse_der_subject_public_key },
	{ GCR_FORMAT_DER_CERTIFICATE_X509, parse_der_certificate },
	{ GCR_FORMAT_DER_PKCS7, parse_der_pkcs7 },
	{ GCR_FORMAT_DER_PKCS8, parse_der_pkcs8 },
	{ GCR_FORMAT_DER_PKCS8_PLAIN, parse_der_pkcs8_plain },
	{ GCR_FORMAT_DER_PKCS8_ENCRYPTED, parse_der_pkcs8_encrypted },
	{ GCR_FORMAT_DER_PKCS10, parse_der_pkcs10 },
	{ GCR_FORMAT_DER_SPKAC, parse_der_spkac },
	{ GCR_FORMAT_BASE64_SPKAC, parse_base64_spkac },
	{ GCR_FORMAT_DER_PKCS12, parse_der_pkcs12 },
	{ GCR_FORMAT_OPENSSH_PUBLIC, parse_openssh_public },
	{ GCR_FORMAT_OPENPGP_PACKET, parse_openpgp_packets },
	{ GCR_FORMAT_OPENPGP_ARMOR, parse_openpgp_armor },
	{ GCR_FORMAT_PEM, parse_pem },
	{ GCR_FORMAT_PEM_PRIVATE_KEY_RSA, parse_pem_private_key_rsa },
	{ GCR_FORMAT_PEM_PRIVATE_KEY_DSA, parse_pem_private_key_dsa },
	{ GCR_FORMAT_PEM_CERTIFICATE_X509, parse_pem_certificate },
	{ GCR_FORMAT_PEM_PKCS7, parse_pem_pkcs7 },
	{ GCR_FORMAT_PEM_PKCS8_PLAIN, parse_pem_pkcs8_plain },
	{ GCR_FORMAT_PEM_PKCS8_ENCRYPTED, parse_pem_pkcs8_encrypted },
	{ GCR_FORMAT_PEM_PKCS12, parse_pem_pkcs12 },
	{ GCR_FORMAT_PEM_PKCS10, parse_pem_pkcs10 },
	{ GCR_FORMAT_PEM_PRIVATE_KEY_EC, parse_pem_private_key_ec },
	{ GCR_FORMAT_PEM_PUBLIC_KEY, parse_pem_public_key },
};

static int
compar_id_to_parser_format (const void *a, const void *b)
{
	const gint *format_id = a;
	const ParserFormat *format = b;

	g_assert (format_id);
	g_assert (format);

	if (format->format_id == *format_id)
		return 0;
	return (*format_id < format->format_id) ? -1 : 1;
}

static ParserFormat*
parser_format_lookup (gint format_id)
{
	return bsearch (&format_id, parser_formats, G_N_ELEMENTS (parser_formats),
	                sizeof (parser_formats[0]), compar_id_to_parser_format);
}

static gint
compare_pointers (gconstpointer a, gconstpointer b)
{
	if (a == b)
		return 0;
	return a < b ? -1 : 1;
}

typedef struct _ForeachArgs {
	GcrParser *parser;
	GBytes *data;
	gint result;
} ForeachArgs;

static gboolean
parser_format_foreach (gpointer key, gpointer value, gpointer data)
{
	ForeachArgs *args = data;
	ParserFormat *format = key;
	gint result;

	g_assert (format);
	g_assert (format->function);
	g_assert (GCR_IS_PARSER (args->parser));

	result = (format->function) (args->parser, args->data);
	if (result != GCR_ERROR_UNRECOGNIZED) {
		args->result = result;
		return TRUE;
	}

	/* Keep going */
	return FALSE;
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */


static GObject*
gcr_parser_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	GcrParser *self = GCR_PARSER (G_OBJECT_CLASS (gcr_parser_parent_class)->constructor(type, n_props, props));
	g_return_val_if_fail (self, NULL);

	/* Always try to parse with NULL and empty passwords first */
	gcr_parser_add_password (self, NULL);
	gcr_parser_add_password (self, "");

	return G_OBJECT (self);
}

static void
gcr_parser_init (GcrParser *self)
{
	self->pv = gcr_parser_get_instance_private (self);
	self->pv->passwords = g_ptr_array_new ();
	self->pv->normal_formats = TRUE;
}

static void
gcr_parser_dispose (GObject *obj)
{
	GcrParser *self = GCR_PARSER (obj);
	gsize i;

	g_assert (!self->pv->parsed);

	if (self->pv->specific_formats)
		g_tree_destroy (self->pv->specific_formats);
	self->pv->specific_formats = NULL;

	for (i = 0; i < self->pv->passwords->len; ++i)
		egg_secure_strfree (g_ptr_array_index (self->pv->passwords, i));
	g_ptr_array_set_size (self->pv->passwords, 0);

	G_OBJECT_CLASS (gcr_parser_parent_class)->dispose (obj);
}

static void
gcr_parser_finalize (GObject *obj)
{
	GcrParser *self = GCR_PARSER (obj);

	g_assert (!self->pv->parsed);

	g_ptr_array_free (self->pv->passwords, TRUE);
	self->pv->passwords = NULL;

	g_free (self->pv->filename);
	self->pv->filename = NULL;

	G_OBJECT_CLASS (gcr_parser_parent_class)->finalize (obj);
}

static void
gcr_parser_set_property (GObject *obj, guint prop_id, const GValue *value,
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_parser_get_property (GObject *obj, guint prop_id, GValue *value,
                         GParamSpec *pspec)
{
	GcrParser *self = GCR_PARSER (obj);

	switch (prop_id) {
	case PROP_PARSED_ATTRIBUTES:
		g_value_set_boxed (value, gcr_parser_get_parsed_attributes (self));
		break;
	case PROP_PARSED_LABEL:
		g_value_set_string (value, gcr_parser_get_parsed_label (self));
		break;
	case PROP_PARSED_DESCRIPTION:
		g_value_set_string (value, gcr_parser_get_parsed_description (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_parser_class_init (GcrParserClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gint i;

	gobject_class->constructor = gcr_parser_constructor;
	gobject_class->dispose = gcr_parser_dispose;
	gobject_class->finalize = gcr_parser_finalize;
	gobject_class->set_property = gcr_parser_set_property;
	gobject_class->get_property = gcr_parser_get_property;

	/**
	 * GcrParser:parsed-attributes:
	 *
	 * Get the attributes that make up the currently parsed item. This is
	 * generally only valid during a #GcrParser::parsed signal.
	 */
	g_object_class_install_property (gobject_class, PROP_PARSED_ATTRIBUTES,
	           g_param_spec_boxed ("parsed-attributes", "Parsed Attributes", "Parsed PKCS#11 attributes",
	                               GCK_TYPE_ATTRIBUTES, G_PARAM_READABLE));

	/**
	 * GcrParser:parsed-label:
	 *
	 * The label of the currently parsed item. This is generally
	 * only valid during a #GcrParser::parsed signal.
	 */
	g_object_class_install_property (gobject_class, PROP_PARSED_LABEL,
	           g_param_spec_string ("parsed-label", "Parsed Label", "Parsed item label",
	                                "", G_PARAM_READABLE));

	/**
	 * GcrParser:parsed-description:
	 *
	 * The description of the type of the currently parsed item. This is generally
	 * only valid during a #GcrParser::parsed signal.
	 */
	g_object_class_install_property (gobject_class, PROP_PARSED_DESCRIPTION,
	           g_param_spec_string ("parsed-description", "Parsed Description", "Parsed item description",
	                                "", G_PARAM_READABLE));

	/**
	 * GcrParser::authenticate:
	 * @self: the parser
	 * @count: the number of times this item has been authenticated
	 *
	 * This signal is emitted when an item needs to be unlocked or decrypted before
	 * it can be parsed. The @count argument specifies the number of times
	 * the signal has been emitted for a given item. This can be used to
	 * display a message saying the previous password was incorrect.
	 *
	 * Typically the gcr_parser_add_password() function is called in
	 * response to this signal.
	 *
	 * If %FALSE is returned, then the authentication was not handled. If
	 * no handlers return %TRUE then the item is not parsed and an error
	 * with the code %GCR_ERROR_CANCELLED will be raised.
	 *
	 * Returns: Whether the authentication was handled.
	 */
	signals[AUTHENTICATE] = g_signal_new ("authenticate", GCR_TYPE_PARSER,
	                                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GcrParserClass, authenticate),
	                                g_signal_accumulator_true_handled, NULL, _gcr_marshal_BOOLEAN__INT,
	                                G_TYPE_BOOLEAN, 1, G_TYPE_INT);

	/**
	 * GcrParser::parsed:
	 * @self: the parser
	 *
	 * This signal is emitted when an item is sucessfully parsed. To access
	 * the information about the item use the gcr_parser_get_parsed_label(),
	 * gcr_parser_get_parsed_attributes() and gcr_parser_get_parsed_description()
	 * functions.
	 */
	signals[PARSED] = g_signal_new ("parsed", GCR_TYPE_PARSER,
	                                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (GcrParserClass, parsed),
	                                NULL, NULL, g_cclosure_marshal_VOID__VOID,
	                                G_TYPE_NONE, 0);

	init_quarks ();
	_gcr_initialize_library ();

	/* Check that the format tables are in order */
	for (i = 1; i < G_N_ELEMENTS (parser_formats); ++i)
		g_assert (parser_formats[i].format_id >= parser_formats[i - 1].format_id);
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * gcr_parser_new:
 *
 * Create a new #GcrParser
 *
 * Returns: (transfer full): a newly allocated #GcrParser
 */
GcrParser *
gcr_parser_new (void)
{
	return g_object_new (GCR_TYPE_PARSER, NULL);
}

/**
 * gcr_parser_add_password:
 * @self: The parser
 * @password: (allow-none): a password to try
 *
 * Add a password to the set of passwords to try when parsing locked or encrypted
 * items. This is usually called from the #GcrParser::authenticate signal.
 */
void
gcr_parser_add_password (GcrParser *self, const gchar *password)
{
	g_return_if_fail (GCR_IS_PARSER (self));
	g_ptr_array_add (self->pv->passwords, egg_secure_strdup (password));
}

/**
 * gcr_parser_parse_bytes:
 * @self: The parser
 * @data: the data to parse
 * @error: A location to raise an error on failure.
 *
 * Parse the data. The #GcrParser::parsed and #GcrParser::authenticate signals
 * may fire during the parsing.
 *
 * Returns: Whether the data was parsed successfully or not.
 */
gboolean
gcr_parser_parse_bytes (GcrParser *self,
                        GBytes *data,
                        GError **error)
{
	ForeachArgs args = { self, NULL, GCR_ERROR_UNRECOGNIZED };
	const gchar *message = NULL;
	gint i;

	g_return_val_if_fail (GCR_IS_PARSER (self), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	if (g_bytes_get_size (data) > 0) {
		args.data = g_bytes_ref (data);

		/* Just the specific formats requested */
		if (self->pv->specific_formats) {
			g_tree_foreach (self->pv->specific_formats, parser_format_foreach, &args);

		/* All the 'normal' formats */
		} else if (self->pv->normal_formats) {
			for (i = 0; i < G_N_ELEMENTS (parser_normal); ++i) {
				if (parser_format_foreach ((gpointer)(parser_normal + i),
							   (gpointer)(parser_normal + i), &args))
					break;
			}
		}

		g_bytes_unref (args.data);
	}

	switch (args.result) {
	case SUCCESS:
		return TRUE;
	case GCR_ERROR_CANCELLED:
		message = _("The operation was cancelled");
		break;
	case GCR_ERROR_UNRECOGNIZED:
		message = _("Unrecognized or unsupported data.");
		break;
	case GCR_ERROR_FAILURE:
		message = _("Could not parse invalid or corrupted data.");
		break;
	case GCR_ERROR_LOCKED:
		message = _("The data is locked");
		break;
	default:
		g_assert_not_reached ();
		break;
	};

	g_set_error_literal (error, GCR_DATA_ERROR, args.result, message);
	return FALSE;
}

/**
 * gcr_parser_parse_data:
 * @self: The parser
 * @data: (array length=n_data): the data to parse
 * @n_data: The length of the data
 * @error: A location to raise an error on failure.
 *
 * Parse the data. The #GcrParser::parsed and #GcrParser::authenticate signals
 * may fire during the parsing.
 *
 * A copy of the data will be made. Use gcr_parser_parse_bytes() to avoid this.
 *
 * Returns: Whether the data was parsed successfully or not.
 */
gboolean
gcr_parser_parse_data (GcrParser *self,
                       const guchar *data,
                       gsize n_data,
                       GError **error)
{
	GBytes *bytes;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_PARSER (self), FALSE);
	g_return_val_if_fail (data || !n_data, FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	bytes = g_bytes_new (data, n_data);
	ret = gcr_parser_parse_bytes (self, bytes, error);
	g_bytes_unref (bytes);

	return ret;
}

/**
 * gcr_parser_format_enable:
 * @self: The parser
 * @format: The format identifier
 *
 * Enable parsing of the given format. Use %GCR_FORMAT_ALL to enable all the formats.
 */
void
gcr_parser_format_enable (GcrParser *self,
                          GcrDataFormat format)
{
	const ParserFormat *form;
	guint i;

	g_return_if_fail (GCR_IS_PARSER (self));

	if (!self->pv->specific_formats)
		self->pv->specific_formats = g_tree_new (compare_pointers);

	if (format != -1) {
		form = parser_format_lookup (format);
		g_return_if_fail (form);
		g_tree_insert (self->pv->specific_formats,
		               (gpointer)form, (gpointer)form);
	} else {
		for (i = 0; i < G_N_ELEMENTS (parser_formats); i++) {
			form = &parser_formats[i];
			g_tree_insert (self->pv->specific_formats, (gpointer)form,
			               (gpointer)form);
		}
	}
}

/**
 * gcr_parser_format_disable:
 * @self: The parser
 * @format: The format identifier
 *
 * Disable parsing of the given format. Use %GCR_FORMAT_ALL to disable all the formats.
 */
void
gcr_parser_format_disable (GcrParser *self,
                           GcrDataFormat format)
{
	ParserFormat *form;

	g_return_if_fail (GCR_IS_PARSER (self));

	if (format == -1) {
		if (self->pv->specific_formats)
			g_tree_destroy (self->pv->specific_formats);
		self->pv->specific_formats = NULL;
		self->pv->normal_formats = FALSE;
	}

	if (!self->pv->specific_formats)
		return;

	form = parser_format_lookup (format);
	g_return_if_fail (form);

	g_tree_remove (self->pv->specific_formats, form);
}

/**
 * gcr_parser_format_supported:
 * @self: The parser
 * @format: The format identifier
 *
 * Check whether the given format is supported by the parser.
 *
 * Returns: Whether the format is supported.
 */
gboolean
gcr_parser_format_supported (GcrParser *self,
                             GcrDataFormat format)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), FALSE);
	g_return_val_if_fail (format != GCR_FORMAT_ALL, FALSE);
	g_return_val_if_fail (format != GCR_FORMAT_INVALID, FALSE);
	return parser_format_lookup (format) ? TRUE : FALSE;
}

/**
 * gcr_parser_get_parsed:
 * @self: a parser
 *
 * Get the currently parsed item
 *
 * Returns: (transfer none): the currently parsed item
 */
GcrParsed *
gcr_parser_get_parsed (GcrParser *self)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), NULL);
	return self->pv->parsed;
}

GType
gcr_parsed_get_type (void)
{
	static volatile gsize initialized = 0;
	static GType type = 0;
	if (g_once_init_enter (&initialized)) {
		type = g_boxed_type_register_static ("GcrParsed",
		                                     (GBoxedCopyFunc)gcr_parsed_ref,
		                                     (GBoxedFreeFunc)gcr_parsed_unref);
		g_once_init_leave (&initialized, 1);
	}
	return type;
}

/**
 * gcr_parser_get_filename:
 * @self: a parser item
 *
 * Get the filename of the parser item.
 *
 * Returns: the filename set on the parser, or %NULL
 */
const gchar *
gcr_parser_get_filename (GcrParser *self)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), NULL);
	return self->pv->filename;
}

/**
 * gcr_parser_set_filename:
 * @self: a parser item
 * @filename: (allow-none): a string of the filename of the parser item
 *
 * Sets the filename of the parser item.
 */
void
gcr_parser_set_filename (GcrParser *self,
                         const gchar *filename)
{
	g_return_if_fail (GCR_IS_PARSER (self));
	g_free (self->pv->filename);
	self->pv->filename = g_strdup (filename);
}

/**
 * gcr_parsed_ref:
 * @parsed: a parsed item
 *
 * Add a reference to a parsed item. An item may not be shared across threads
 * until it has been referenced at least once.
 *
 * Returns: (transfer full): the parsed item
 */
GcrParsed *
gcr_parsed_ref (GcrParsed *parsed)
{
	GcrParsed *copy;

	g_return_val_if_fail (parsed != NULL, NULL);

	/* Already had a reference */
	if (g_atomic_int_add (&parsed->refs, 1) >= 1)
		return parsed;

	/* If this is the first reference, flatten the stack of parsed */
	copy = g_new0 (GcrParsed, 1);
	copy->refs = 1;
	copy->label = g_strdup (gcr_parsed_get_label (parsed));
	copy->filename = g_strdup (gcr_parsed_get_filename (parsed));
	copy->attrs = gcr_parsed_get_attributes (parsed);
	copy->format = gcr_parsed_get_format (parsed);
	if (copy->attrs)
		gck_attributes_ref (copy->attrs);
	copy->description = gcr_parsed_get_description (parsed);
	copy->next = NULL;

	/* Find the block of data to copy */
	while (parsed != NULL) {
		if (parsed->data != NULL) {
			copy->data = g_bytes_ref (parsed->data);
			copy->sensitive = parsed->sensitive;
			break;
		}
		parsed = parsed->next;
	}

	return copy;
}

/**
 * gcr_parsed_unref:
 * @parsed: a parsed item
 *
 * Unreferences a parsed item which was referenced with gcr_parsed_ref()
 */
void
gcr_parsed_unref (gpointer parsed)
{
	GcrParsed *par = parsed;

	g_return_if_fail (parsed != NULL);

	if (g_atomic_int_dec_and_test (&par->refs)) {
		_gcr_parsed_free (parsed);
	}
}

/**
 * gcr_parser_get_parsed_description:
 * @self: The parser
 *
 * Get a description for the type of the currently parsed item. This is generally
 * only valid during the #GcrParser::parsed signal.
 *
 * Returns: (allow-none): the description for the current item; this is owned by
 *          the parser and should not be freed
 */
const gchar*
gcr_parser_get_parsed_description (GcrParser *self)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), NULL);
	g_return_val_if_fail (self->pv->parsed != NULL, NULL);

	return gcr_parsed_get_description (self->pv->parsed);
}

/**
 * gcr_parsed_get_description:
 * @parsed: a parsed item
 *
 * Get the descirption for a parsed item.
 *
 * Returns: (allow-none): the description
 */
const gchar*
gcr_parsed_get_description (GcrParsed *parsed)
{
	while (parsed != NULL) {
		if (parsed->description != NULL)
			return parsed->description;
		parsed = parsed->next;
	}

	return NULL;
}

/**
 * gcr_parser_get_parsed_attributes:
 * @self: The parser
 *
 * Get the attributes which make up the currently parsed item. This is generally
 * only valid during the #GcrParser::parsed signal.
 *
 * Returns: (transfer none) (allow-none): the attributes for the current item,
 *          which are owned by the parser and should not be freed
 */
GckAttributes *
gcr_parser_get_parsed_attributes (GcrParser *self)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), NULL);
	g_return_val_if_fail (self->pv->parsed != NULL, NULL);

	return gcr_parsed_get_attributes (self->pv->parsed);
}

/**
 * gcr_parsed_get_attributes:
 * @parsed: a parsed item
 *
 * Get the attributes which make up the parsed item.
 *
 * Returns: (transfer none) (allow-none): the attributes for the item; these
 *          are owned by the parsed item and should not be freed
 */
GckAttributes *
gcr_parsed_get_attributes (GcrParsed *parsed)
{
	while (parsed != NULL) {
		if (parsed->attrs != NULL)
			return parsed->attrs;
		parsed = parsed->next;
	}

	return NULL;
}

/**
 * gcr_parser_get_parsed_label:
 * @self: The parser
 *
 * Get the label of the currently parsed item. This is generally only valid
 * during the #GcrParser::parsed signal.
 *
 * Returns: (allow-none): the label of the currently parsed item. The value is
 *          owned by the parser and should not be freed.
 */
const gchar*
gcr_parser_get_parsed_label (GcrParser *self)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), NULL);
	g_return_val_if_fail (self->pv->parsed != NULL, NULL);

	return gcr_parsed_get_label (self->pv->parsed);
}

/**
 * gcr_parsed_get_label:
 * @parsed: a parsed item
 *
 * Get the label for the parsed item.
 *
 * Returns: (allow-none): the label for the item
 */
const gchar*
gcr_parsed_get_label (GcrParsed *parsed)
{
	while (parsed != NULL) {
		if (parsed->label != NULL)
			return parsed->label;
		parsed = parsed->next;
	}

	return NULL;
}

/**
 * gcr_parser_get_parsed_block:
 * @self: a parser
 * @n_block: a location to place the size of the block
 *
 * Get the raw data block that represents this parsed object. This is only
 * valid during the #GcrParser::parsed signal.
 *
 * Returns: (transfer none) (array length=n_block) (allow-none): the raw data
 *          block of the currently parsed item; the value is owned by the parser
 *          and should not be freed
 */
const guchar *
gcr_parser_get_parsed_block (GcrParser *self,
                             gsize *n_block)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), NULL);
	g_return_val_if_fail (n_block != NULL, NULL);
	g_return_val_if_fail (self->pv->parsed != NULL, NULL);

	return gcr_parsed_get_data (self->pv->parsed, n_block);
}


/**
 * gcr_parser_get_parsed_bytes:
 * @self: a parser
 *
 * Get the raw data block that represents this parsed object. This is only
 * valid during the #GcrParser::parsed signal.
 *
 * Returns: (transfer none): the raw data block of the currently parsed item
 */
GBytes *
gcr_parser_get_parsed_bytes (GcrParser *self)
{
	return gcr_parsed_get_bytes (self->pv->parsed);
}

/**
 * gcr_parsed_get_data:
 * @parsed: a parsed item
 * @n_data: location to store size of returned data
 *
 * Get the raw data block for the parsed item.
 *
 * Returns: (transfer none) (array length=n_data) (allow-none): the raw data of
 *          the parsed item, or %NULL
 */
const guchar *
gcr_parsed_get_data (GcrParsed *parsed,
                     gsize *n_data)
{
	GBytes *bytes;

	g_return_val_if_fail (n_data != NULL, NULL);

	bytes = gcr_parsed_get_bytes (parsed);
	if (bytes == NULL) {
		*n_data = 0;
		return NULL;
	}

	return g_bytes_get_data (bytes, n_data);
}

/**
 * gcr_parsed_get_bytes:
 * @parsed: a parsed item
 *
 * Get the raw data block for the parsed item.
 *
 * Returns: (transfer none): the raw data of the parsed item, or %NULL
 */
GBytes *
gcr_parsed_get_bytes (GcrParsed *parsed)
{
	while (parsed != NULL) {
		if (parsed->data != NULL)
			return parsed->data;
		parsed = parsed->next;
	}

	return NULL;
}

/**
 * gcr_parser_get_parsed_format:
 * @self: a parser
 *
 * Get the format of the raw data block that represents this parsed object.
 * This corresponds with the data returned from gcr_parser_get_parsed_block().
 *
 * This is only valid during the #GcrParser::parsed signal.
 *
 * Returns: the data format of the currently parsed item
 */
GcrDataFormat
gcr_parser_get_parsed_format (GcrParser *self)
{
	g_return_val_if_fail (GCR_IS_PARSER (self), 0);
	g_return_val_if_fail (self->pv->parsed != NULL, 0);

	return gcr_parsed_get_format (self->pv->parsed);
}

/**
 * gcr_parsed_get_format:
 * @parsed: a parsed item
 *
 * Get the format of the parsed item.
 *
 * Returns: the data format of the item
 */
GcrDataFormat
gcr_parsed_get_format (GcrParsed *parsed)
{
	while (parsed != NULL) {
		if (parsed->data != NULL)
			return parsed->format;
		parsed = parsed->next;
	}

	return 0;
}

/**
 * gcr_parsed_get_filename:
 * @parsed: a parsed item
 *
 * Get the filename of the parsed item.
 *
 * Returns: (transfer none): the filename of
 *          the parsed item, or %NULL
 */
const gchar *
gcr_parsed_get_filename (GcrParsed *parsed)
{
	g_return_val_if_fail (parsed != NULL, NULL);
	return parsed->filename;
}
/* ---------------------------------------------------------------------------------
 * STREAM PARSING
 */

#define GCR_TYPE_PARSING        (gcr_parsing_get_type ())
#define GCR_PARSING(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_PARSING, GcrParsing))
#define GCR_IS_PARSING(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_PARSING))

typedef struct _GcrParsing {
	GObjectClass parent;

	GcrParser *parser;
	gboolean async;
	GCancellable *cancel;

	/* Failure information */
	GError *error;
	gboolean complete;

	/* Operation state */
	GInputStream *input;
	GByteArray *buffer;

	/* Async callback stuff */
	GAsyncReadyCallback callback;
	gpointer user_data;

} GcrParsing;

typedef struct _GcrParsingClass {
	GObjectClass parent_class;
} GcrParsingClass;

/* State forward declarations */
static void state_cancelled (GcrParsing *self, gboolean async);
static void state_failure (GcrParsing *self, gboolean async);
static void state_complete (GcrParsing *self, gboolean async);
static void state_parse_buffer (GcrParsing *self, gboolean async);
static void state_read_buffer (GcrParsing *self, gboolean async);

/* Other forward declarations */
static GType gcr_parsing_get_type (void) G_GNUC_CONST;
static void gcr_parsing_async_result_init (GAsyncResultIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrParsing, gcr_parsing, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, gcr_parsing_async_result_init));

#define BLOCK 4096

static void
next_state (GcrParsing *self, void (*state) (GcrParsing*, gboolean))
{
	g_assert (GCR_IS_PARSING (self));
	g_assert (state);

	if (self->cancel && g_cancellable_is_cancelled (self->cancel))
		state = state_cancelled;

	(state) (self, self->async);
}

static void
state_complete (GcrParsing *self, gboolean async)
{
	g_assert (GCR_IS_PARSING (self));
	g_assert (!self->complete);
	self->complete = TRUE;
	if (async && self->callback != NULL)
		(self->callback) (G_OBJECT (self->parser), G_ASYNC_RESULT (self), self->user_data);
}

static void
state_failure (GcrParsing *self, gboolean async)
{
	g_assert (GCR_IS_PARSING (self));
	g_assert (self->error);
	next_state (self, state_complete);
}

static void
state_cancelled (GcrParsing *self, gboolean async)
{
	g_assert (GCR_IS_PARSING (self));
	if (self->cancel && g_cancellable_is_cancelled (self->cancel))
		g_cancellable_cancel (self->cancel);
	if (self->error)
		g_error_free (self->error);
	self->error = g_error_new_literal (GCR_DATA_ERROR, GCR_ERROR_CANCELLED, _("The operation was cancelled"));
	next_state (self, state_failure);
}

static void
state_parse_buffer (GcrParsing *self, gboolean async)
{
	GError *error = NULL;
	GBytes *bytes;
	gboolean ret;

	g_assert (GCR_IS_PARSING (self));
	g_assert (self->buffer);

	bytes = g_byte_array_free_to_bytes (self->buffer);
	self->buffer = NULL;
	ret = gcr_parser_parse_bytes (self->parser, bytes, &error);
	g_bytes_unref (bytes);

	if (ret == TRUE) {
		next_state (self, state_complete);
	} else {
		g_propagate_error (&self->error, error);
		next_state (self, state_failure);
	}
}

static void
complete_read_buffer (GcrParsing *self, gssize count, GError *error)
{
	g_assert (GCR_IS_PARSING (self));
	g_assert (self->buffer);

	/* A failure */
	if (count == -1) {
		g_propagate_error (&self->error, error);
		next_state (self, state_failure);
	} else {

		g_return_if_fail (count >= 0 && count <= BLOCK);
		g_byte_array_set_size (self->buffer, self->buffer->len - (BLOCK - count));

		/* Finished reading */
		if (count == 0)
			next_state (self, state_parse_buffer);

		/* Read the next block */
		else
			next_state (self, state_read_buffer);
	}

}

static void
on_read_buffer (GObject *obj, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	gssize count;

	count = g_input_stream_read_finish (G_INPUT_STREAM (obj), res, &error);
	complete_read_buffer (user_data, count, error);
}

static void
state_read_buffer (GcrParsing *self, gboolean async)
{
	GError *error = NULL;
	gssize count;
	gsize at;

	g_assert (GCR_IS_PARSING (self));
	g_assert (G_IS_INPUT_STREAM (self->input));

	if (!self->buffer)
		self->buffer = g_byte_array_sized_new (BLOCK);

	at = self->buffer->len;
	g_byte_array_set_size (self->buffer, at + BLOCK);

	if (async) {
		g_input_stream_read_async (self->input, self->buffer->data + at,
		                           BLOCK, G_PRIORITY_DEFAULT, self->cancel,
		                           on_read_buffer, self);
	} else {
		count = g_input_stream_read (self->input, self->buffer->data + at,
		                             BLOCK, self->cancel, &error);
		complete_read_buffer (self, count, error);
	}
}

static void
gcr_parsing_init (GcrParsing *self)
{

}

static void
gcr_parsing_finalize (GObject *obj)
{
	GcrParsing *self = GCR_PARSING (obj);

	g_object_unref (self->parser);
	self->parser = NULL;

	g_object_unref (self->input);
	self->input = NULL;

	if (self->cancel)
		g_object_unref (self->cancel);
	self->cancel = NULL;

	g_clear_error (&self->error);

	if (self->buffer)
		g_byte_array_free (self->buffer, TRUE);
	self->buffer = NULL;

	G_OBJECT_CLASS (gcr_parsing_parent_class)->finalize (obj);
}

static void
gcr_parsing_class_init (GcrParsingClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gcr_parsing_finalize;
}

static gpointer
gcr_parsing_real_get_user_data (GAsyncResult *base)
{
	g_return_val_if_fail (GCR_IS_PARSING (base), NULL);
	return GCR_PARSING (base)->user_data;
}

static GObject*
gcr_parsing_real_get_source_object (GAsyncResult *base)
{
	g_return_val_if_fail (GCR_IS_PARSING (base), NULL);
	return G_OBJECT (GCR_PARSING (base)->parser);
}

static void
gcr_parsing_async_result_init (GAsyncResultIface *iface)
{
	iface->get_source_object = gcr_parsing_real_get_source_object;
	iface->get_user_data = gcr_parsing_real_get_user_data;
}

static GcrParsing*
gcr_parsing_new (GcrParser *parser, GInputStream *input, GCancellable *cancel)
{
	GcrParsing *self;

	g_assert (GCR_IS_PARSER (parser));
	g_assert (G_IS_INPUT_STREAM (input));

	self = g_object_new (GCR_TYPE_PARSING, NULL);
	self->parser = g_object_ref (parser);
	self->input = g_object_ref (input);
	if (cancel)
		self->cancel = g_object_ref (cancel);

	return self;
}

/**
 * gcr_parser_parse_stream:
 * @self: The parser
 * @input: The input stream
 * @cancellable: An optional cancellation object
 * @error: A location to raise an error on failure
 *
 * Parse items from the data in a #GInputStream. This function may block while
 * reading from the input stream. Use gcr_parser_parse_stream_async() for
 * a non-blocking variant.
 *
 * The #GcrParser::parsed and #GcrParser::authenticate signals
 * may fire during the parsing.
 *
 * Returns: Whether the parsing completed successfully or not.
 */
gboolean
gcr_parser_parse_stream (GcrParser *self, GInputStream *input, GCancellable *cancellable,
                         GError **error)
{
	GcrParsing *parsing;
	gboolean result;

	g_return_val_if_fail (GCR_IS_PARSER (self), FALSE);
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	parsing = gcr_parsing_new (self, input, cancellable);
	parsing->async = FALSE;

	next_state (parsing, state_read_buffer);
	g_assert (parsing->complete);

	result = gcr_parser_parse_stream_finish (self, G_ASYNC_RESULT (parsing), error);
	g_object_unref (parsing);

	return result;
}

/**
 * gcr_parser_parse_stream_async:
 * @self: The parser
 * @input: The input stream
 * @cancellable: An optional cancellation object
 * @callback: Called when the operation result is ready.
 * @user_data: Data to pass to callback
 *
 * Parse items from the data in a #GInputStream. This function completes
 * asyncronously and doesn't block.
 *
 * The #GcrParser::parsed and #GcrParser::authenticate signals
 * may fire during the parsing.
 */
void
gcr_parser_parse_stream_async (GcrParser *self, GInputStream *input, GCancellable *cancellable,
                               GAsyncReadyCallback callback, gpointer user_data)
{
	GcrParsing *parsing;

	g_return_if_fail (GCR_IS_PARSER (self));
	g_return_if_fail (G_IS_INPUT_STREAM (input));

	parsing = gcr_parsing_new (self, input, cancellable);
	parsing->async = TRUE;
	parsing->callback = callback;
	parsing->user_data = user_data;

	next_state (parsing, state_read_buffer);
}

/**
 * gcr_parser_parse_stream_finish:
 * @self: The parser
 * @result:The operation result
 * @error: A location to raise an error on failure
 *
 * Complete an operation to parse a stream.
 *
 * Returns: Whether the parsing completed successfully or not.
 */
gboolean
gcr_parser_parse_stream_finish (GcrParser *self, GAsyncResult *result, GError **error)
{
	GcrParsing *parsing;

	g_return_val_if_fail (GCR_IS_PARSING (result), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	parsing = GCR_PARSING (result);
	g_return_val_if_fail (parsing->complete, FALSE);

	if (parsing->error) {
		g_propagate_error (error, parsing->error);
		return FALSE;
	}

	return TRUE;
}
