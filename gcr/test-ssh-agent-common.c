/*
 * gnome-keyring
 *
 * Copyright (C) 2018 Red Hat, Inc.
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Daiki Ueno
 */

#include "config.h"

#include "test-common.h"

#include "gcr-ssh-agent-private.h"
#include "gcr-ssh-agent-util.h"

/* RSA private key blob decoded from pkcs11/ssh-store/fixtures/id_rsa_plain */
static const guint8 private_blob[4*6 + 0x101 + 0x1 + 0x101 + 0x80 + 0x81 + 0x81] = {
	/* n */
	0x00, 0x00, 0x01, 0x01, 0x00, 0xa0, 0x3e, 0x95, 0x2a, 0xa9, 0x21, 0x6b,
	0x2e, 0xa9, 0x28, 0x74,	0x91, 0x8c, 0x01, 0x96, 0x59, 0xf1, 0x4f, 0x53,
	0xcc, 0x5f, 0xb2, 0x2d, 0xa0, 0x9c, 0xec, 0x0f,	0xfc, 0x1d, 0x54, 0x1c,
	0x3a, 0x33, 0xb7, 0x1d, 0xdc, 0xce, 0x13, 0xbe, 0xa7, 0x2f, 0xdf, 0x4e,
	0x58, 0x42, 0x9d, 0x23, 0xf5, 0x8e, 0xc8, 0xe4, 0xad, 0x52, 0x19, 0x72,
	0x7c, 0xda, 0x87, 0x67,	0xd4, 0x34, 0x51, 0x51, 0x81, 0x2e, 0x3e, 0x8d,
	0x13, 0x81, 0xb6, 0xf6, 0xe0, 0x1e, 0xc4, 0xbb,	0xd9, 0x5d, 0x44, 0xeb,
	0xe6, 0x68, 0x81, 0x5f, 0xa6, 0x04, 0x95, 0x96, 0x02, 0x1c, 0x34, 0x88,
	0xfa, 0xe6, 0x43, 0x72, 0xaf, 0x9b, 0x7f, 0x03, 0xdc, 0xf0, 0x72, 0xa3,
	0x96, 0x3b, 0xc8, 0xa3,	0xb9, 0x90, 0x81, 0xb6, 0x2e, 0x5a, 0x18, 0x2e,
	0x3a, 0x2c, 0x27, 0x91, 0x78, 0xb3, 0x1d, 0xb1,	0x87, 0x4b, 0xb3, 0xdb,
	0x05, 0xcd, 0xb6, 0x76, 0x35, 0x6f, 0x9c, 0x61, 0x7b, 0x6f, 0x95, 0x12,
	0x4b, 0x26, 0xf4, 0xe0, 0x7e, 0x15, 0x76, 0x94, 0x91, 0x90, 0xb6, 0x7d,
	0x0a, 0xd3, 0x36, 0x8f,	0x19, 0x18, 0x52, 0x50, 0x48, 0x57, 0x7c, 0x91,
	0x48, 0x48, 0x7d, 0xb5, 0x03, 0x26, 0x69, 0x58,	0xb9, 0x9f, 0xaf, 0xbc,
	0x73, 0x3e, 0x03, 0x72, 0xdc, 0xf6, 0xb1, 0xf2, 0x5b, 0x82, 0x0f, 0x69,
	0x1c, 0xb1, 0x15, 0x07, 0x22, 0x46, 0x66, 0xfe, 0x65, 0x0a, 0x94, 0xda,
	0xe4, 0x9d, 0x39, 0x70,	0x21, 0x83, 0x5e, 0xe5, 0xb2, 0x4b, 0x97, 0xfe,
	0xaf, 0x32, 0x08, 0x8e, 0x47, 0xcb, 0x97, 0x83,	0x89, 0xc0, 0xb6, 0xdb,
	0x6a, 0x14, 0x31, 0xd2, 0x53, 0xb5, 0x88, 0x30, 0x5f, 0x87, 0x50, 0x09,
	0x4f, 0x13, 0x20, 0x25, 0xa1, 0xc5, 0xbd, 0xf1, 0xe1, 0x10, 0x95, 0xfa,
	0x0e, 0xc3, 0xf7, 0xdf,	0xad, 0x90, 0x8b, 0xef, 0xfb,
	/* e */
	0x00, 0x00, 0x00, 0x01, 0x23,
	/* d */
	0x00, 0x00, 0x01, 0x01, 0x00, 0x9b, 0xaa, 0x82, 0x46, 0xb2, 0xed, 0x43,
	0x8c, 0x69, 0xcf, 0x87,	0x2e, 0x4d, 0x7d, 0xe2, 0x83, 0x42, 0x2f, 0xcd,
	0xbf, 0x38, 0x63, 0xf1, 0xcf, 0x39, 0x5a, 0x58,	0xab, 0xc4, 0xb8, 0x1b,
	0x6b, 0xbd, 0x35, 0x8a, 0xb9, 0x3d, 0x37, 0xc0, 0x85, 0x27, 0x30, 0xb2,
	0x81, 0x9f, 0xcb, 0xd9, 0xc9, 0xf8, 0x6b, 0x61, 0xcc, 0xf0, 0xab, 0x01,
	0x80, 0x99, 0xc5, 0x5d,	0x8c, 0x50, 0x14, 0x7b, 0x0f, 0xc6, 0x85, 0xe8,
	0x21, 0x93, 0xf3, 0x90, 0xbc, 0x75, 0xa9, 0x2b,	0x82, 0xb2, 0x60, 0x35,
	0x9d, 0xff, 0x1e, 0x97, 0x6e, 0x13, 0x14, 0xf8, 0x1f, 0x4e, 0x99, 0x6f,
	0x1f, 0x9d, 0xdb, 0x1e, 0xf3, 0xbb, 0x9f, 0xf5, 0x1f, 0xc5, 0x01, 0xa6,
	0x3a, 0x2b, 0x72, 0x73,	0x29, 0x4a, 0x8c, 0xa2, 0x58, 0xe9, 0xce, 0x58,
	0xca, 0xcb, 0xce, 0xaa, 0x92, 0x82, 0x1c, 0xd8,	0x57, 0x8b, 0x5e, 0x42,
	0x79, 0x21, 0x0e, 0x63, 0x13, 0x0e, 0x03, 0xff, 0x2f, 0x7f, 0x64, 0xf6,
	0x82, 0xe1, 0xfe, 0x0b, 0xc3, 0x1e, 0x4c, 0x50, 0x11, 0x3f, 0xc8, 0x8a,
	0xba, 0xcc, 0xde, 0x24,	0xf7, 0xae, 0x96, 0x6c, 0x5e, 0x3b, 0x00, 0xfa,
	0xf0, 0x0e, 0xac, 0x3a, 0xeb, 0xb1, 0xab, 0x8f,	0x3f, 0xdb, 0x80, 0xb3,
	0x06, 0x91, 0x18, 0xe1, 0xf5, 0x3b, 0xec, 0x5d, 0x01, 0xcf, 0xd0, 0x1f,
	0xaf, 0xe3, 0xd9, 0x12, 0xba, 0x7b, 0x0f, 0xee, 0x20, 0x29, 0x74, 0x57,
	0xdc, 0x58, 0x75, 0xd4,	0xb0, 0xf4, 0xb4, 0xa4, 0x93, 0x48, 0x2b, 0x7b,
	0x6b, 0x1d, 0x77, 0xbc, 0xf3, 0xfe, 0xbd, 0xad,	0xd6, 0x83, 0x05, 0x16,
	0xca, 0xbe, 0x31, 0xa4, 0x39, 0x53, 0x29, 0xf3, 0xd3, 0x39, 0xb0, 0xa5,
	0xef, 0xf0, 0xc9, 0x08, 0xd6, 0x63, 0x52, 0x0b, 0xcb, 0xfc, 0x1c, 0x21,
	0xd3, 0xa9, 0x2f, 0x23,	0x92, 0x3d, 0x46, 0x8c, 0x4b,
	/* iqmp */
	0x00, 0x00, 0x00, 0x80, 0x15, 0x40, 0xcc, 0xa4, 0x83, 0xdf, 0x26, 0xbe,
	0x55, 0x82, 0x85, 0x0f,	0x71, 0x3c, 0x19, 0xa8, 0x8b, 0x42, 0x80, 0xa5,
	0x24, 0x5d, 0xad, 0xf5, 0x99, 0x33, 0xaf, 0x7c,	0xb2, 0x27, 0xae, 0x7b,
	0x0b, 0x0b, 0xa0, 0x03, 0xfd, 0xae, 0x53, 0x6f, 0xf1, 0xdd, 0x83, 0x54,
	0xde, 0xf2, 0xbd, 0x87, 0x2c, 0xa9, 0x4d, 0x7b, 0xa5, 0x6e, 0xdb, 0x5e,
	0x89, 0xf4, 0x5c, 0x79,	0x22, 0xc3, 0xc4, 0x40, 0x50, 0xeb, 0xb7, 0xf4,
	0x17, 0x78, 0x2f, 0x06, 0xa5, 0x3a, 0x65, 0x4d,	0x85, 0x98, 0x3e, 0xd8,
	0x4d, 0x3b, 0xfc, 0xd8, 0x9b, 0xe5, 0xd1, 0x47, 0xb6, 0xe3, 0xda, 0x2e,
	0xc5, 0x18, 0xce, 0x37, 0xd9, 0xd7, 0x9a, 0xbf, 0xba, 0xa9, 0xef, 0xf2,
	0xaf, 0x9b, 0xc8, 0x46,	0x57, 0x11, 0x8c, 0xa9, 0x5f, 0x68, 0x8c, 0x43,
	0x2f, 0xb5, 0x7a, 0x39, 0x38, 0x30, 0x79, 0xd5,	0x30, 0xa8, 0x2b, 0x98,
	/* p */
	0x00, 0x00, 0x00, 0x81, 0x00, 0xcc, 0x50, 0xb1, 0x2c, 0x5f, 0xe4, 0x02,
	0x85, 0x7d, 0xce, 0x77,	0xd8, 0x27, 0xc1, 0xf6, 0xee, 0xe2, 0x2b, 0x7b,
	0x29, 0x83, 0x95, 0xf1, 0x5e, 0x3d, 0xe5, 0xa9,	0x75, 0x62, 0xc6, 0x84,
	0xc9, 0x97, 0x26, 0x70, 0xf4, 0x0d, 0x28, 0x6a, 0xc6, 0x88, 0x7c, 0xa3,
	0x0d, 0x35, 0xa3, 0x8f, 0xdc, 0x34, 0x4c, 0x78, 0x6b, 0xcc, 0x5d, 0x99,
	0x7e, 0x45, 0xb0, 0xdf,	0xe3, 0x77, 0x48, 0x77, 0xd8, 0xa9, 0x1c, 0x74,
	0xf9, 0xbc, 0xcc, 0x82, 0xdb, 0x44, 0x10, 0x96,	0xda, 0x00, 0x23, 0xaa,
	0x04, 0x93, 0xcc, 0x98, 0xec, 0x26, 0x8b, 0x7d, 0x08, 0xf4, 0x82, 0xdc,
	0x9a, 0xc4, 0x8c, 0xc8, 0xe9, 0x3e, 0x5b, 0xd6, 0xc7, 0x28, 0xf4, 0x38,
	0x3a, 0x3c, 0x08, 0x56,	0xbb, 0xa2, 0xca, 0xfb, 0x05, 0xa0, 0xb7, 0xe1,
	0x70, 0x59, 0xb4, 0x86, 0x2b, 0x29, 0x89, 0xb5,	0x82, 0x2a, 0x79, 0x61,
	0x51,
	/* q */
	0x00, 0x00, 0x00, 0x81, 0x00, 0xc8, 0xc7, 0xe6, 0x93, 0x90, 0x59, 0xe7,
	0x54, 0x1b, 0xcf, 0x9c,	0xb0, 0x07, 0x80, 0x37, 0xcd, 0xdf, 0x65, 0xf4,
	0x29, 0x1e, 0x4a, 0x93, 0x73, 0xd1, 0x7b, 0x47,	0x1d, 0x36, 0x87, 0x89,
	0x1d, 0xbf, 0xd5, 0x1e, 0x02, 0xc2, 0xd1, 0x2b, 0xb3, 0x67, 0x07, 0x65,
	0xf9, 0xbc, 0xcb, 0x74, 0x4c, 0x83, 0x68, 0xa8, 0x6d, 0x30, 0x68, 0x8f,
	0xb5, 0xb9, 0x44, 0x86,	0xb8, 0xde, 0x4e, 0xfc, 0x02, 0x1e, 0x9c, 0x05,
	0x3b, 0x23, 0x1b, 0xdf, 0x79, 0x58, 0x73, 0x51,	0x27, 0xf0, 0xbd, 0x83,
	0x34, 0x38, 0xcb, 0xd0, 0x20, 0x12, 0xcd, 0x1a, 0x07, 0x6e, 0xf7, 0x0a,
	0x92, 0x29, 0xff, 0x2f, 0xbf, 0x30, 0x2a, 0x69, 0x15, 0x4d, 0x8e, 0x6e,
	0x17, 0x26, 0x7b, 0x43,	0xfe, 0x52, 0xd1, 0x83, 0x65, 0x19, 0x22, 0x8b,
	0xd3, 0x6f, 0x97, 0x51, 0x11, 0x3f, 0x17, 0xfe,	0x05, 0xcc, 0xa4, 0x49,
	0x8b
};

