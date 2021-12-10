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

#include "gcr-import-interaction.h"

/**
 * GcrImportInteraction:
 *
 * This is an interface implemented by a caller performing an import. It allows
 * the importer to ask the caller for further information about the import.
 *
 * It must be implemented on a derived class of [class@Gio.TlsInteraction]
 */

/**
 * GcrImportInteractionInterface:
 * @parent: parent interface
 * @supplement_prep: method which prepares for supplementing the given attributes before import
 * @supplement: method which synchronously supplements attributes before import
 * @supplement_async: method which asynchronously supplements attributes before import
 * @supplement_finish: method which completes @supplement_async
 *
 * Interface implemented by implementations of [iface@ImportInteraction].
 */

G_DEFINE_INTERFACE (GcrImportInteraction, gcr_import_interaction, G_TYPE_TLS_INTERACTION);

static void
gcr_import_interaction_default_init (GcrImportInteractionInterface *iface)
{
}

/**
 * gcr_import_interaction_supplement_prep:
 * @interaction: the interaction
 * @builder: attributes to supplement
 *
 * Prepare for supplementing the given attributes before import. This means
 * prompting the user for things like labels and the like. The attributes
 * will contain attributes for values that the importer needs, either empty
 * or prefilled with suggested values.
 *
 * This method does not prompt the user, but rather just prepares the
 * interaction that these are the attributes that are needed.
 */
void
gcr_import_interaction_supplement_prep (GcrImportInteraction *interaction,
                                        GckBuilder *builder)
{
	GcrImportInteractionInterface *iface;

	g_return_if_fail (GCR_IS_IMPORT_INTERACTION (interaction));
	g_return_if_fail (builder != NULL);

	iface = GCR_IMPORT_INTERACTION_GET_IFACE (interaction);
	if (iface->supplement != NULL)
		(iface->supplement_prep) (interaction, builder);
}

/**
 * gcr_import_interaction_supplement:
 * @interaction: the interaction
 * @builder: supplemented attributes
 * @cancellable: optional cancellable object
 * @error: location to store error on failure
 *
 * Supplement attributes before import. This means prompting the user for
 * things like labels and the like. The needed attributes will have been passed
 * to gcr_import_interaction_supplement_prep().
 *
 * This method prompts the user and fills in the attributes. If the user or
 * cancellable cancels the operation the error should be set with %G_IO_ERROR_CANCELLED.
 *
 * Returns: %G_TLS_INTERACTION_HANDLED if successful or %G_TLS_INTERACTION_FAILED
 */
GTlsInteractionResult
gcr_import_interaction_supplement (GcrImportInteraction *interaction,
                                   GckBuilder *builder,
                                   GCancellable *cancellable,
                                   GError **error)
{
	GcrImportInteractionInterface *iface;

	g_return_val_if_fail (GCR_IS_IMPORT_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
	g_return_val_if_fail (builder != NULL, G_TLS_INTERACTION_UNHANDLED);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), G_TLS_INTERACTION_UNHANDLED);
	g_return_val_if_fail (error == NULL || *error == NULL, G_TLS_INTERACTION_UNHANDLED);

	iface = GCR_IMPORT_INTERACTION_GET_IFACE (interaction);
	g_return_val_if_fail (iface->supplement != NULL, G_TLS_INTERACTION_UNHANDLED);

	return (iface->supplement) (interaction, builder, cancellable, error);
}


/**
 * gcr_import_interaction_supplement_async:
 * @interaction: the interaction
 * @builder: supplemented attributes
 * @cancellable: optional cancellable object
 * @callback: called when the operation completes
 * @user_data: data to be passed to the callback
 *
 * Asynchronously supplement attributes before import. This means prompting the
 * user for things like labels and the like. The needed attributes will have
 * been passed to gcr_import_interaction_supplement_prep().
 *
 * This method prompts the user and fills in the attributes.
 */
void
gcr_import_interaction_supplement_async (GcrImportInteraction *interaction,
                                         GckBuilder *builder,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
	GcrImportInteractionInterface *iface;

	g_return_if_fail (GCR_IS_IMPORT_INTERACTION (interaction));
	g_return_if_fail (builder != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = GCR_IMPORT_INTERACTION_GET_IFACE (interaction);
	g_return_if_fail (iface->supplement != NULL);

	(iface->supplement_async) (interaction, builder, cancellable, callback, user_data);
}

/**
 * gcr_import_interaction_supplement_finish:
 * @interaction: the interaction
 * @result: the asynchronous result
 * @error: location to place an error on failure
 *
 * Complete operation to asynchronously supplement attributes before import.
 *
 * If the user or cancellable cancels the operation the error should be set
 * with %G_IO_ERROR_CANCELLED.
 *
 * Returns: %G_TLS_INTERACTION_HANDLED if successful or %G_TLS_INTERACTION_FAILED
 */
GTlsInteractionResult
gcr_import_interaction_supplement_finish (GcrImportInteraction *interaction,
                                          GAsyncResult *result,
                                          GError **error)
{
	GcrImportInteractionInterface *iface;

	g_return_val_if_fail (GCR_IS_IMPORT_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), G_TLS_INTERACTION_UNHANDLED);
	g_return_val_if_fail (error == NULL || *error == NULL, G_TLS_INTERACTION_UNHANDLED);

	iface = GCR_IMPORT_INTERACTION_GET_IFACE (interaction);
	g_return_val_if_fail (iface->supplement != NULL, G_TLS_INTERACTION_UNHANDLED);

	return (iface->supplement_finish) (interaction, result, error);
}
