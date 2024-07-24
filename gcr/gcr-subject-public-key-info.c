/*
 * gcr
 *
 * Copyright (C) Niels De Graef
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
 * Author: Niels De Graef <nielsdegraef@gmail.com>
 */
#include "config.h"

#include "gcr-subject-public-key-info.h"

#include "gcr-subject-public-key.h"
#include "gcr-types.h"

#include "gcr/gcr-oids.h"
#include "egg/egg-oid.h"

#include "egg/egg-asn1x.h"
#include "egg/egg-asn1-defs.h"

struct _GcrSubjectPublicKeyInfo {
	guint key_size;
	GBytes *key;
	GQuark algo_oid;
	GBytes *algo_params;
};

GcrSubjectPublicKeyInfo *
gcr_subject_public_key_info_copy (GcrSubjectPublicKeyInfo *self)
{
	GcrSubjectPublicKeyInfo *copy;

	copy = g_new0 (GcrSubjectPublicKeyInfo, 1);
	copy->key_size = self->key_size;
	copy->key = g_bytes_ref (self->key);
	copy->algo_oid = copy->algo_oid;
	copy->algo_params = g_bytes_ref (self->algo_params);

	return copy;
}

void
gcr_subject_public_key_info_free (GcrSubjectPublicKeyInfo *self)
{
	if (self == NULL)
		return;

	g_bytes_unref (self->key);
	g_bytes_unref (self->algo_params);
	g_free (self);
}

G_DEFINE_BOXED_TYPE (GcrSubjectPublicKeyInfo, gcr_subject_public_key_info,
                     gcr_subject_public_key_info_copy,
                     gcr_subject_public_key_info_free)

/**
 * gcr_subject_public_key_info_get_algorithm_oid:
 *
 * Returns the OID of the algorithm used by the public key.
 */
const char *
gcr_subject_public_key_info_get_algorithm_oid (GcrSubjectPublicKeyInfo *self)
{
	g_return_val_if_fail (self, NULL);

	return g_quark_to_string (self->algo_oid);
}

/**
 * gcr_subject_public_key_info_get_algorithm_description:
 *
 * Returns a user-facing description of the algorithm used by the public key.
 */
const char *
gcr_subject_public_key_info_get_algorithm_description (GcrSubjectPublicKeyInfo *self)
{
	g_return_val_if_fail (self, NULL);

	return egg_oid_get_description (self->algo_oid);
}

/**
 * gcr_subject_public_key_info_get_algorithm_parameters_raw:
 *
 * Returns the raw bytes describing the parameters for the public key's
 * algorithm. Their meaning is algorithm-specific
 *
 * Returns: (transfer none): The raw bytes describing the algorithm's parameters
 */
GBytes *
gcr_subject_public_key_info_get_algorithm_parameters_raw (GcrSubjectPublicKeyInfo *self)
{
	g_return_val_if_fail (self, NULL);

	return self->algo_params;
}

/**
 * gcr_subject_public_key_info_get_key:
 *
 * Returns the public key.
 *
 * Returns: (transfer none): The raw data of the public key
 */
GBytes *
gcr_subject_public_key_info_get_key (GcrSubjectPublicKeyInfo *self)
{
	g_return_val_if_fail (self, NULL);

	return self->key;
}

/**
 * gcr_subject_public_key_info_get_key_size:
 *
 * Returns the size of the public key.
 *
 * Returns: The key size
 */
unsigned int
gcr_subject_public_key_info_get_key_size (GcrSubjectPublicKeyInfo *self)
{
	g_return_val_if_fail (self, 0);

	return self->key_size;
}

GcrSubjectPublicKeyInfo *
_gcr_subject_public_key_info_new (GNode *key_info_node)
{
	GcrSubjectPublicKeyInfo *self;
	GNode *child_node;
	guint bits;

	g_return_val_if_fail (key_info_node, NULL);

	self = g_new0 (GcrSubjectPublicKeyInfo, 1);

	self->key_size = _gcr_subject_public_key_calculate_size (key_info_node);

	child_node = egg_asn1x_node (key_info_node, "subjectPublicKey", NULL);
	self->key = egg_asn1x_get_bits_as_raw (child_node, &bits);

	child_node = egg_asn1x_node (key_info_node, "algorithm", "algorithm", NULL);
	self->algo_oid = egg_asn1x_get_oid_as_quark (child_node);

	child_node = egg_asn1x_node (key_info_node, "algorithm", "parameters", NULL);
	self->algo_params = egg_asn1x_get_element_raw (child_node);

	return self;
}