void
prepare_request_identities (EggBuffer *req)
{
	gboolean ret;

	egg_buffer_reset (req);

	ret = egg_buffer_add_uint32 (req, 1);
	g_assert_true (ret);

	ret = egg_buffer_add_byte (req, GCR_SSH_OP_REQUEST_IDENTITIES);
	g_assert_true (ret);
}

void
check_identities_answer (EggBuffer *resp, gsize count)
{
	uint32_t length;
	unsigned char code;
	size_t offset;
	gboolean ret;

	offset = 0;
	ret = egg_buffer_get_uint32 (resp, offset, &offset, &length);
	g_assert_true (ret);
	g_assert_cmpint (length, ==, resp->len - 4);

	code = 0;
	ret = egg_buffer_get_byte (resp, offset, &offset, &code);
	g_assert_true (ret);
	g_assert_cmpint (code, ==, GCR_SSH_RES_IDENTITIES_ANSWER);

	ret = egg_buffer_get_uint32 (resp, offset, &offset, &length);
	g_assert_true (ret);
	g_assert_cmpint (length, ==, count);
}

void
prepare_add_identity (EggBuffer *req)
{
	gboolean ret;

	egg_buffer_reset (req);

	ret = egg_buffer_add_uint32 (req, 0);
	g_assert_true (ret);

	ret = egg_buffer_add_byte (req, GCR_SSH_OP_ADD_IDENTITY);
	g_assert_true (ret);

	ret = egg_buffer_add_string (req, "ssh-rsa");
	g_assert_true (ret);

	ret = egg_buffer_append (req, private_blob, G_N_ELEMENTS(private_blob));
	g_assert_true (ret);

	ret = egg_buffer_add_string (req, "comment");
	g_assert_true (ret);

	ret = egg_buffer_set_uint32 (req, 0, req->len - 4);
	g_assert_true (ret);
}

