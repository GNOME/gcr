/*
 * gnome-keyring
 *
 * Copyright (C) 2014 Stef Walter
 * Copyright (C) 2018 Red Hat, Inc.
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stef@thewalter.net>, Daiki Ueno
 */

#include "config.h"

#include "gcr-ssh-agent-preload.h"

#include "gcr-ssh-agent-util.h"
#include <string.h>

enum {
	PROP_0,
	PROP_PATH
};

struct _GcrSshAgentPreload
{
	GObject object;

	gchar *path;
	GHashTable *keys_by_public_filename;
	GHashTable *keys_by_public_key;
	GFileMonitor *file_monitor;
	GMutex lock;
};

G_DEFINE_TYPE (GcrSshAgentPreload, gcr_ssh_agent_preload, G_TYPE_OBJECT);
G_DEFINE_BOXED_TYPE(GcrSshAgentKeyInfo, gcr_ssh_agent_key_info, gcr_ssh_agent_key_info_copy, gcr_ssh_agent_key_info_free)

void
gcr_ssh_agent_key_info_free (gpointer boxed)
{
	GcrSshAgentKeyInfo *info = boxed;
	if (!info)
		return;
	g_clear_pointer (&info->public_key, g_bytes_unref);
	g_clear_pointer (&info->comment, g_free);
	g_clear_pointer (&info->filename, g_free);
	g_free (info);
}

gpointer
gcr_ssh_agent_key_info_copy (gpointer boxed)
{
	GcrSshAgentKeyInfo *info = boxed;
	GcrSshAgentKeyInfo *copy = g_new0 (GcrSshAgentKeyInfo, 1);
	copy->public_key = g_bytes_ref (info->public_key);
	copy->comment = g_strdup (info->comment);
	copy->filename = g_strdup (info->filename);
	return copy;
}

static void changed_inlock (GFileMonitor *monitor,
                            GFile *file,
                            GFile *other_file,
                            GFileMonitorEvent event_type,
                            gpointer user_data);

static void
gcr_ssh_agent_preload_init (GcrSshAgentPreload *self)
{
	self->keys_by_public_filename = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	self->keys_by_public_key = g_hash_table_new_full (g_bytes_hash, g_bytes_equal, NULL, gcr_ssh_agent_key_info_free);
}

static void
gcr_ssh_agent_preload_constructed (GObject *object)
{
	GcrSshAgentPreload *self = GCR_SSH_AGENT_PRELOAD (object);
	GError *error = NULL;
	GFile *file;

	file = g_file_new_for_path (self->path);
	self->file_monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &error);
	if (!self->file_monitor) {
		g_critical ("Unable to listen to directory %s: %s", self->path, error->message);
		g_clear_error (&error);
	} else
		g_signal_connect (self->file_monitor, "changed", G_CALLBACK (changed_inlock), self);
	g_object_unref (file);

	G_OBJECT_CLASS (gcr_ssh_agent_preload_parent_class)->constructed (object);
}

