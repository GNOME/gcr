/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-secure-buffer.c - secure memory gtkentry buffer

   Copyright (C) 2009 Stefan Walter

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stef@memberwebs.com>
*/

#include "config.h"

#include "gcr-secure-entry-buffer.h"

#include "egg/egg-secure-memory.h"

#include <string.h>

/**
 * SECTION:gcr-secure-entry-buffer
 * @title: GcrSecureEntryBuffer
 * @short_description: a GtkEntryBuffer that uses non-pageable memory
 *
 * It's good practice to try to keep passwords or sensitive secrets out of
 * pageable memory whenever possible, so that they don't get written to disk.
 *
 * This is a #GtkEntryBuffer to be used with #GtkEntry which uses non-pageable
 * memory to store a password placed in the entry. In order to make any sense
 * at all, the entry must have it's visibility turned off, and just be displaying
 * place holder characters for the text. That is, a password style entry.
 *
 * Use gtk_entry_new_with_buffer() or gtk_entry_set_buffer() to set this buffer
 * on an entry.
 */

/**
 * GcrSecureEntryBuffer:
 *
 * A #GtkEntryBuffer which uses non-pageable memory for passwords or secrets.
 */

/**
 * GcrSecureEntryBufferClass:
 * @parent_class: parent class
 *
 * The class for #GcrSecureEntryBuffer.
 */

EGG_SECURE_DECLARE (secure_entry_buffer);

/* Initial size of buffer, in bytes */
#define MIN_SIZE 16

struct _GcrSecureEntryBufferPrivate
{
	gchar *text;
	gsize text_size;
	gsize text_bytes;
	guint text_chars;
};

G_DEFINE_TYPE (GcrSecureEntryBuffer, gcr_secure_entry_buffer, GTK_TYPE_ENTRY_BUFFER);

static const gchar *
gcr_secure_entry_buffer_real_get_text (GtkEntryBuffer *buffer,
                                       gsize *n_bytes)
{
	GcrSecureEntryBuffer *self = GCR_SECURE_ENTRY_BUFFER (buffer);
	if (n_bytes)
		*n_bytes = self->pv->text_bytes;
	if (!self->pv->text)
		return "";
	return self->pv->text;
}

static guint
gcr_secure_entry_buffer_real_get_length (GtkEntryBuffer *buffer)
{
	GcrSecureEntryBuffer *self = GCR_SECURE_ENTRY_BUFFER (buffer);
	return self->pv->text_chars;
}

static guint
gcr_secure_entry_buffer_real_insert_text (GtkEntryBuffer *buffer,
                                          guint position,
                                          const gchar *chars,
                                          guint n_chars)
{
	GcrSecureEntryBuffer *self = GCR_SECURE_ENTRY_BUFFER (buffer);
	GcrSecureEntryBufferPrivate *pv = self->pv;
	gsize n_bytes;
	gsize at;

	n_bytes = g_utf8_offset_to_pointer (chars, n_chars) - chars;

	/* Need more memory */
	if (n_bytes + pv->text_bytes + 1 > pv->text_size) {

		/* Calculate our new buffer size */
		while (n_bytes + pv->text_bytes + 1 > pv->text_size) {
			if (pv->text_size == 0) {
				pv->text_size = MIN_SIZE;
			} else {
				if (2 * pv->text_size < GTK_ENTRY_BUFFER_MAX_SIZE) {
					pv->text_size *= 2;
				} else {
					pv->text_size = GTK_ENTRY_BUFFER_MAX_SIZE;
					if (n_bytes > pv->text_size - pv->text_bytes - 1) {
						n_bytes = pv->text_size - pv->text_bytes - 1;
						n_bytes = g_utf8_find_prev_char (chars, chars + n_bytes + 1) - chars;
						n_chars = g_utf8_strlen (chars, n_bytes);
					}
					break;
				}
			}
		}

		pv->text = egg_secure_realloc (pv->text, pv->text_size);
	}

	/* Actual text insertion */
	at = g_utf8_offset_to_pointer (pv->text, position) - pv->text;
	g_memmove (pv->text + at + n_bytes, pv->text + at, pv->text_bytes - at);
	memcpy (pv->text + at, chars, n_bytes);

	/* Book keeping */
	pv->text_bytes += n_bytes;
	pv->text_chars += n_chars;
	pv->text[pv->text_bytes] = '\0';

	gtk_entry_buffer_emit_inserted_text (buffer, position, chars, n_chars);
	return n_chars;
}

static guint
gcr_secure_entry_buffer_real_delete_text (GtkEntryBuffer *buffer,
                                          guint position,
                                          guint n_chars)
{
	GcrSecureEntryBuffer *self = GCR_SECURE_ENTRY_BUFFER (buffer);
	GcrSecureEntryBufferPrivate *pv = self->pv;
	gsize start, end;

	if (position > pv->text_chars)
		position = pv->text_chars;
	if (position + n_chars > pv->text_chars)
		n_chars = pv->text_chars - position;

	if (n_chars > 0) {
		start = g_utf8_offset_to_pointer (pv->text, position) - pv->text;
		end = g_utf8_offset_to_pointer (pv->text, position + n_chars) - pv->text;

		g_memmove (pv->text + start, pv->text + end, pv->text_bytes + 1 - end);
		pv->text_chars -= n_chars;
		pv->text_bytes -= (end - start);

		gtk_entry_buffer_emit_deleted_text (buffer, position, n_chars);
	}

	return n_chars;
}

static void
gcr_secure_entry_buffer_init (GcrSecureEntryBuffer *self)
{
	GcrSecureEntryBufferPrivate *pv;
	pv = self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, GCR_TYPE_SECURE_ENTRY_BUFFER, GcrSecureEntryBufferPrivate);

	pv->text = NULL;
	pv->text_chars = 0;
	pv->text_bytes = 0;
	pv->text_size = 0;
}

static void
gcr_secure_entry_buffer_finalize (GObject *obj)
{
	GcrSecureEntryBuffer *self = GCR_SECURE_ENTRY_BUFFER (obj);
	GcrSecureEntryBufferPrivate *pv = self->pv;

	if (pv->text) {
		egg_secure_strfree (pv->text);
		pv->text = NULL;
		pv->text_bytes = pv->text_size = 0;
		pv->text_chars = 0;
	}

	G_OBJECT_CLASS (gcr_secure_entry_buffer_parent_class)->finalize (obj);
}

static void
gcr_secure_entry_buffer_class_init (GcrSecureEntryBufferClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkEntryBufferClass *buffer_class = GTK_ENTRY_BUFFER_CLASS (klass);

	gobject_class->finalize = gcr_secure_entry_buffer_finalize;

	buffer_class->get_text = gcr_secure_entry_buffer_real_get_text;
	buffer_class->get_length = gcr_secure_entry_buffer_real_get_length;
	buffer_class->insert_text = gcr_secure_entry_buffer_real_insert_text;
	buffer_class->delete_text = gcr_secure_entry_buffer_real_delete_text;

	g_type_class_add_private (gobject_class, sizeof (GcrSecureEntryBufferPrivate));
}

/**
 * gcr_secure_entry_buffer_new:
 *
 * Create a new #GcrSecureEntryBuffer, a #GtkEntryBuffer which uses
 * non-pageable memory for the text.
 *
 * Returns: (transfer full): the new entry buffer
 */
GtkEntryBuffer *
gcr_secure_entry_buffer_new (void)
{
	return g_object_new (GCR_TYPE_SECURE_ENTRY_BUFFER, NULL);
}
