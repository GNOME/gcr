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

#include "gcr-memory-icon.h"

#include <string.h>

struct _GcrMemoryIconPrivate {
	gpointer data;
	gsize n_data;
	goffset offset;
	gchar *image_type;
	GDestroyNotify destroy;
};

/* Forward declarations */
static void _gcr_memory_icon_iface_icon (GIconIface *iface);
static void _gcr_memory_icon_iface_loadable_icon (GLoadableIconIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrMemoryIcon, _gcr_memory_icon, G_TYPE_OBJECT,
	G_ADD_PRIVATE (GcrMemoryIcon);
	G_IMPLEMENT_INTERFACE (G_TYPE_ICON, _gcr_memory_icon_iface_icon);
	G_IMPLEMENT_INTERFACE (G_TYPE_LOADABLE_ICON, _gcr_memory_icon_iface_loadable_icon);
);


static void
_gcr_memory_icon_init (GcrMemoryIcon *self)
{
	self->pv = _gcr_memory_icon_get_instance_private (self);
}

static void
_gcr_memory_icon_finalize (GObject *obj)
{
	GcrMemoryIcon *self = GCR_MEMORY_ICON (obj);

	if (self->pv->destroy)
		(self->pv->destroy) (self->pv->data);
	g_free (self->pv->image_type);

	G_OBJECT_CLASS (_gcr_memory_icon_parent_class)->finalize (obj);
}

static void
_gcr_memory_icon_class_init (GcrMemoryIconClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = _gcr_memory_icon_finalize;
}

static gboolean
_gcr_memory_icon_equal (GIcon *icon1, GIcon *icon2)
{
	GcrMemoryIcon *one = GCR_MEMORY_ICON (icon1);
	GcrMemoryIcon *two = GCR_MEMORY_ICON (icon2);

	if (icon1 == icon2)
		return TRUE;
	if (!g_str_equal (one->pv->image_type, two->pv->image_type))
		return FALSE;
	if ((one->pv->n_data - one->pv->offset) != (two->pv->n_data - two->pv->offset))
		return FALSE;
	return memcmp ((guchar*)one->pv->data + one->pv->offset,
	               (guchar*)two->pv->data + two->pv->offset,
	               one->pv->n_data - one->pv->offset) == 0;
}

static guint
_gcr_memory_icon_hash (GIcon *icon)
{
	GcrMemoryIcon *self = GCR_MEMORY_ICON (icon);
	const signed char *p, *end;
	guint32 hash;

	hash = g_str_hash (self->pv->image_type);

	/* Adapted from g_str_hash */
	p = self->pv->data;
	end = p + self->pv->n_data;
	p += self->pv->offset;
	while (p < end)
		hash = (hash << 5) + hash + *(p++);

	return hash;
}

static void
_gcr_memory_icon_iface_icon (GIconIface *iface)
{
	iface->equal = _gcr_memory_icon_equal;
	iface->hash = _gcr_memory_icon_hash;
}

static GInputStream *
_gcr_memory_icon_load (GLoadableIcon *icon, int size, gchar **type,
                       GCancellable *cancellable, GError **error)
{
	GcrMemoryIcon *self = GCR_MEMORY_ICON (icon);
	GInputStream *is;

	if (type != NULL)
		*type = g_strdup (self->pv->image_type);

	is = g_memory_input_stream_new_from_data ((guchar*)self->pv->data + self->pv->offset,
	                                          self->pv->n_data, NULL);

	/*
	 * Hold a reference to this object from the stream, so that we can rely
	 * on the data hanging around.
	 */
	g_object_set_data_full (G_OBJECT (is), "back-reference", g_object_ref (self),
	                        g_object_unref);

	return is;
}

static void
_gcr_memory_icon_load_async (GLoadableIcon *icon, int size, GCancellable *cancellable,
                             GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;

	task = g_task_new (icon, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gcr_memory_icon_load_async);

	/* We don't do anything with the task value, so just return a bogus value */
	g_task_return_pointer (task, NULL, NULL);
	g_object_unref (task);
}

static GInputStream*
_gcr_memory_icon_finish (GLoadableIcon *icon, GAsyncResult *res, char **type,
                         GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, icon), NULL);

	return _gcr_memory_icon_load (icon, 0, type, NULL, error);
}


static void
_gcr_memory_icon_iface_loadable_icon (GLoadableIconIface *iface)
{
	iface->load = _gcr_memory_icon_load;
	iface->load_async = _gcr_memory_icon_load_async;
	iface->load_finish = _gcr_memory_icon_finish;
}

/**
 * _gcr_memory_icon_new:
 * @image_type: MIME content-type of the image.
 * @data: Data for the image.
 * @n_data: Length of data.
 *
 * Create a new GIcon based on image data in memory. The data will be copied
 * by the new icon.
 *
 * Returns: (transfer full) (type Gcr.MemoryIcon): A newly allocated icon.
 */
GIcon*
_gcr_memory_icon_new (const gchar *image_type, gconstpointer data, gsize n_data)
{
	g_return_val_if_fail (image_type != NULL, NULL);
	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (n_data != 0, NULL);

	return _gcr_memory_icon_new_full (image_type, g_memdup (data, n_data),
	                                  n_data, 0, g_free);
}

/**
 * _gcr_memory_icon_new_full:
 * @image_type: MIME content-type of the image.
 * @data: Data for the image.
 * @n_data: Length of data.
 * @offset: Offset of the start of the image in @data.
 * @destroy: Callback to free or release @data when no longer needed.
 *
 * Create a new GIcon based on image data in memory. The data will be used
 * directly from the @data passed. Use @destroy to control the lifetime of
 * the data in memory.
 *
 * Returns: (transfer full): A newly allocated icon.
 */
GIcon*
_gcr_memory_icon_new_full (const gchar *image_type, gpointer data, gsize n_data,
                           goffset offset, GDestroyNotify destroy)
{
	GcrMemoryIcon *self;

	g_return_val_if_fail (image_type != NULL, NULL);
	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (offset < n_data, NULL);

	self = g_object_new (GCR_TYPE_MEMORY_ICON, NULL);
	self->pv->data = data;
	self->pv->n_data = n_data;
	self->pv->offset = offset;
	self->pv->destroy = destroy;
	self->pv->image_type = g_strdup (image_type);

	return G_ICON (self);
}
