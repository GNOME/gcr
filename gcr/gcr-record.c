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

#include "gcr-record.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_COLUMNS 32

typedef struct {
	gpointer next;
	gsize n_value;
	gchar value[1];
	/* Hangs off the end */
} GcrRecordBlock;

struct _GcrRecord {
	GcrRecordBlock *block;
	const gchar *columns[MAX_COLUMNS];
	guint n_columns;
	gchar delimiter;
};

G_DEFINE_BOXED_TYPE (GcrRecord, _gcr_record, _gcr_record_copy, _gcr_record_free);

static GcrRecordBlock *
record_block_new (const gchar *value,
                  gsize length)
{
	GcrRecordBlock *block;

	block = g_malloc (sizeof (GcrRecordBlock) + length);
	block->next = NULL;
	block->n_value = length;

	if (value != NULL) {
		memcpy (block->value, value, length);
		block->value[length] = 0;
	} else {
		block->value[0] = 0;
	}

	return block;
}

static GcrRecordBlock *
record_block_take (gchar *value,
                   gsize length)
{
	GcrRecordBlock *block;

	g_assert (value);

	block = g_realloc (value, sizeof (GcrRecordBlock) + length);
	memmove (((gchar*)block) + G_STRUCT_OFFSET (GcrRecordBlock, value),
	         block, length);
	block->next = NULL;
	block->n_value = length;
	block->value[length] = 0;

	return block;
}

static GcrRecord *
record_flatten (GcrRecord *record)
{
	GcrRecord *result;
	GcrRecordBlock *block;
	gsize total;
	gsize at;
	gsize len;
	guint i;

	/* Calculate the length of what we need */
	total = 0;
	for (i = 0; i < record->n_columns; i++)
		total += strlen (record->columns[i]) + 1;

	/* Allocate a new GcrRecordData which will hold all that */
	result = g_new0 (GcrRecord, 1);
	result->block = block = record_block_new (NULL, total);

	at = 0;
	for (i = 0; i < record->n_columns; i++) {
		len = strlen (record->columns[i]);
		result->columns[i] = block->value + at;
		memcpy ((gchar *)result->columns[i], record->columns[i], len + 1);
		at += len + 1;
	}

	result->n_columns = record->n_columns;
	result->delimiter = record->delimiter;
	g_assert (at == total);

	return result;
}

static void
print_record_to_string (GcrRecord *record,
                        GString *string)
{
	guint i;

	for (i = 0; i < record->n_columns; i++) {
		g_string_append (string, record->columns[i]);
		g_string_append_c (string, record->delimiter);
	}
}

gchar *
_gcr_record_format (GcrRecord *record)
{
	GString *string;

	g_return_val_if_fail (record, NULL);

	string = g_string_new ("");
	print_record_to_string (record, string);
	return g_string_free (string, FALSE);
}

GcrRecord *
_gcr_record_new (GQuark schema,
                 guint n_columns,
                 gchar delimiter)
{
	GcrRecord *result;
	guint i;

	result = g_new0 (GcrRecord, 1);
	result->block = NULL;
	result->delimiter = delimiter;

	for (i = 0; i < n_columns; i++)
		result->columns[i] = "";
	result->columns[0] = g_quark_to_string (schema);
	result->n_columns = n_columns;

	return result;
}

GcrRecord *
_gcr_record_copy (GcrRecord *record)
{
	return record_flatten (record);
}

