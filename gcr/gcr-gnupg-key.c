/*
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

#include "gcr-gnupg-key.h"
#include "gcr-gnupg-records.h"
#include "gcr-record.h"
#include "gcr-memory-icon.h"

#include "gck/gck.h"

#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_KEYID,
	PROP_PUBLIC_RECORDS,
	PROP_SECRET_RECORDS,
	PROP_LABEL,
	PROP_MARKUP,
	PROP_DESCRIPTION,
	PROP_SHORT_KEYID,
	PROP_ICON
};

struct _GcrGnupgKeyPrivate {
	GPtrArray *public_records;
	GPtrArray *secret_records;
	GIcon *icon;
};

G_DEFINE_TYPE_WITH_PRIVATE (GcrGnupgKey, _gcr_gnupg_key, G_TYPE_OBJECT);

static gchar *
calculate_name (GcrGnupgKey *self)
{
	GcrRecord* record;

	record = _gcr_records_find (self->pv->public_records, GCR_RECORD_SCHEMA_UID);
	g_return_val_if_fail (record, NULL);

	return _gcr_record_get_string (record, GCR_RECORD_UID_USERID);
}

static gchar *
calculate_markup (GcrGnupgKey *self)
{
	gchar *markup = NULL;
	gchar *uid, *name, *email, *comment;

	uid = calculate_name (self);
	if (uid == NULL)
		return NULL;

	_gcr_gnupg_records_parse_user_id (uid, &name, &email, &comment);
	if (comment != NULL && comment[0] != '\0')
		markup = g_markup_printf_escaped ("%s\n<small>%s \'%s\'</small>", name, email, comment);
	else
		markup = g_markup_printf_escaped ("%s\n<small>%s</small>", name, email);
	g_free (name);
	g_free (email);
	g_free (comment);
	g_free (uid);

	return markup;
}

static void
_gcr_gnupg_key_init (GcrGnupgKey *self)
{
	self->pv = _gcr_gnupg_key_get_instance_private (self);
}

static void
_gcr_gnupg_key_finalize (GObject *obj)
{
	GcrGnupgKey *self = GCR_GNUPG_KEY (obj);

	if (self->pv->public_records)
		g_ptr_array_unref (self->pv->public_records);
	if (self->pv->secret_records)
		g_ptr_array_unref (self->pv->secret_records);

	G_OBJECT_CLASS (_gcr_gnupg_key_parent_class)->finalize (obj);
}

static void
_gcr_gnupg_key_set_property (GObject *obj, guint prop_id, const GValue *value,
                             GParamSpec *pspec)
{
	GcrGnupgKey *self = GCR_GNUPG_KEY (obj);

	switch (prop_id) {
	case PROP_PUBLIC_RECORDS:
		_gcr_gnupg_key_set_public_records (self, g_value_get_boxed (value));
		break;
	case PROP_SECRET_RECORDS:
		_gcr_gnupg_key_set_secret_records (self, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
_gcr_gnupg_key_get_property (GObject *obj, guint prop_id, GValue *value,
                             GParamSpec *pspec)
{
	GcrGnupgKey *self = GCR_GNUPG_KEY (obj);

	switch (prop_id) {
	case PROP_PUBLIC_RECORDS:
		g_value_set_boxed (value, self->pv->public_records);
		break;
	case PROP_SECRET_RECORDS:
		g_value_set_boxed (value, self->pv->secret_records);
		break;
	case PROP_KEYID:
		g_value_set_string (value, _gcr_gnupg_key_get_keyid (self));
		break;
	case PROP_LABEL:
		g_value_take_string (value, calculate_name (self));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, _("PGP Key"));
		break;
	case PROP_MARKUP:
		g_value_take_string (value, calculate_markup (self));
		break;
	case PROP_SHORT_KEYID:
		g_value_set_string (value, _gcr_gnupg_records_get_short_keyid (self->pv->public_records));
		break;
	case PROP_ICON:
		g_value_set_object (value, _gcr_gnupg_key_get_icon (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
_gcr_gnupg_key_class_init (GcrGnupgKeyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	_gcr_gnupg_key_parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = _gcr_gnupg_key_finalize;
	gobject_class->set_property = _gcr_gnupg_key_set_property;
	gobject_class->get_property = _gcr_gnupg_key_get_property;

	/**
	 * GcrGnupgKey:public-records:
	 *
	 * Public key data. Should always be present.
	 */
	g_object_class_install_property (gobject_class, PROP_PUBLIC_RECORDS,
	         g_param_spec_boxed ("public-records", "Public Records", "Public Key Colon Records",
	                             G_TYPE_PTR_ARRAY, G_PARAM_READWRITE));

	/**
	 * GcrGnupgKey:secret-records:
	 *
	 * Secret key data. The keyid of this data must match public-dataset.
	 * If present, this key represents a secret key.
	 */
	g_object_class_install_property (gobject_class, PROP_SECRET_RECORDS,
	         g_param_spec_boxed ("secret-records", "Secret Records", "Secret Key Colon Records",
	                             G_TYPE_PTR_ARRAY, G_PARAM_READWRITE));

	/**
	 * GcrGnupgKey:keyid:
	 *
	 * Key identifier.
	 */
	g_object_class_install_property (gobject_class, PROP_KEYID,
	         g_param_spec_string ("keyid", "Key ID", "Key identifier",
	                              "", G_PARAM_READABLE));

	/**
	 * GcrGnupgKey:label:
	 *
	 * User readable label for this key.
	 */
	g_object_class_install_property (gobject_class, PROP_LABEL,
	         g_param_spec_string ("label", "Label", "Key label",
	                              "", G_PARAM_READABLE));

	/**
	 * GcrGnupgKey::description:
	 *
	 * Description of type of key.
	 */
	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
	         g_param_spec_string ("description", "Description", "Description of object type",
	                              "", G_PARAM_READABLE));

	/**
	 * GcrGnupgKey:markup:
	 *
	 * User readable markup which contains key label.
	 */
	g_object_class_install_property (gobject_class, PROP_MARKUP,
	         g_param_spec_string ("markup", "Markup", "Markup which describes key",
	                              "", G_PARAM_READABLE));

	/**
	 * GcrGnupgKey:short-keyid:
	 *
	 * User readable key identifier.
	 */
	g_object_class_install_property (gobject_class, PROP_SHORT_KEYID,
	         g_param_spec_string ("short-keyid", "Short Key ID", "Display key identifier",
	                              "", G_PARAM_READABLE));

	/**
	 * GcrGnupgKey:icon:
	 *
	 * Icon for this key.
	 */
	g_object_class_install_property (gobject_class, PROP_ICON,
	         g_param_spec_object ("icon", "Icon", "Icon for this key",
	                              G_TYPE_ICON, G_PARAM_READABLE));
}

