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

#include "gcr-gnupg-records.h"
#include "gcr-record.h"

#include "gck/gck.h"

#include <glib/gi18n-lib.h>

/* Copied from GPGME */
gboolean
_gcr_gnupg_records_parse_user_id (const gchar *user_id,
                                  gchar **rname,
                                  gchar **remail,
                                  gchar **rcomment)
{
	gchar *src, *tail, *x;
	int in_name = 0;
	int in_email = 0;
	int in_comment = 0;
	gboolean anything;
	const gchar *name = NULL;
	const gchar *email = NULL;
	const gchar *comment = NULL;

	x = tail = src = g_strdup (user_id);

	while (*src) {
		if (in_email) {
			/* Not legal but anyway.  */
			if (*src == '<')
				in_email++;
			else if (*src == '>') {
				if (!--in_email && !email) {
					email = tail;
					*src = 0;
					tail = src + 1;
				}
			}
		} else if (in_comment) {
			if (*src == '(')
				in_comment++;
			else if (*src == ')') {
				if (!--in_comment && !comment) {
					comment = tail;
					*src = 0;
					tail = src + 1;
				}
			}
		} else if (*src == '<') {
			if (in_name) {
				if (!name) {
					name = tail;
					*src = 0;
					tail = src + 1;
				}
				in_name = 0;
			} else
				tail = src + 1;

			in_email = 1;
		} else if (*src == '(') {
			if (in_name) {
				if (!name) {
					name = tail;
					*src = 0;
					tail = src + 1;
				}
				in_name = 0;
			}
			in_comment = 1;
		} else if (!in_name && *src != ' ' && *src != '\t') {
			in_name = 1;
		}
		src++;
	}

	if (in_name) {
		if (!name) {
			name = tail;
			*src = 0;
		}
	}

	anything = FALSE;

	if (rname) {
		*rname = g_strdup (name);
		if (name) {
			g_strstrip (*rname);
			anything = TRUE;
		}
	}

	if (remail) {
		*remail = g_strdup (email);
		if (email) {
			g_strstrip (*remail);
			anything = TRUE;
		}
	}

	if (rcomment) {
		*rcomment = g_strdup (comment);
		if (comment) {
			g_strstrip (*rcomment);
			anything = TRUE;
		}
	}

	g_free (x);
	return anything;
}

const gchar *
_gcr_gnupg_records_get_keyid (GPtrArray *records)
{
	GcrRecord *record;

	record = _gcr_records_find (records, GCR_RECORD_SCHEMA_PUB);
	if (record != NULL)
		return _gcr_record_get_raw (record, GCR_RECORD_KEY_KEYID);
	record = _gcr_records_find (records, GCR_RECORD_SCHEMA_SEC);
	if (record != NULL)
		return _gcr_record_get_raw (record, GCR_RECORD_KEY_KEYID);
	return NULL;
}

const gchar *
_gcr_gnupg_records_get_short_keyid (GPtrArray *records)
{
	const gchar *keyid;
	gsize length;

	keyid = _gcr_gnupg_records_get_keyid (records);
	if (keyid == NULL)
		return NULL;

	length = strlen (keyid);
	if (length > 8)
		keyid += (length - 8);

	return keyid;
}

gchar *
_gcr_gnupg_records_get_user_id (GPtrArray *records)
{
	GcrRecord *record;

	record = _gcr_records_find (records, GCR_RECORD_SCHEMA_UID);
	if (record != NULL)
		return _gcr_record_get_string (record, GCR_RECORD_UID_USERID);
	return NULL;
}

const gchar *
_gcr_gnupg_records_get_fingerprint (GPtrArray *records)
{
	GcrRecord *record;

	record = _gcr_records_find (records, GCR_RECORD_SCHEMA_FPR);
	if (record != NULL)
		return _gcr_record_get_raw (record, GCR_RECORD_FPR_FINGERPRINT);
	return NULL;
}
