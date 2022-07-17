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

#include "gcr-importer.h"
#include "gcr-internal.h"
#include "gcr-gnupg-importer.h"
#include "gcr-parser.h"
#include "gcr-pkcs11-importer.h"

#include "gcr/gcr-marshal.h"

#include <glib/gi18n-lib.h>

/**
 * GcrImporter:
 *
 * An interface which allows importing of certificates and keys. Each importer
 * is registered with a set of PKCS#11 attributes to match stuff that it can
 * import.
 *
 * An importer gets passed a [class@Parser] and accesses the currently parsed
 * item. To create a set of importers that can import the currently parsed
 * item in a parser, use [func@Importer.create_for_parsed]. The list of
 * importers returned has the parsed item queued for import.
 *
 * To queue additional items with a importer use
 * [method@Importer.queue_for_parsed].  In addition you can try and queue an
 * additional item with a set of importers using the
 * [func@Importer.queue_and_filter_for_parsed].
 *
 * To start the import, use [method@Importer.import_async].
 */

/**
 * GcrImporterInterface:
 * @parent: parent interface
 * @create_for_parsed: implementation of gcr_importer_create_for_parsed(), required
 * @queue_for_parsed: implementation of gcr_importer_queue_for_parsed(), required
 * @import_async: implementation of [method@Importer.import_async], required
 * @import_finish: implementation of [method@Importer.import_finish]
 *
 * Interface implemented for a #GcrImporter.
 */

G_DEFINE_INTERFACE (GcrImporter, gcr_importer, G_TYPE_OBJECT);

typedef struct _GcrRegistered {
	GckAttributes *attrs;
	GType importer_type;
} GcrRegistered;

static GArray *registered_importers = NULL;
static gboolean registered_sorted = FALSE;