static GcrRecord *
take_and_parse_internal (GcrRecordBlock *block,
                         gchar delimiter,
                         gboolean allow_empty)
{
	GcrRecord *result;
	gchar *at, *beg, *end;

	g_assert (block);

	result = g_new0 (GcrRecord, 1);
	result->block = block;
	result->delimiter = delimiter;

	g_debug ("parsing line %s", block->value);

	at = block->value;
	for (;;) {
		if (result->n_columns >= MAX_COLUMNS) {
			g_debug ("too many record (%d) in gnupg line", MAX_COLUMNS);
			_gcr_record_free (result);
			return NULL;
		}

		beg = at;
		result->columns[result->n_columns] = beg;

		at = strchr (beg, delimiter);
		if (at == NULL) {
			end = (block->value + block->n_value) - 1;
		} else {
			at[0] = '\0';
			end = at;
			at++;
		}

		if (allow_empty || end > beg)
			result->n_columns++;

		if (at == NULL)
			break;
	}

	return result;
}

GcrRecord*
_gcr_record_parse_colons (const gchar *line, gssize n_line)
{
	g_return_val_if_fail (line, NULL);
	if (n_line < 0)
		n_line = strlen (line);
	return take_and_parse_internal (record_block_new (line, n_line), ':', TRUE);
}

GcrRecord*
_gcr_record_parse_spaces (const gchar *line, gssize n_line)
{
	g_return_val_if_fail (line, NULL);
	if (n_line < 0)
		n_line = strlen (line);
	return take_and_parse_internal (record_block_new (line, n_line), ' ', FALSE);
}

guint
_gcr_record_get_count (GcrRecord *record)
{
	g_return_val_if_fail (record, 0);
	return record->n_columns;
}

static void
record_take_column (GcrRecord *record,
                    guint column,
                    GcrRecordBlock *block)
{
	g_assert (block->next == NULL);
	block->next = record->block;
	record->block = block;

	g_assert (column < record->n_columns);
	record->columns[column] = block->value;
}

static const char HEXC_LOWER[] = "0123456789abcdef";

/* Will return NULL if unescaping failed or not needed */
static gchar *
c_colons_unescape (const gchar *source,
                   gsize *length)
{
	const gchar *p = source, *octal, *hex;
	gchar *dest = NULL;
	gchar *q = dest;
	gchar *pos;

	while (*p) {
		if (*p == '\\') {
			if (dest == NULL) {
				dest = 	g_malloc (strlen (source) + 1);
				memcpy (dest, source, (p - source));
				q = dest + (p - source);
			}

			p++;
			switch (*p) {
			case '\0': /* invalid trailing backslash */
				g_free (dest);
				return NULL;
			case '0':  case '1':  case '2':  case '3':  case '4':
			case '5':  case '6':  case '7':
				*q = 0;
				octal = p;
				while ((p < octal + 3) && (*p >= '0') && (*p <= '7')) {
					*q = (*q * 8) + (*p - '0');
					p++;
				}
				q++;
				p--;
				break;
			case 'x':
				*q = 0;
				hex = p;
				while (p < hex + 2) {
					pos = strchr (HEXC_LOWER, g_ascii_tolower (*p));
					if (pos == 0) { /* invalid bad hex character */
						g_free (dest);
						return NULL;
					}
					*q = (*q * 16) + (pos - HEXC_LOWER);
					p++;
				}
				q++;
				p--;
				break;
			case 'b':
				*q++ = '\b';
				break;
			case 'f':
				*q++ = '\f';
				break;
			case 'n':
				*q++ = '\n';
				break;
			case 'r':
				*q++ = '\r';
				break;
			case 't':
				*q++ = '\t';
				break;
			default:            /* Also handles \" and \\ */
				*q++ = *p;
				break;
			}
		} else if (q != NULL) {
			*q++ = *p;
		}
		p++;
	}

	if (q != NULL) {
		*q = 0;
		if (length)
			*length = q - dest;
	}

	return dest;
}

