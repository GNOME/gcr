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

#include "gcr/gcr.h"
#include "gcr/gcr-openpgp.h"
#include "gcr/gcr-record.h"

#include "egg/egg-armor.h"

static void
on_packet_print_records (GPtrArray *records,
                         GBytes *packet,
                         gpointer user_data)
{
	gchar *string;
	guint i;

	for (i = 0; i < records->len; i++) {
		string = _gcr_record_format (records->pdata[i]);
		g_print ("%s\n", string);
		g_free (string);
	}
}

static gboolean
parse_binary (GBytes *contents)
{
	guint packets;

	packets = _gcr_openpgp_parse (contents,
	                              GCR_OPENPGP_PARSE_KEYS |
	                              GCR_OPENPGP_PARSE_ATTRIBUTES,
	                              on_packet_print_records, NULL);

	return (packets > 0);
}

static void
on_armor_parsed (GQuark type,
                 GBytes *data,
                 GBytes *outer,
                 GHashTable *headers,
                 gpointer user_data)
{
	const gchar *value;
	gboolean *result = user_data;

	value = g_hash_table_lookup (headers, "Version");
	g_assert_cmpstr (value, ==, "GnuPG v1.4.11 (GNU/Linux)");

	*result = parse_binary (data);
}

static gboolean
parse_armor_or_binary (GBytes *contents)
{
	gboolean result;
	guint parts;

	parts = egg_armor_parse (contents, on_armor_parsed, &result);
	if (parts == 0)
		result = parse_binary (contents);
	return result;
}

int
main(int argc, char *argv[])
{
	GError *error = NULL;
	gchar *contents;
	gsize length;
	GBytes *bytes;
	int ret;

	g_set_prgname ("frob-openpgp");

	if (argc != 2) {
		g_printerr ("usage: frob-openpgp filename\n");
		return 2;
	}

	if (!g_file_get_contents (argv[1], &contents, &length, &error)) {
		g_printerr ("frob-openpgp: couldn't read file: %s: %s", argv[1], error->message);
		g_error_free (error);
		return 1;
	}

	bytes = g_bytes_new_take (contents, length);
	ret = 0;
	if (!parse_armor_or_binary (bytes)) {
		g_printerr ("frob-openpgp: no openpgp data found in data");
		ret = 1;
	}

	g_bytes_unref (bytes);
	return ret;
}
