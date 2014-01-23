/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-secure-buffer.h - secure memory gtkentry buffer

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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_SECURE_ENTRY_BUFFER_H__
#define __GCR_SECURE_ENTRY_BUFFER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCR_TYPE_SECURE_ENTRY_BUFFER            (gcr_secure_entry_buffer_get_type ())
#define GCR_SECURE_ENTRY_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_SECURE_ENTRY_BUFFER, GcrSecureEntryBuffer))
#define GCR_SECURE_ENTRY_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_SECURE_ENTRY_BUFFER, GcrSecureEntryBufferClass))
#define GCR_IS_SECURE_ENTRY_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_SECURE_ENTRY_BUFFER))
#define GCR_IS_SECURE_ENTRY_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_SECURE_ENTRY_BUFFER))
#define GCR_SECURE_ENTRY_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_SECURE_ENTRY_BUFFER, GcrSecureEntryBufferClass))

typedef struct _GcrSecureEntryBuffer            GcrSecureEntryBuffer;
typedef struct _GcrSecureEntryBufferClass       GcrSecureEntryBufferClass;
typedef struct _GcrSecureEntryBufferPrivate     GcrSecureEntryBufferPrivate;

struct _GcrSecureEntryBuffer {
	GtkEntryBuffer parent;

	/*< private >*/
	GcrSecureEntryBufferPrivate *pv;
};

struct _GcrSecureEntryBufferClass
{
	GtkEntryBufferClass parent_class;
};

GType                     gcr_secure_entry_buffer_get_type               (void) G_GNUC_CONST;

GtkEntryBuffer *          gcr_secure_entry_buffer_new                    (void);

G_END_DECLS

#endif /* __GCR_SECURE_ENTRY_BUFFER_H__ */