static void
gcr_ssh_agent_preload_set_property (GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	GcrSshAgentPreload *self = GCR_SSH_AGENT_PRELOAD (object);

	switch (prop_id) {
	case PROP_PATH:
		self->path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gcr_ssh_agent_preload_finalize (GObject *object)
{
	GcrSshAgentPreload *self = GCR_SSH_AGENT_PRELOAD (object);

	g_clear_pointer (&self->path, g_free);
	g_clear_pointer (&self->keys_by_public_key, g_hash_table_unref);
	g_clear_pointer (&self->keys_by_public_filename, g_hash_table_unref);
	g_signal_handlers_disconnect_by_func (self->file_monitor, changed_inlock, self);
	g_clear_object (&self->file_monitor);

	g_mutex_clear (&self->lock);

	G_OBJECT_CLASS (gcr_ssh_agent_preload_parent_class)->finalize (object);
}

static void
gcr_ssh_agent_preload_class_init (GcrSshAgentPreloadClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructed = gcr_ssh_agent_preload_constructed;
	gobject_class->set_property = gcr_ssh_agent_preload_set_property;
	gobject_class->finalize = gcr_ssh_agent_preload_finalize;
	g_object_class_install_property (gobject_class, PROP_PATH,
		 g_param_spec_string ("path", "Path", "Path",
				      "",
				      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

static GBytes *
file_get_contents (const gchar *path,
                   gboolean must_be_present)
{
	GError *error = NULL;
	gchar *contents;
	gsize length;

	if (!g_file_get_contents (path, &contents, &length, &error)) {
		if (must_be_present || error->code != G_FILE_ERROR_NOENT)
			g_message ("couldn't read file: %s: %s", path, error->message);
		g_error_free (error);
		return NULL;
	}

	return g_bytes_new_take (contents, length);
}

static void
file_remove_inlock (GcrSshAgentPreload *self, const gchar *path)
{
	GcrSshAgentKeyInfo *info;

	info = g_hash_table_lookup (self->keys_by_public_filename, path);
	if (info) {
		g_hash_table_remove (self->keys_by_public_filename, path);
		g_hash_table_remove (self->keys_by_public_key, info->public_key);
	}
}

static void
file_load_inlock (GcrSshAgentPreload *self, const gchar *path)
{
	gchar *private_path = NULL;
	GBytes *public_bytes;

	file_remove_inlock (self, path);

	if (g_str_has_suffix (path, ".pub")) {
		GBytes *private_bytes;

		private_path = g_strndup (path, strlen (path) - 4);
		private_bytes = file_get_contents (private_path, FALSE);
		if (!private_bytes) {
			g_debug ("no private key present for public key: %s", path);
			g_free (private_path);
			return;
		}

		g_bytes_unref (private_bytes);
	}

	public_bytes = file_get_contents (path, TRUE);
	if (public_bytes) {
		gchar *comment = NULL;
		GBytes *public_key;

		public_key = _gcr_ssh_agent_parse_public_key (public_bytes, &comment);
		if (public_key) {
			GcrSshAgentKeyInfo *info;

			info = g_new0 (GcrSshAgentKeyInfo, 1);
			info->filename = g_steal_pointer (&private_path);
			info->public_key = g_steal_pointer (&public_key);
			info->comment = g_steal_pointer (&comment);
			g_hash_table_replace (self->keys_by_public_filename, g_strdup (path), info);
			g_hash_table_replace (self->keys_by_public_key, info->public_key, info);
		} else {
			g_message ("failed to parse ssh public key: %s", path);
		}

		g_bytes_unref (public_bytes);
	}

	g_free (private_path);
}

static void
changed_inlock (GFileMonitor *monitor,
                GFile *file,
                GFile *other_file,
                GFileMonitorEvent event_type,
                gpointer user_data)
{
	GcrSshAgentPreload *self = GCR_SSH_AGENT_PRELOAD (user_data);
	char *path;

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_CREATED:
		path = g_file_get_path (file);
		if (g_str_has_suffix (path, ".pub"))
			file_load_inlock (self, path);
		g_free (path);
		break;
	case G_FILE_MONITOR_EVENT_DELETED:
		path = g_file_get_path (file);
		if (g_str_has_suffix (path, ".pub"))
			file_remove_inlock (self, path);
		g_free (path);
		break;
	default:
		break;
	}
}

static void
refresh_listened_directory (GcrSshAgentPreload *self)
{
	GFile *file;
	GFileEnumerator *direnum;
	GError *error = NULL;

	file = g_file_new_for_path (self->path);
	direnum = g_file_enumerate_children (file,
	                                     G_FILE_ATTRIBUTE_STANDARD_TYPE G_FILE_ATTRIBUTE_STANDARD_NAME G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
	                                     G_FILE_QUERY_INFO_NONE,
	                                     NULL,
	                                     &error);
	g_object_unref (file);

	while (TRUE) {
		GFileInfo *info;
		const char *name;
		if (!g_file_enumerator_iterate (direnum, &info, NULL, NULL, &error)) {
			g_critical ("Error while iterating files in %s: %s", self->path, error->message);
			g_clear_error (&error);
			break;
		}

		if (!info)
			break;

		if (g_file_info_get_is_hidden (info) ||
		    g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
			continue;

		name = g_file_info_get_name (info);
		if (g_str_has_suffix (name, ".pub")) {
			char *path = g_build_filename (self->path, name, NULL);
			file_load_inlock (self, path);
			g_free (path);
		}
	}

	g_object_unref (direnum);
}

GcrSshAgentPreload *
gcr_ssh_agent_preload_new (const gchar *path)
{
	g_return_val_if_fail (path, NULL);

	return g_object_new (GCR_TYPE_SSH_AGENT_PRELOAD, "path", path, NULL);
}

GList *
gcr_ssh_agent_preload_get_keys (GcrSshAgentPreload *self)
{
	GList *keys = NULL;
	GHashTableIter iter;
	GcrSshAgentKeyInfo *info;

	g_mutex_lock (&self->lock);

	refresh_listened_directory (self);

	g_hash_table_iter_init (&iter, self->keys_by_public_key);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&info))
		keys = g_list_prepend (keys, gcr_ssh_agent_key_info_copy (info));

	g_mutex_unlock (&self->lock);

	return keys;
}

GcrSshAgentKeyInfo *
gcr_ssh_agent_preload_lookup_by_public_key (GcrSshAgentPreload *self,
					    GBytes *public_key)
{
	GcrSshAgentKeyInfo *info;

	g_mutex_lock (&self->lock);

	refresh_listened_directory (self);

	info = g_hash_table_lookup (self->keys_by_public_key, public_key);
	if (info)
		info = gcr_ssh_agent_key_info_copy (info);

	g_mutex_unlock (&self->lock);

	return info;
}
