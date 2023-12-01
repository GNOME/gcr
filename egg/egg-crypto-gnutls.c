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

#include "egg-crypto.h"

#include <gnutls/crypto.h>
#include <gnutls/gnutls.h>

struct EggHasher {
	gnutls_hash_hd_t mdh;
	gnutls_digest_algorithm_t dig;
};

static inline gnutls_digest_algorithm_t
hash_algo_to_gnutls (EggHashAlgorithm algo)
{
	switch (algo) {
	case EGG_HASH_SHA1:
		return GNUTLS_DIG_SHA1;
	case EGG_HASH_RIPEMD160:
		return GNUTLS_DIG_RMD160;
	default:
		return GNUTLS_DIG_UNKNOWN;
	}
}

EggHasher *
egg_hasher_new (EggHashAlgorithm algo)
{
	EggHasher *hasher;
	gnutls_digest_algorithm_t dig;
	int ret;

	dig = hash_algo_to_gnutls (algo);
	g_return_val_if_fail (dig != GNUTLS_DIG_UNKNOWN, NULL);

	hasher = g_new0 (EggHasher, 1);
	g_return_val_if_fail (hasher, NULL);

	ret = gnutls_hash_init (&hasher->mdh, dig);
	if (ret < 0) {
		g_free (hasher);
		return NULL;
	}
	hasher->dig = dig;

	return hasher;
}

gboolean
egg_hasher_hash (EggHasher *hasher,
		 gconstpointer data,
		 gsize size)
{
	return gnutls_hash (hasher->mdh, data, size) == GNUTLS_E_SUCCESS;
}

GBytes *
egg_hasher_free_to_bytes (EggHasher *hasher)
{
	guchar *data;
	gsize size;
	GBytes *result;

	size = gnutls_hash_get_len (hasher->dig);
	data = g_malloc (size);
	g_return_val_if_fail (data, NULL);

	gnutls_hash_deinit (hasher->mdh, data);

	result = g_bytes_new_take (data, size);
	g_free (hasher);
	return result;
}

gboolean
egg_hash (EggHashAlgorithm algo,
	  gconstpointer data,
	  gsize size,
	  gpointer output)
{
	return gnutls_hash_fast (hash_algo_to_gnutls (algo), data, size, output) == GNUTLS_E_SUCCESS;
}

struct EggCipher {
	gnutls_cipher_hd_t cih;
};

static inline gnutls_cipher_algorithm_t
cipher_algo_to_gnutls (EggCipherAlgorithm algo)
{
	switch (algo) {
	case EGG_CIPHER_AES_128_CBC:
		return GNUTLS_CIPHER_AES_128_CBC;
	default:
		return GNUTLS_CIPHER_UNKNOWN;
	}
}

EggCipher *
egg_cipher_new (EggCipherAlgorithm algo,
		gconstpointer key,
		gsize n_key,
		gconstpointer iv,
		gsize n_iv)
{
	EggCipher *cipher;
	gnutls_cipher_algorithm_t galgo;
	gnutls_datum_t key_data, iv_data;
	int ret;

	galgo = cipher_algo_to_gnutls (algo);
	g_return_val_if_fail (galgo != GNUTLS_CIPHER_UNKNOWN, NULL);

	cipher = g_new0 (EggCipher, 1);
	g_return_val_if_fail (cipher, NULL);

	key_data.data = (unsigned char *)key;
	key_data.size = n_key;

	iv_data.data = (unsigned char *)iv;
	iv_data.size = n_iv;

	ret = gnutls_cipher_init (&cipher->cih, galgo, &key_data, &iv_data);
	if (ret < 0) {
		egg_cipher_free (cipher);
		return NULL;
	}

	return cipher;
}

gboolean
egg_cipher_encrypt (EggCipher *cipher,
		    gconstpointer plaintext,
		    gsize n_plaintext,
		    gpointer ciphertext,
		    gsize n_ciphertext)
{
	return gnutls_cipher_encrypt2 (cipher->cih,
				       plaintext,
				       n_plaintext,
				       ciphertext,
				       n_ciphertext) == GNUTLS_E_SUCCESS;
}

gboolean
egg_cipher_decrypt (EggCipher *cipher,
		    gconstpointer ciphertext,
		    gsize n_ciphertext,
		    gpointer plaintext,
		    gsize n_plaintext)
{
	return gnutls_cipher_decrypt2 (cipher->cih,
				       ciphertext,
				       n_ciphertext,
				       plaintext,
				       n_plaintext) == GNUTLS_E_SUCCESS;
}

void
egg_cipher_free (EggCipher *cipher)
{
	if (!cipher)
		return;
	if (cipher->cih)
		gnutls_cipher_deinit (cipher->cih);
	g_free (cipher);
}

static inline int
strength_to_gnutls (EggRandomStrength strength)
{
	switch (strength) {
	case EGG_RANDOM_NONCE:
		return GNUTLS_RND_NONCE;
	case EGG_RANDOM_KEY:
		return GNUTLS_RND_KEY;
	default:
		g_return_val_if_reached (GNUTLS_RND_NONCE);
	}
}
void
egg_random (EggRandomStrength strength,
	    gpointer data,
	    gsize size)
{
	int level;

	level = strength_to_gnutls (strength);
	g_assert (level >= 0);

	gnutls_rnd (level, data, size);
}