/* Will return NULL if no escaping needed */
static gchar *
c_colons_escape (const gchar *source,
                 const gchar extra,
                 gsize *length)
{
	const guchar *p;
	gchar *dest = NULL;
	gchar *q = NULL;
	gchar escape;
	gsize off;

	g_return_val_if_fail (source != NULL, NULL);

	p = (guchar *) source;

	while (*p) {
		escape = 0;
		switch (*p) {
		case '\b':
			escape = 'b';
			break;
		case '\f':
			escape = 'f';
			break;
		case '\n':
			escape = 'n';
			break;
		case '\r':
			escape = 'r';
			break;
		case '\t':
			escape = 't';
			break;
		case '\\':
			escape = '\\';
			break;
		case '"':
			escape = '"';
			break;
		}

		if (escape != 0 || *p < ' ' || *p >= 0x127 || *p == extra) {
			if (dest == NULL) {
				/* Each source byte needs maximally four destination chars (\xff) */
				dest = g_malloc (strlen (source) * 4 + 1);
				off = (gchar *)p - source;
				memcpy (dest, source, off);
				q = dest + off;
			}

			if (escape) {
				*q++ = '\\';
				*q++ = escape;
			} else {
				*q++ = '\\';
				*q++ = 'x';
				*q++ = HEXC_LOWER[*p >> 4 & 0xf];
				*q++ = HEXC_LOWER[*p & 0xf];
			}
		} else if (q != NULL) {
			*q++ = *p;
		}
		p++;
	}

	if (q != NULL) {
		*q = 0;
		if (length)
			*length = q - dest;
	}

	return dest;
}

