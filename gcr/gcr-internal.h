/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Collabora Ltd.
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

#ifndef GCR_INTERNAL_H_
#define GCR_INTERNAL_H_

#include <glib.h>
#include <gio/gio.h>

#include "gcr-parser.h"

/* Should only be used internally */
#define GCR_SUCCESS 0

void              _gcr_initialize_library          (void);

void              _gcr_uninitialize_library        (void);

void              _gcr_set_pkcs11_config_dir       (const gchar *dir);

/* Functions defined in gcr-parser-libgcrypt.c or gcr-parser-gnutls.c */

gint              _gcr_parser_parse_der_private_key_rsa
                                                   (GcrParser *self,
                                                    GBytes *data);

gint              _gcr_parser_parse_der_private_key_dsa
                                                   (GcrParser *self,
                                                    GBytes *data);

gint              _gcr_parser_parse_der_private_key_dsa_parts
                                                   (GcrParser *self,
                                                    GBytes *keydata,
                                                    GNode *params);

gint              _gcr_parser_parse_der_private_key_ec
                                                   (GcrParser *self,
                                                    GBytes *data);

gint
                  _gcr_parser_parse_der_pkcs8_plain
                                                   (GcrParser *self,
                                                    GBytes *data);

gint              _gcr_parser_parse_der_pkcs8_encrypted
                                                   (GcrParser *self,
                                                    GBytes *data);

gint              _gcr_parser_parse_der_pkcs12     (GcrParser *self,
                                                    GBytes *data);

gint              _gcr_parser_handle_plain_pem     (GcrParser *self,
                                                    gint format_id,
                                                    gint want_format,
                                                    GBytes *data);

gint              _gcr_parser_handle_encrypted_pem (GcrParser *self,
                                                    gint format_id,
                                                    gint want_format,
                                                    GHashTable *headers,
                                                    GBytes *data,
                                                    GBytes *outer);

gint              _gcr_parser_handle_pkcs12_cert_bag
                                                   (GcrParser *self,
                                                    GBytes *data);

void              _gcr_parsed_set_attribute        (GcrParsed *parsed,
                                                    CK_ATTRIBUTE_TYPE type,
                                                    gconstpointer data,
                                                    gsize n_data);

void              _gcr_parsed_set_attribute_bytes  (GcrParsed *parsed,
                                                    CK_ATTRIBUTE_TYPE type,
                                                    GBytes *data);

gboolean          _gcr_parsed_set_asn1_number      (GcrParsed *parsed,
                                                    GNode *asn,
                                                    const gchar *part,
                                                    CK_ATTRIBUTE_TYPE type);

gboolean          _gcr_parsed_set_asn1_element     (GcrParsed *parsed,
                                                    GNode *asn,
                                                    const gchar *part,
                                                    CK_ATTRIBUTE_TYPE type);

gboolean          _gcr_parsed_set_asn1_structure   (GcrParsed *parsed,
                                                    GNode *asn,
                                                    CK_ATTRIBUTE_TYPE type);

void              _gcr_parsed_set_ulong_attribute  (GcrParsed *parsed,
                                                    CK_ATTRIBUTE_TYPE type,
                                                    gulong value);

void              _gcr_parsed_set_boolean_attribute
                                                   (GcrParsed *parsed,
                                                    CK_ATTRIBUTE_TYPE type,
                                                    gboolean value);

void              _gcr_parsed_set_description      (GcrParsed *parsed,
                                                    CK_OBJECT_CLASS klass);

void              _gcr_parsed_set_attributes       (GcrParsed *parsed,
                                                    GckAttributes *attrs);

void              _gcr_parsed_set_label            (GcrParsed *parsed,
                                                    const gchar *label);

void              _gcr_parsed_parsing_block        (GcrParsed *parsed,
                                                    gint format,
                                                    GBytes *data);

void              _gcr_parsed_parsing_object       (GcrParsed *parsed,
                                                    CK_OBJECT_CLASS klass);

GcrParsed        *_gcr_parser_push_parsed          (GcrParser *self,
                                                    gboolean sensitive);

void              _gcr_parser_pop_parsed           (GcrParser *self,
                                                    GcrParsed *parsed);

void              _gcr_parser_fire_parsed          (GcrParser *self,
                                                    GcrParsed *parsed);

typedef struct {
	gint ask_state;
	gint seen;
} PasswordState;

#define PASSWORD_STATE_INIT { 0, 0 }

gint              _gcr_enum_next_password          (GcrParser *self,
                                                    PasswordState *state,
                                                    const gchar **password);

#endif /* GCR_INTERNAL_H_ */