static void
gcr_importer_default_init (GcrImporterInterface *iface)
{
	static size_t initialized = 0;

	if (g_once_init_enter (&initialized)) {

		/**
		 * GcrImporter:label:
		 *
		 * The label for the importer.
		 */
		g_object_interface_install_property (iface,
			g_param_spec_string ("label", "Label", "The label for the importer",
			                     "",
			                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrImporter:interaction:
		 *
		 * The interaction for the importer.
		 */
		g_object_interface_install_property (iface,
			g_param_spec_object ("interaction", "Interaction",
			                     "Interaction for prompts",
			                     G_TYPE_TLS_INTERACTION,
			                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

		/**
		 * GcrImporter:uri:
		 *
		 * The URI of the location imported to.
		 */
		g_object_interface_install_property (iface,
			g_param_spec_string ("uri", "URI", "URI of location",
			                     NULL,
			                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		g_once_init_leave (&initialized, 1);
	}
}

/**
 * gcr_importer_register:
 * @importer_type: the GType of the importer being registered
 * @attrs: (transfer full): the attributes that this importer is compatible with
 *
 * Register an importer to handle parsed items that match the given attributes.
 */
void
gcr_importer_register (GType importer_type,
                       GckAttributes *attrs)
{
	GcrRegistered registered;

	if (!registered_importers)
		registered_importers = g_array_new (FALSE, FALSE, sizeof (GcrRegistered));

	registered.importer_type = importer_type;
	registered.attrs = attrs;
	g_array_append_val (registered_importers, registered);
	registered_sorted = FALSE;
}

static gint
sort_registered_by_n_attrs (gconstpointer a, gconstpointer b)
{
	const GcrRegistered *ra = a;
	const GcrRegistered *rb = b;
	gulong na, nb;

	g_assert (a);
	g_assert (b);

	na = gck_attributes_count (ra->attrs);
	nb = gck_attributes_count (rb->attrs);

	/* Note we're sorting in reverse order */
	if (na < nb)
		return 1;
	return (na == nb) ? 0 : -1;
}

static gboolean
check_if_seen_or_add (GHashTable *seen,
                      gpointer key)
{
	if (g_hash_table_lookup (seen, key))
		return TRUE;
	g_hash_table_insert (seen, key, key);
	return FALSE;
}

/**
 * gcr_importer_create_for_parsed:
 * @parsed: a parser with a parsed item to import
 *
 * Create a set of importers which can import this parsed item.
 * The parsed item is represented by the state of the GcrParser at the
 * time of calling this method.
 *
 * Returns: (element-type Gcr.Importer) (transfer full): a list of importers
 *          which can import the parsed item, which should be freed with
 *          g_object_unref(), or %NULL if no types of importers can be created
 */
GList *
gcr_importer_create_for_parsed (GcrParsed *parsed)
{
	GcrRegistered *registered;
	GcrImporterInterface *iface;
	gpointer instance_class;
	GckAttributes *attrs;
	gboolean matched;
	gulong n_attrs;
	GList *results = NULL;
	GHashTable *seen;
	gulong j;
	gsize i;

	g_return_val_if_fail (parsed != NULL, NULL);

	gcr_importer_register_well_known ();

	if (!registered_importers)
		return NULL;

	if (!registered_sorted) {
		g_array_sort (registered_importers, sort_registered_by_n_attrs);
		registered_sorted = TRUE;
	}

	attrs = gcr_parsed_get_attributes (parsed);
	if (attrs != NULL)
		gck_attributes_ref (attrs);
	else
		attrs = gck_attributes_new_empty (GCK_INVALID);

	seen = g_hash_table_new (g_direct_hash, g_direct_equal);

	gchar *a = gck_attributes_to_string (attrs);
	g_debug ("looking for importer for: %s", a);
	g_free (a);

	for (i = 0; i < registered_importers->len; ++i) {
		registered = &(g_array_index (registered_importers, GcrRegistered, i));
		n_attrs = gck_attributes_count (registered->attrs);

		matched = TRUE;

		for (j = 0; j < n_attrs; ++j) {
			if (!gck_attributes_contains (attrs, gck_attributes_at (registered->attrs, j))) {
				matched = FALSE;
				break;
			}
		}

		gchar *a = gck_attributes_to_string (registered->attrs);
		g_debug ("importer %s %s: %s", g_type_name (registered->importer_type),
		         matched ? "matched" : "didn't match", a);
		g_free (a);

		if (matched) {
			if (check_if_seen_or_add (seen, GUINT_TO_POINTER (registered->importer_type)))
				continue;

			instance_class = g_type_class_ref (registered->importer_type);

			iface = g_type_interface_peek (instance_class, GCR_TYPE_IMPORTER);
			g_return_val_if_fail (iface != NULL, NULL);
			g_return_val_if_fail (iface->create_for_parsed, NULL);
			results = g_list_concat (results, (iface->create_for_parsed) (parsed));

			g_type_class_unref (instance_class);
		}
	}

	g_hash_table_unref (seen);
	gck_attributes_unref (attrs);
	return results;
}

/**
 * gcr_importer_queue_for_parsed:
 * @importer: an importer to add additional items to
 * @parsed: a parsed item to import
 *
 * Queues an additional item to be imported. The parsed item is represented
 * by the state of the [class@Parser] at the time of calling this method.
 *
 * If the parsed item is incompatible with the importer, then this will
 * fail and the item will not be queued.
 *
 * Returns: whether the item was queued or not
 */
gboolean
gcr_importer_queue_for_parsed (GcrImporter *importer,
                               GcrParsed *parsed)
{
	GcrImporterInterface *iface;

	g_return_val_if_fail (GCR_IS_IMPORTER (importer), FALSE);
	g_return_val_if_fail (parsed != NULL, FALSE);

	iface = GCR_IMPORTER_GET_IFACE (importer);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->queue_for_parsed != NULL, FALSE);

	return (iface->queue_for_parsed) (importer, parsed);
}

/**
 * gcr_importer_queue_and_filter_for_parsed:
 * @importers: (element-type Gcr.Importer): a set of importers
 * @parsed: a parsed item
 *
 * Queues an additional item to be imported in all compattible importers
 * in the set. The parsed item is represented by the state of the #GcrParser
 * at the time of calling this method.
 *
 * If the parsed item is incompatible with an importer, then that the item
 * will not be queued on that importer.
 *
 * Returns: (transfer full) (element-type Gcr.Importer): a new set of importers
 *          that queued the item.
 */
GList *
gcr_importer_queue_and_filter_for_parsed (GList *importers,
                                          GcrParsed *parsed)
{
	GList *results = NULL;
	GList *l;

	for (l = importers; l != NULL; l = g_list_next (l)) {
		if (gcr_importer_queue_for_parsed (l->data, parsed))
			results = g_list_prepend (results, g_object_ref (l->data));
	}

	return g_list_reverse (results);
}

/**
 * gcr_importer_import_async:
 * @importer: the importer
 * @cancellable: a #GCancellable, or %NULL
 * @callback: called when the operation completes
 * @user_data: data to be passed to the callback
 *
 * Import the queued items in the importer. This function returns immediately
 * and completes asynchronously.
 */
void
gcr_importer_import_async (GcrImporter *importer,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	GcrImporterInterface *iface;

	g_return_if_fail (GCR_IS_IMPORTER (importer));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCR_IMPORTER_GET_IFACE (importer);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->import_async != NULL);

	return (iface->import_async) (importer, cancellable, callback, user_data);
}

/**
 * gcr_importer_import_finish:
 * @importer: the importer
 * @result: an asynchronous result
 * @error: the location to place an error on failure, or %NULL
 *
 * Complete an asynchronous operation to import queued items.
 *
 * Returns: whether the import succeeded or failed
 */
gboolean
gcr_importer_import_finish (GcrImporter *importer,
                            GAsyncResult *result,
                            GError **error)
{
	GcrImporterInterface *iface;

	g_return_val_if_fail (GCR_IS_IMPORTER (importer), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	iface = GCR_IMPORTER_GET_IFACE (importer);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->import_finish != NULL, FALSE);

	return (iface->import_finish) (importer, result, error);
}

/**
 * gcr_importer_get_interaction:
 * @importer: the importer
 *
 * Get the interaction used to prompt the user when needed by this
 * importer.
 *
 * Returns: (transfer none) (nullable): the interaction or %NULL
 */
GTlsInteraction *
gcr_importer_get_interaction (GcrImporter *importer)
{
	GTlsInteraction *interaction = NULL;

	g_return_val_if_fail (GCR_IS_IMPORTER (importer), NULL);

	g_object_get (importer, "interaction", &interaction, NULL);

	if (interaction != NULL)
		g_object_unref (interaction);

	return interaction;
}

/**
 * gcr_importer_set_interaction:
 * @importer: the importer
 * @interaction: the interaction used by the importer
 *
 * Set the interaction used to prompt the user when needed by this
 * importer.
 */
void
gcr_importer_set_interaction (GcrImporter *importer,
                              GTlsInteraction *interaction)
{
	g_return_if_fail (GCR_IS_IMPORTER (importer));
	g_object_set (importer, "interaction", interaction, NULL);
}

/**
 * gcr_importer_register_well_known:
 *
 * Register built-in PKCS#11 and GnuPG importers.
 */
void
gcr_importer_register_well_known (void)
{
	g_type_class_unref (g_type_class_ref (GCR_TYPE_PKCS11_IMPORTER));
	g_type_class_unref (g_type_class_ref (GCR_TYPE_GNUPG_IMPORTER));
}