/**
 * _gcr_gnupg_key_new:
 * @pubset: array of GcrRecord* representing public part of key
 * @secset: (allow-none): array of GcrRecord* representing secret part of key.
 *
 * Create a new GcrGnupgKey for the record data passed. If the secret part
 * of the key is set, then this represents a secret key; otherwise it represents
 * a public key.
 *
 * Returns: (transfer full): A newly allocated key.
 */
GcrGnupgKey*
_gcr_gnupg_key_new (GPtrArray *pubset, GPtrArray *secset)
{
	g_return_val_if_fail (pubset, NULL);
	return g_object_new (GCR_TYPE_GNUPG_KEY,
	                     "public-records", pubset,
	                     "secret-records", secset,
	                     NULL);
}

/**
 * _gcr_gnupg_key_get_public_records:
 * @self: The key
 *
 * Get the record data this key is based on.
 *
 * Returns: (transfer none): An array of GcrRecord*.
 */
GPtrArray*
_gcr_gnupg_key_get_public_records (GcrGnupgKey *self)
{
	g_return_val_if_fail (GCR_IS_GNUPG_KEY (self), NULL);
	return self->pv->public_records;
}

/**
 * _gcr_gnupg_key_set_public_records:
 * @self: The key
 * @records: The new array of GcrRecord*
 *
 * Change the record data that this key is based on.
 */
void
_gcr_gnupg_key_set_public_records (GcrGnupgKey *self, GPtrArray *records)
{
	GObject *obj;

	g_return_if_fail (GCR_IS_GNUPG_KEY (self));
	g_return_if_fail (records);

	/* Check that it matches previous */
	if (self->pv->public_records) {
		const gchar *old_keyid = _gcr_gnupg_records_get_keyid (self->pv->public_records);
		const gchar *new_keyid = _gcr_gnupg_records_get_keyid (records);

		if (g_strcmp0 (old_keyid, new_keyid) != 0) {
			g_warning ("it is an error to change a gnupg key so that the "
			           "fingerprint is no longer the same: %s != %s",
			           old_keyid, new_keyid);
			return;
		}
	}

	g_ptr_array_ref (records);
	if (self->pv->public_records)
		g_ptr_array_unref (self->pv->public_records);
	self->pv->public_records = records;

	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	g_object_notify (obj, "public-records");
	g_object_notify (obj, "label");
	g_object_notify (obj, "markup");
	g_object_thaw_notify (obj);
}