gchar*
_gcr_record_get_string (GcrRecord *record, guint column)
{
	const gchar *value;
	gchar *text = NULL;

	g_return_val_if_fail (record, NULL);

	value = _gcr_record_get_raw (record, column);
	if (!value)
		return NULL;

	text = c_colons_unescape (value, NULL);
	if (text != NULL)
		value = text;

	/* If it's not UTF-8, we guess that it's latin1 */
	if (!g_utf8_validate (value, -1, NULL)) {
		gchar *conv = g_convert (value, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
		g_free (text);
		value = text = conv;
	}

	/*
	 * latin1 to utf-8 conversion can't really fail, just produce
	 * garbage... so there's no need to check here.
	 */

	return (text == value) ? text : g_strdup (value);
}

void
_gcr_record_set_string (GcrRecord *record,
                        guint column,
                        const gchar *string)
{
	GcrRecordBlock *block;
	gchar *escaped;

	g_return_if_fail (record != NULL);
	g_return_if_fail (string != NULL);
	g_return_if_fail (column < record->n_columns);

	escaped = c_colons_escape (string, record->delimiter, NULL);
	if (escaped != NULL)
		block = record_block_take (escaped, strlen (escaped));
	else
		block = record_block_new (string, strlen (string));

	record_take_column (record, column, block);
}

gchar
_gcr_record_get_char (GcrRecord *record,
                      guint column)
{
	const gchar *value;

	g_return_val_if_fail (record, 0);

	value = _gcr_record_get_raw (record, column);
	if (!value)
		return 0;

	if (value[0] != 0 && value[1] == 0)
		return value[0];

	return 0;
}

void
_gcr_record_set_char (GcrRecord *record,
                      guint column,
                      gchar value)
{
	g_return_if_fail (record != NULL);
	g_return_if_fail (column < record->n_columns);
	g_return_if_fail (value != 0);

	record_take_column (record, column, record_block_new (&value, 1));
}

gboolean
_gcr_record_get_uint (GcrRecord *record, guint column, guint *value)
{
	const gchar *raw;
	gint64 result;
	gchar *end = NULL;

	g_return_val_if_fail (record, FALSE);

	raw = _gcr_record_get_raw (record, column);
	if (raw == NULL)
		return FALSE;

	result = g_ascii_strtoll (raw, &end, 10);
	if (!end || end[0]) {
		g_debug ("invalid unsigned integer value: %s", raw);
		return FALSE;
	}

	if (result < 0 || result > G_MAXUINT32) {
		g_debug ("unsigned integer value is out of range: %s", raw);
		return FALSE;
	}

	if (value)
		*value = (guint)result;
	return TRUE;
}

void
_gcr_record_set_uint (GcrRecord *record,
                      guint column,
                      guint value)
{
	gchar *escaped;

	g_return_if_fail (record != NULL);
	g_return_if_fail (column < record->n_columns);

	escaped = g_strdup_printf ("%u", value);
	record_take_column (record, column,
	                    record_block_take (escaped, strlen (escaped)));
}

gboolean
_gcr_record_get_ulong (GcrRecord *record,
                       guint column,
                       gulong *value)
{
	const gchar *raw;
	gint64 result;
	gchar *end = NULL;

	g_return_val_if_fail (record, FALSE);

	raw = _gcr_record_get_raw (record, column);
	if (raw == NULL)
		return FALSE;

	result = g_ascii_strtoull (raw, &end, 10);
	if (!end || end[0]) {
		g_debug ("invalid unsigned long value: %s", raw);
		return FALSE;
	}

	if (result < 0 || result > G_MAXULONG) {
		g_debug ("unsigned long value is out of range: %s", raw);
		return FALSE;
	}

	if (value)
		*value = (guint)result;
	return TRUE;

}

void
_gcr_record_set_ulong (GcrRecord *record,
                       guint column,
                       gulong value)
{
	gchar *escaped;

	g_return_if_fail (record != NULL);
	g_return_if_fail (column < record->n_columns);

	escaped = g_strdup_printf ("%lu", value);
	record_take_column (record, column,
	                    record_block_take (escaped, strlen (escaped)));
}

GDateTime *
_gcr_record_get_date (GcrRecord *record,
                      guint column)
{
	const char *raw;
	guint64 result;
	char *end = NULL;
	GTimeZone *tz;
	GDateTime *ret;

	g_return_val_if_fail (record, NULL);

	raw = _gcr_record_get_raw (record, column);
	if (raw == NULL)
		return NULL;

	/* Try to parse as a number */
	result = g_ascii_strtoull (raw, &end, 10);
	if (end != NULL && end[0] == '\0') {
		if (result == 0)
			return NULL;
		else
			return g_date_time_new_from_unix_utc (result);
	}

	/* Try to parse as a date */
	tz = g_time_zone_new_utc ();
	ret = g_date_time_new_from_iso8601 (raw, tz);
	g_time_zone_unref (tz);

	return ret;
}

/**
 * _gcr_record_get_base64:
 * @record: The record
 * @column: The column to decode.
 * @n_data: Location to return size of returned data.
 *
 * Decode a column of a record as base64 data.
 *
 * Returns: (transfer full): The decoded value, or %NULL if not found.
 */
gpointer
_gcr_record_get_base64 (GcrRecord *record, guint column, gsize *n_data)
{
	const gchar *raw;

	g_return_val_if_fail (record, NULL);

	raw = _gcr_record_get_raw (record, column);
	if (raw == NULL)
		return NULL;

	return g_base64_decode (raw, n_data);
}

void
_gcr_record_set_base64 (GcrRecord *record,
                        guint column,
                        gconstpointer data,
                        gsize n_data)
{
	GcrRecordBlock *block;
	gint state, save;
	gsize estimate;
	gsize length;

	g_return_if_fail (record != NULL);
	g_return_if_fail (column < record->n_columns);

	estimate = n_data * 4 / 3 + n_data * 4 / (3 * 65) + 7;
	block = record_block_new (NULL, estimate);

	/* The actual base64 data, without line breaks */
	state = save = 0;
	length = g_base64_encode_step ((guchar *)data, n_data, FALSE,
	                               block->value, &state, &save);
	length += g_base64_encode_close (TRUE, block->value + length,
	                                 &state, &save);
	block->value[length] = 0;
	g_assert (length < estimate);

	g_strchomp (block->value);
	record_take_column (record, column, block);
}

const gchar*
_gcr_record_get_raw (GcrRecord *record, guint column)
{
	g_return_val_if_fail (record, NULL);

	if (column >= record->n_columns) {
		g_debug ("only %d columns exist, tried to access %d",
		         record->n_columns, column);
		return NULL;
	}

	return record->columns[column];
}

void
_gcr_record_set_raw (GcrRecord *record,
                     guint column,
                     const gchar *value)
{
	g_return_if_fail (record != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (column < record->n_columns);

	record_take_column (record, column,
	                    record_block_new (value, strlen (value)));
}

void
_gcr_record_take_raw (GcrRecord *record,
                      guint column,
                      gchar *value)
{
	g_return_if_fail (record != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (column < record->n_columns);

	record_take_column (record, column,
	                    record_block_take (value, strlen (value)));
}

void
_gcr_record_free (gpointer record)
{
	GcrRecordBlock *block, *next;
	GcrRecord *rec = record;

	if (!record)
		return;

	for (block = rec->block; block != NULL; block = next) {
		next = block->next;
		g_free (block);
	}

	g_free (record);
}

GQuark
_gcr_record_get_schema (GcrRecord *record)
{
	const gchar *value;

	value = _gcr_record_get_raw (record, GCR_RECORD_SCHEMA);
	if (value != NULL)
		return g_quark_try_string (value);
	return 0;
}

GcrRecord *
_gcr_records_find (GPtrArray *records,
                   GQuark schema)
{
	guint i;

	g_return_val_if_fail (records, NULL);
	g_return_val_if_fail (schema, NULL);

	for (i = 0; i < records->len; i++) {
		if (schema == _gcr_record_get_schema (records->pdata[i]))
			return records->pdata[i];
	}

	return NULL;
}

gchar *
_gcr_records_format (GPtrArray *records)
{
	GString *string;
	guint i;

	g_return_val_if_fail (records, NULL);

	string = g_string_new ("");
	for (i = 0; i < records->len; i++) {
		print_record_to_string (records->pdata[i], string);
		g_string_append_c (string, '\n');
	}
	return g_string_free (string, FALSE);
}

static gchar **
strnsplit (const gchar *string,
           gsize length,
           gchar delimiter)
{
  GSList *string_list = NULL, *slist;
  gchar **str_array, *s;
  guint n = 0;
  const gchar *remainder;
  const gchar *end;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (delimiter != '\0', NULL);

  end = string + length;
  remainder = string;
  s = memchr (remainder, delimiter, end - remainder);
  if (s)
    {
      while (s)
        {
          gsize len;

          len = s - remainder;
          string_list = g_slist_prepend (string_list,
                                         g_strndup (remainder, len));
          n++;
          remainder = s + 1;
          s = memchr (remainder, delimiter, end - remainder);
        }
    }
  if (*string)
    {
      n++;
      string_list = g_slist_prepend (string_list, g_strndup (remainder, end - remainder));
    }

  str_array = g_new (gchar*, n + 1);

  str_array[n--] = NULL;
  for (slist = string_list; slist; slist = slist->next)
    str_array[n--] = slist->data;

  g_slist_free (string_list);

  return str_array;
}

GPtrArray *
_gcr_records_parse_colons (gconstpointer data,
                           gssize n_data)
{
	GPtrArray *result = NULL;
	GcrRecordBlock *block;
	GcrRecord *record;
	gchar **lines;
	guint i;

	lines = strnsplit (data, n_data, '\n');
	result = g_ptr_array_new_with_free_func (_gcr_record_free);

	for (i = 0; lines[i] != NULL; i++) {
		block = record_block_take (lines[i], strlen (lines[i]));
		record = take_and_parse_internal (block, ':', TRUE);
		if (record == NULL) {
			g_ptr_array_unref (result);
			result = NULL;
			break;
		}
		g_ptr_array_add (result, record);
	}

	/* Free any not done */
	for (; lines[i] != NULL; i++)
		g_free (lines[i]);

	/* Individual lines already freed */
	g_free (lines);

	return result;
}
