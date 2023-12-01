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

#include <gcrypt.h>

struct EggHasher {
	gcry_md_hd_t mdh;
};

static inline int
hash_algo_to_gcry (EggHashAlgorithm algo)
{
	switch (algo) {
	case EGG_HASH_SHA1:
		return GCRY_MD_SHA1;
	case EGG_HASH_RIPEMD160:
		return GCRY_MD_RMD160;
	default:
		return GCRY_MD_NONE;
	}
}

EggHasher *
egg_hasher_new (EggHashAlgorithm algo)
{
	EggHasher *hasher;
	gcry_error_t gcry;

	hasher = g_new0 (EggHasher, 1);
	g_return_val_if_fail (hasher, NULL);

	gcry = gcry_md_open (&hasher->mdh, hash_algo_to_gcry (algo), 0);
	if (gcry) {
		g_free (hasher);
		return NULL;
	}

	return hasher;
}

gboolean
egg_hasher_hash (EggHasher *hasher,
		 gconstpointer data,
		 gsize size)
{
	gcry_md_write (hasher->mdh, data, size);
	return TRUE;
}

GBytes *
egg_hasher_free_to_bytes (EggHasher *hasher)
{
	guchar *data;
	GBytes *result;

	data = gcry_md_read (hasher->mdh, 0);

	result = g_bytes_new (data,
			      gcry_md_get_algo_dlen (gcry_md_get_algo (hasher->mdh)));
	gcry_md_close (hasher->mdh);
	g_free (hasher);
	return result;
}

gboolean
egg_hash (EggHashAlgorithm algo,
	  gconstpointer data,
	  gsize size,
	  gpointer output)
{
	gcry_md_hash_buffer (hash_algo_to_gcry (algo), output, data, size);
	return TRUE;
}

struct EggCipher {
	gcry_cipher_hd_t cih;
};

static inline int
cipher_algo_to_gcry (EggCipherAlgorithm algo, int *mode)
{
	switch (algo) {
	case EGG_CIPHER_AES_128_CBC:
		*mode = GCRY_CIPHER_MODE_CBC;
		return GCRY_CIPHER_AES128;
	default:
		*mode = GCRY_CIPHER_MODE_NONE;
		return GCRY_CIPHER_NONE;
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
	gcry_error_t gcry;
	int galgo, mode;

	cipher = g_new0 (EggCipher, 1);
	g_return_val_if_fail (cipher, NULL);

	galgo = cipher_algo_to_gcry (algo, &mode);
	gcry = gcry_cipher_open (&cipher->cih, galgo, mode, 0);
	if (gcry)
		goto out;

	gcry = gcry_cipher_setkey (cipher->cih, key, n_key);
	if (gcry)
		goto out;

	gcry = gcry_cipher_setiv (cipher->cih, iv, n_iv);
	if (gcry)
		goto out;

 out:
	if (gcry) {
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
	gcry_error_t gcry;

	gcry = gcry_cipher_encrypt (cipher->cih, ciphertext, n_ciphertext,
				    plaintext, n_plaintext);

	return gcry == 0;
}

gboolean
egg_cipher_decrypt (EggCipher *cipher,
		    gconstpointer ciphertext,
		    gsize n_ciphertext,
		    gpointer plaintext,
		    gsize n_plaintext)
{
	gcry_error_t gcry;

	gcry = gcry_cipher_decrypt (cipher->cih, plaintext, n_plaintext,
				    ciphertext, n_ciphertext);

	return gcry == 0;
}

void
egg_cipher_free (EggCipher *cipher)
{
	if (!cipher)
		return;
	if (cipher->cih)
		gcry_cipher_close (cipher->cih);
	g_free (cipher);
}

void
egg_random (EggRandomStrength strength,
	    gpointer data,
	    gsize size)
{
	switch (strength) {
	case EGG_RANDOM_NONCE:
		gcry_create_nonce (data, size);
		break;
	case EGG_RANDOM_KEY:
		gcry_randomize (data, size, GCRY_STRONG_RANDOM);
		break;
	default:
		g_return_if_reached ();
	}
}
