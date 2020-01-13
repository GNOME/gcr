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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_PARSER_H__
#define __GCR_PARSER_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "gcr-types.h"

G_BEGIN_DECLS

#define GCR_TYPE_PARSER               (gcr_parser_get_type ())
#define GCR_PARSER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_PARSER, GcrParser))
#define GCR_PARSER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_PARSER, GcrParserClass))
#define GCR_IS_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_PARSER))
#define GCR_IS_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_PARSER))
#define GCR_PARSER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_PARSER, GcrParserClass))

typedef struct _GcrParser GcrParser;
typedef struct _GcrParserClass GcrParserClass;
typedef struct _GcrParserPrivate GcrParserPrivate;
typedef struct _GcrParsed GcrParsed;

struct _GcrParser {
	GObject parent;

	/*< private >*/
	GcrParserPrivate *pv;
};

struct _GcrParserClass {
	GObjectClass parent_class;

	/* signals --------------------------------------------------------- */

	/* A callback for each password needed */
	gboolean (*authenticate) (GcrParser *self, gint count);

	void     (*parsed) (GcrParser *self);
};

GType                    gcr_parser_get_type               (void);

GcrParser*               gcr_parser_new                    (void);

void                     gcr_parser_format_enable          (GcrParser *self,
                                                            GcrDataFormat format);

void                     gcr_parser_format_disable         (GcrParser *self,
                                                            GcrDataFormat format);

gboolean                 gcr_parser_format_supported       (GcrParser *self,
                                                            GcrDataFormat format);

gboolean                 gcr_parser_parse_bytes            (GcrParser *self,
                                                            GBytes *data,
                                                            GError **error);

gboolean                 gcr_parser_parse_data             (GcrParser *self,
                                                            const guchar *data,
                                                            gsize n_data,
                                                            GError **error);

gboolean                 gcr_parser_parse_stream           (GcrParser *self,
                                                            GInputStream *input,
                                                            GCancellable *cancellable,
                                                            GError **error);

void                     gcr_parser_parse_stream_async     (GcrParser *self,
                                                            GInputStream *input,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gboolean                 gcr_parser_parse_stream_finish    (GcrParser *self,
                                                            GAsyncResult *result,
                                                            GError **error);

void                     gcr_parser_add_password           (GcrParser *self,
                                                            const gchar *password);

GcrParsed *              gcr_parser_get_parsed             (GcrParser *self);

const gchar*             gcr_parser_get_parsed_label       (GcrParser *self);

const gchar*             gcr_parser_get_parsed_description (GcrParser *self);

GckAttributes*           gcr_parser_get_parsed_attributes  (GcrParser *self);

const guchar *           gcr_parser_get_parsed_block       (GcrParser *self,
                                                            gsize *n_block);

GBytes *                 gcr_parser_get_parsed_bytes       (GcrParser *self);

GcrDataFormat            gcr_parser_get_parsed_format      (GcrParser *self);

const gchar*             gcr_parser_get_filename           (GcrParser *self);

void                     gcr_parser_set_filename           (GcrParser *self,
		                                            const gchar *filename);

#define                  GCR_TYPE_PARSED                   (gcr_parsed_get_type ())

GType                    gcr_parsed_get_type               (void) G_GNUC_CONST;

GcrParsed *              gcr_parsed_ref                    (GcrParsed *parsed);

void                     gcr_parsed_unref                  (gpointer parsed);

const gchar*             gcr_parsed_get_label              (GcrParsed *parsed);

const gchar*             gcr_parsed_get_description        (GcrParsed *parsed);

GckAttributes*           gcr_parsed_get_attributes         (GcrParsed *parsed);

const guchar *           gcr_parsed_get_data               (GcrParsed *parsed,
                                                            gsize *n_data);

GBytes *                 gcr_parsed_get_bytes              (GcrParsed *parsed);

GcrDataFormat            gcr_parsed_get_format             (GcrParsed *parsed);

const gchar*             gcr_parsed_get_filename           (GcrParsed *parsed);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GcrParsed, g_object_unref)

G_END_DECLS

#endif /* __GCR_PARSER_H__ */
