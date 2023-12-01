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

#ifndef EGG_CRYPTO_H_
#define EGG_CRYPTO_H_

#include <glib.h>

typedef enum {
	EGG_HASH_UNKNOWN,
	EGG_HASH_SHA1,
	EGG_HASH_RIPEMD160,
} EggHashAlgorithm;

typedef struct EggHasher EggHasher;

EggHasher *egg_hasher_new           (EggHashAlgorithm algo);
gboolean   egg_hasher_hash          (EggHasher *hasher,
                                     gconstpointer data,
                                     gsize size);
GBytes    *egg_hasher_free_to_bytes (EggHasher *hasher);
gboolean   egg_hash                 (EggHashAlgorithm algo,
                                     gconstpointer data,
                                     gsize size,
                                     gpointer output);

typedef enum {
	EGG_CIPHER_UNKNOWN,
	EGG_CIPHER_AES_128_CBC,
} EggCipherAlgorithm;

typedef struct EggCipher EggCipher;

EggCipher *egg_cipher_new           (EggCipherAlgorithm algo,
                                     gconstpointer key,
                                     gsize n_key,
                                     gconstpointer iv,
                                     gsize n_iv);
gboolean   egg_cipher_encrypt       (EggCipher *cipher,
                                     gconstpointer plaintext,
                                     gsize n_plaintext,
                                     gpointer ciphertext,
                                     gsize n_ciphertext);
gboolean   egg_cipher_decrypt       (EggCipher *cipher,
                                     gconstpointer ciphertext,
                                     gsize n_ciphertext,
                                     gpointer plaintext,
                                     gsize n_plaintext);
void       egg_cipher_free          (EggCipher *cipher);

typedef enum {
	EGG_RANDOM_NONCE,
	EGG_RANDOM_KEY,
} EggRandomStrength;

void       egg_random               (EggRandomStrength strength,
                                     gpointer data,
                                     gsize size);

#endif /* EGG_CRYPTO_H_ */
