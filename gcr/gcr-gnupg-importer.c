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

#include "gcr-gnupg-importer.h"
#include "gcr-gnupg-process.h"
#include "gcr-internal.h"

#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_LABEL,
	PROP_IMPORTED,
	PROP_DIRECTORY,
	PROP_INTERACTION,
	PROP_URI
};

struct _GcrGnupgImporterPrivate {
	GcrGnupgProcess *process;
	GMemoryInputStream *packets;
	GTlsInteraction *interaction;
	gchar *first_error;
	GArray *imported;
};

static void gcr_gnupg_importer_iface (GcrImporterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrGnupgImporter, _gcr_gnupg_importer, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GcrGnupgImporter);
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_IMPORTER, gcr_gnupg_importer_iface);
);

static void
_gcr_gnupg_importer_init (GcrGnupgImporter *self)
{
	self->pv = _gcr_gnupg_importer_get_instance_private (self);
	self->pv->packets = G_MEMORY_INPUT_STREAM (g_memory_input_stream_new ());
	self->pv->imported = g_array_new (TRUE, TRUE, sizeof (gchar *));
}

static void
_gcr_gnupg_importer_dispose (GObject *obj)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (obj);

	if (self->pv->process)
		g_object_run_dispose (G_OBJECT (self->pv->process));
	g_clear_object (&self->pv->process);
	g_clear_object (&self->pv->packets);
	g_clear_object (&self->pv->interaction);

	G_OBJECT_CLASS (_gcr_gnupg_importer_parent_class)->dispose (obj);
}

static void
_gcr_gnupg_importer_finalize (GObject *obj)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (obj);

	g_array_free (self->pv->imported, TRUE);
	g_free (self->pv->first_error);

	G_OBJECT_CLASS (_gcr_gnupg_importer_parent_class)->finalize (obj);
}

static gchar *
calculate_label (GcrGnupgImporter *self)
{
	const gchar *directory;

	directory = _gcr_gnupg_process_get_directory (self->pv->process);
	if (directory == NULL)
		return g_strdup (_("GnuPG Keyring"));
	else
		return g_strdup_printf (_("GnuPG Keyring: %s"), directory);
}

static gchar *
calculate_uri (GcrGnupgImporter *self)
{
	const gchar *directory;

	directory = _gcr_gnupg_process_get_directory (self->pv->process);
	if (directory == NULL)
		return g_strdup ("gnupg://");
	else
		return g_strdup_printf ("gnupg://%s", directory);
}

static gboolean
on_process_error_line (GcrGnupgProcess *process,
                       const gchar *line,
                       gpointer user_data)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (user_data);

	if (self->pv->first_error)
		return TRUE;

	if (g_str_has_prefix (line, "gpg: ")) {
		line += 5;
		if (g_pattern_match_simple ("key ????????:*", line))
			line += 13;
	}

	while (line[0] && g_ascii_isspace (line[0]))
		line++;

	self->pv->first_error = g_strdup (line);
	g_strstrip (self->pv->first_error);
	return TRUE;
}

static gboolean
on_process_status_record (GcrGnupgProcess *process,
                          GcrRecord *record,
                          gpointer user_data)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (user_data);
	const gchar *value;
	gchar *fingerprint;

	if (_gcr_record_get_schema (record) != GCR_RECORD_SCHEMA_IMPORT_OK)
		return TRUE;

	value = _gcr_record_get_raw (record, GCR_RECORD_IMPORT_FINGERPRINT);
	if (value != NULL && value[0] != 0) {
		fingerprint = g_strdup (value);
		g_array_append_val (self->pv->imported, fingerprint);
	}

	return TRUE;
}