GBytes *
public_key_from_file (const gchar *path, gchar **comment)
{
	GBytes *public_bytes;
	GBytes *public_key;

	GError *error = NULL;
	gchar *contents;
	gsize length;

	if (!g_file_get_contents (path, &contents, &length, &error)) {
		g_message ("couldn't read file: %s: %s", path, error->message);
		g_error_free (error);
		return NULL;
	}

	public_bytes = g_bytes_new_take (contents, length);
	public_key = _gcr_ssh_agent_parse_public_key (public_bytes, comment);
	g_bytes_unref (public_bytes);

	return public_key;
}

void
prepare_remove_identity (EggBuffer *req)
{
	GBytes *public_key;
	gchar *comment;
	gsize length;
	const guchar *blob;
	gboolean ret;

	public_key = public_key_from_file (SRCDIR "/pkcs11/ssh-store/fixtures/id_rsa_plain.pub", &comment);
	g_free (comment);
	blob = g_bytes_get_data (public_key, &length);

	egg_buffer_reset (req);
	ret = egg_buffer_add_uint32 (req, 0);
	g_assert_true (ret);

	ret = egg_buffer_add_byte (req, GCR_SSH_OP_REMOVE_IDENTITY);
	g_assert_true (ret);

	ret = egg_buffer_add_byte_array (req, blob, length);
	g_assert_true (ret);

	ret = egg_buffer_set_uint32 (req, 0, req->len - 4);
	g_assert_true (ret);

	g_bytes_unref (public_key);
}