/**
 * _gcr_gnupg_key_get_secret_records:
 * @self: The key
 *
 * Get the record secret data this key is based on. %NULL if a public key.
 *
 * Returns: (transfer none) (allow-none): An array of GcrColons*.
 */
GPtrArray*
_gcr_gnupg_key_get_secret_records (GcrGnupgKey *self)
{
	g_return_val_if_fail (GCR_IS_GNUPG_KEY (self), NULL);
	return self->pv->secret_records;
}

/**
 * _gcr_gnupg_key_set_secret_records:
 * @self: The key
 * @records: (allow-none): The new array of GcrRecord*
 *
 * Set the secret data for this key. %NULL if public key.
 */
void
_gcr_gnupg_key_set_secret_records (GcrGnupgKey *self, GPtrArray *records)
{
	GObject *obj;

	g_return_if_fail (GCR_IS_GNUPG_KEY (self));

	/* Check that it matches public key */
	if (self->pv->public_records && records) {
		const gchar *pub_keyid = _gcr_gnupg_records_get_keyid (self->pv->public_records);
		const gchar *sec_keyid = _gcr_gnupg_records_get_keyid (records);

		if (g_strcmp0 (pub_keyid, sec_keyid) != 0) {
			g_warning ("it is an error to create a gnupg key so that the "
			           "fingerprint of thet pub and sec parts are not the same: %s != %s",
			           pub_keyid, sec_keyid);
			return;
		}
	}

	if (records)
		g_ptr_array_ref (records);
	if (self->pv->secret_records)
		g_ptr_array_unref (self->pv->secret_records);
	self->pv->secret_records = records;

	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	g_object_notify (obj, "secret-records");
	g_object_thaw_notify (obj);
}

/**
 * _gcr_gnupg_key_get_keyid:
 * @self: The key
 *
 * Get the keyid for this key.
 *
 * Returns: (transfer none): The keyid.
 */
const gchar*
_gcr_gnupg_key_get_keyid (GcrGnupgKey *self)
{
	g_return_val_if_fail (GCR_IS_GNUPG_KEY (self), NULL);
	return _gcr_gnupg_records_get_keyid (self->pv->public_records);
}

/**
 * _gcr_gnupg_key_get_icon:
 * @self: A gnupg key.
 *
 * Get the display icon for this key.
 *
 * Return value: (transfer none): The icon, owned by the key.
 */
GIcon*
_gcr_gnupg_key_get_icon (GcrGnupgKey *self)
{
	g_return_val_if_fail (GCR_IS_GNUPG_KEY (self), NULL);

	if (self->pv->icon == NULL) {
		self->pv->icon = _gcr_gnupg_records_get_icon (self->pv->public_records);
		if (self->pv->icon == NULL) {
			if (self->pv->secret_records)
				self->pv->icon = g_themed_icon_new ("gcr-key-pair");
			else
				self->pv->icon = g_themed_icon_new ("gcr-key");
		}
	}

	return self->pv->icon;
}

/**
 * _gcr_gnupg_key_get_columns:
 *
 * Get the columns that we should display for gnupg keys.
 *
 * Returns: (transfer none): The columns, NULL terminated, should not be freed.
 */
const GcrColumn*
_gcr_gnupg_key_get_columns (void)
{
	static GcrColumn columns[] = {
		{ "icon", /* later */ 0, /* later */ 0, NULL, 0, NULL, 0 },
		{ "label", G_TYPE_STRING, G_TYPE_STRING, NC_("column", "Name"),
		  GCR_COLUMN_SORTABLE, NULL, 0 },
		{ "short-keyid", G_TYPE_STRING, G_TYPE_STRING, NC_("column", "Key ID"),
		  GCR_COLUMN_SORTABLE, NULL, 0 },
		{ NULL }
	};

	columns[0].property_type = columns[0].column_type = G_TYPE_ICON;
	return columns;
}