static void
_gcr_gnupg_importer_set_property (GObject *obj,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (obj);

	switch (prop_id) {
	case PROP_DIRECTORY:
		self->pv->process = _gcr_gnupg_process_new (g_value_get_string (value), NULL);
		_gcr_gnupg_process_set_input_stream (self->pv->process, G_INPUT_STREAM (self->pv->packets));
		g_signal_connect (self->pv->process, "error-line", G_CALLBACK (on_process_error_line), self);
		g_signal_connect (self->pv->process, "status-record", G_CALLBACK (on_process_status_record), self);
		break;
	case PROP_INTERACTION:
		g_clear_object (&self->pv->interaction);
		self->pv->interaction = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
_gcr_gnupg_importer_get_property (GObject *obj,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_take_string (value, calculate_label (self));
		break;
	case PROP_IMPORTED:
		g_value_set_boxed (value, _gcr_gnupg_importer_get_imported (self));
		break;
	case PROP_DIRECTORY:
		g_value_set_string (value, _gcr_gnupg_process_get_directory (self->pv->process));
		break;
	case PROP_INTERACTION:
		g_value_set_object (value, self->pv->interaction);
		break;
	case PROP_URI:
		g_value_take_string (value, calculate_uri (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
_gcr_gnupg_importer_class_init (GcrGnupgImporterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GckBuilder builder = GCK_BUILDER_INIT;

	gobject_class->dispose = _gcr_gnupg_importer_dispose;
	gobject_class->finalize = _gcr_gnupg_importer_finalize;
	gobject_class->set_property = _gcr_gnupg_importer_set_property;
	gobject_class->get_property = _gcr_gnupg_importer_get_property;

	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_INTERACTION, "interaction");
	g_object_class_override_property (gobject_class, PROP_URI, "uri");

	g_object_class_install_property (gobject_class, PROP_IMPORTED,
		g_param_spec_boxed ("imported", "Imported", "Fingerprints of imported keys",
		                    G_TYPE_STRV,
		                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_DIRECTORY,
		g_param_spec_string ("directory", "Directory", "Directory to import keys to",
		                     NULL,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_GCR_GNUPG_RECORDS);
	gcr_importer_register (GCR_TYPE_GNUPG_IMPORTER, gck_builder_end (&builder));

	_gcr_initialize_library ();
}

static GList *
_gcr_gnupg_importer_create_for_parsed (GcrParsed *parsed)
{
	GcrImporter *self;

	if (gcr_parsed_get_format (parsed) != GCR_FORMAT_OPENPGP_PACKET)
		return NULL;

	self = _gcr_gnupg_importer_new (NULL);
	if (!gcr_importer_queue_for_parsed (self, parsed))
		g_assert_not_reached ();

	return g_list_append (NULL, self);
}

static gboolean
_gcr_gnupg_importer_queue_for_parsed (GcrImporter *importer,
                                      GcrParsed *parsed)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (importer);
	gconstpointer block;
	gsize n_block;

	if (gcr_parsed_get_format (parsed) != GCR_FORMAT_OPENPGP_PACKET)
		return FALSE;

	block = gcr_parsed_get_data (parsed, &n_block);
	g_return_val_if_fail (block, FALSE);

	g_memory_input_stream_add_data (self->pv->packets, g_memdup2 (block, n_block),
	                                n_block, g_free);
	return TRUE;
}

static void
on_process_run_complete (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (g_task_get_source_object (task));
	GError *error = NULL;

	if (!_gcr_gnupg_process_run_finish (GCR_GNUPG_PROCESS (source), result, &error)) {
		if (g_error_matches (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED) && self->pv->first_error) {
			g_task_return_new_error (task, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
			                         "%s", self->pv->first_error);
			g_error_free (error);
		} else {
			g_task_return_error (task, g_steal_pointer (&error));
		}
	} else {
		g_task_return_boolean (task, TRUE);
	}

	g_clear_object (&task);
}

static void
_gcr_gnupg_importer_import_async (GcrImporter *importer,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GcrGnupgImporter *self = GCR_GNUPG_IMPORTER (importer);
	GTask *task;
	const gchar *argv[] = { "--import", NULL };

	g_clear_pointer (&self->pv->first_error, g_free);

	task = g_task_new (importer, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gcr_gnupg_importer_import_async);

	_gcr_gnupg_process_run_async (self->pv->process, argv, NULL,
	                              GCR_GNUPG_PROCESS_WITH_STATUS,
	                              cancellable, on_process_run_complete,
	                              g_steal_pointer (&task));

	g_clear_object (&task);
}

static gboolean
_gcr_gnupg_importer_import_finish (GcrImporter *importer,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, importer), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gcr_gnupg_importer_iface (GcrImporterInterface *iface)
{
	iface->create_for_parsed = _gcr_gnupg_importer_create_for_parsed;
	iface->queue_for_parsed = _gcr_gnupg_importer_queue_for_parsed;
	iface->import_async = _gcr_gnupg_importer_import_async;
	iface->import_finish = _gcr_gnupg_importer_import_finish;
}

/**
 * _gcr_gnupg_importer_new:
 * @directory: (nullable): the directory to import to, or %NULL for default
 *
 * Create a new #GcrGnupgImporter.
 *
 * Returns: (transfer full) (type Gcr.GnupgImporter): the new importer
 */
GcrImporter *
_gcr_gnupg_importer_new (const gchar *directory)
{
	return g_object_new (GCR_TYPE_GNUPG_IMPORTER,
	                     "directory", directory,
	                     NULL);
}

const gchar **
_gcr_gnupg_importer_get_imported (GcrGnupgImporter *self)
{
	g_return_val_if_fail (GCR_IS_GNUPG_IMPORTER (self), NULL);
	return (const gchar **)self->pv->imported->data;
}