void
prepare_remove_all_identities (EggBuffer *req)
{
	gboolean ret;

	egg_buffer_reset (req);
	ret = egg_buffer_add_uint32 (req, 1);
	g_assert_true (ret);

	ret = egg_buffer_add_byte (req, GCR_SSH_OP_REMOVE_ALL_IDENTITIES);
	g_assert_true (ret);
}

void
check_response (EggBuffer *resp, unsigned char expected)
{
	uint32_t length;
	unsigned char code;
	size_t offset;
	gboolean ret;

	offset = 0;
	ret = egg_buffer_get_uint32 (resp, offset, &offset, &length);
	g_assert_true (ret);
	g_assert_cmpint (length, ==, resp->len - 4);

	code = 0;
	ret = egg_buffer_get_byte (resp, offset, &offset, &code);
	g_assert_true (ret);
	g_assert_cmpint (expected, ==, code);
}

void
check_success (EggBuffer *resp)
{
	check_response (resp, GCR_SSH_RES_SUCCESS);
}

void
check_failure (EggBuffer *resp)
{
	check_response (resp, GCR_SSH_RES_FAILURE);
}

void
prepare_sign_request (EggBuffer *req)
{
	GBytes *public_key;
	gchar *comment;
	gsize length;
	const guchar *blob;
	gboolean ret;

	public_key = public_key_from_file (SRCDIR "/gcr/fixtures/ssh-agent/id_rsa_plain.pub", &comment);
	g_free (comment);
	blob = g_bytes_get_data (public_key, &length);

	egg_buffer_reset (req);
	ret = egg_buffer_add_uint32 (req, 0);
	g_assert_true (ret);

	ret = egg_buffer_add_byte (req, GCR_SSH_OP_SIGN_REQUEST);
	g_assert_true (ret);

	ret = egg_buffer_add_byte_array (req, blob, length);
	g_assert_true (ret);

	ret = egg_buffer_add_string (req, "data");
	g_assert_true (ret);

	ret = egg_buffer_add_uint32 (req, GCR_SSH_FLAG_RSA_SHA2_256);
	g_assert_true (ret);

	ret = egg_buffer_set_uint32 (req, 0, req->len - 4);
	g_assert_true (ret);

	g_bytes_unref (public_key);
}

void
check_sign_response (EggBuffer *resp)
{
	uint32_t length;
	unsigned char code;
	size_t offset;
	gboolean ret;

	offset = 0;
	ret = egg_buffer_get_uint32 (resp, offset, &offset, &length);
	g_assert_true (ret);
	g_assert_cmpint (length, ==, resp->len - 4);

	code = 0;
	ret = egg_buffer_get_byte (resp, offset, &offset, &code);
	g_assert_true (ret);
	g_assert_cmpint (code, ==, GCR_SSH_RES_SIGN_RESPONSE);

	ret = egg_buffer_get_uint32 (resp, offset, &offset, &length);
	g_assert_true (ret);
}
