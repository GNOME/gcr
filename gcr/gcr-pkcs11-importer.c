/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "gcr-fingerprint.h"
#include "gcr-internal.h"
#include "gcr-library.h"
#include "gcr-import-interaction.h"
#include "gcr-internal.h"
#include "gcr-parser.h"
#include "gcr-pkcs11-importer.h"

#include "egg/egg-hex.h"

#include <gck/gck.h>

#include <gcrypt.h>

#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_LABEL,
	PROP_INTERACTION,
	PROP_SLOT,
	PROP_QUEUED,
	PROP_URI
};

struct _GcrPkcs11Importer {
	GObject parent;
	GckSlot *slot;
	GList *objects;
	GckSession *session;
	GQueue *queue;
	GTlsInteraction *interaction;
	gboolean any_private;
};

typedef struct  {
	GcrPkcs11Importer *importer;
	gboolean prompted;
	gboolean async;
	GckBuilder *supplement;
} GcrImporterData;

/* frward declarations */
static void   state_cancelled                  (GTask *task,
                                                gboolean async);
static void   state_create_object              (GTask *task,
                                                gboolean async);
static void   _gcr_pkcs11_importer_init_iface  (GcrImporterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrPkcs11Importer, _gcr_pkcs11_importer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_IMPORTER, _gcr_pkcs11_importer_init_iface);
);

#define BLOCK 4096

static void
gcr_importer_data_free (gpointer data)
{
	GcrImporterData *state = data;

	g_clear_object (&state->importer);
	gck_builder_unref (state->supplement);
	g_free (state);
}

static void
next_state (GTask *task,
            void (*state) (GTask *, gboolean))
{
	GcrImporterData *data = g_task_get_task_data (task);

	g_assert (state);

	if (g_cancellable_is_cancelled (g_task_get_cancellable (task)))
		state = state_cancelled;

	(state) (task, data->async);
}

/* ---------------------------------------------------------------------------------
 * COMPLETE
 */

static void
state_cancelled (GTask *task,
                 gboolean async)
{
	GCancellable *cancellable = g_task_get_cancellable (task);
	GError *error = NULL;

	if (cancellable && !g_cancellable_is_cancelled (cancellable))
		g_cancellable_cancel (cancellable);

	g_cancellable_set_error_if_cancelled (cancellable, &error);
	g_task_return_error (task, g_steal_pointer (&error));
}

/* ---------------------------------------------------------------------------------
 * CREATE OBJECTS
 */

static void
complete_create_object (GTask *task,
                        GckObject *object,
                        GError *error)
{
	GcrImporterData *data = g_task_get_task_data (task);
	GcrPkcs11Importer *self = data->importer;

	if (object == NULL) {
		g_task_return_error (task, g_steal_pointer (&error));

	} else {
		self->objects = g_list_append (self->objects, object);
		next_state (task, state_create_object);
	}
}

static void
on_create_object (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GError *error = NULL;
	GckObject *object;

	object = gck_session_create_object_finish (GCK_SESSION (source), result, &error);
	complete_create_object (task, object, error);
	g_clear_object (&task);
}

static void
state_create_object (GTask *task,
                     gboolean async)
{
	GcrImporterData *data = g_task_get_task_data (task);
	GCancellable *cancellable = g_task_get_cancellable (task);
	GcrPkcs11Importer *self = data->importer;
	GckAttributes *attrs;
	GckObject *object;
	GError *error = NULL;

	/* No more objects */
	if (g_queue_is_empty (self->queue)) {
		g_task_return_boolean (task, TRUE);

	} else {

		/* Pop first one off the list */
		attrs = g_queue_pop_head (self->queue);
		g_assert (attrs != NULL);

		if (async) {
			gck_session_create_object_async (self->session, attrs,
			                                 cancellable, on_create_object,
			                                 g_object_ref (task));
		} else {
			object = gck_session_create_object (self->session, attrs,
			                                    cancellable, &error);
			complete_create_object (task, object, error);
		}

		gck_attributes_unref (attrs);
	}
}

/* ---------------------------------------------------------------------------------
 * SUPPLEMENTING and FIXING UP
 */

typedef struct {
	GckAttributes *certificate;
	GckAttributes *private_key;
} CertificateKeyPair;

static void
supplement_with_attributes (GckBuilder *builder,
                            GckAttributes *supplements)
{
	const GckAttribute *supplement;
	gint i;

	for (i = 0; i < gck_attributes_count (supplements); i++) {
		supplement = gck_attributes_at (supplements, i);
		if (!gck_attribute_is_invalid (supplement) && supplement->length != 0)
			gck_builder_add_attribute (builder, supplement);
	}
}

static void
supplement_id_for_data (GckBuilder *builder,
                        guchar *nonce,
                        gsize n_once,
                        gpointer data,
                        gsize n_data)
{
	gcry_md_hd_t mdh;
	gcry_error_t gcry;

	if (gck_builder_find (builder, CKA_ID) != NULL)
		return;

	gcry = gcry_md_open (&mdh, GCRY_MD_SHA1, 0);
	g_return_if_fail (gcry == 0);

	gcry_md_write (mdh, nonce, n_once);
	gcry_md_write (mdh, data, n_data);

	gck_builder_add_data (builder, CKA_ID,
	                      gcry_md_read (mdh, 0),
	                      gcry_md_get_algo_dlen (GCRY_MD_SHA1));

	gcry_md_close (mdh);
}

static void
supplement_attributes (GcrPkcs11Importer *self,
                       GckAttributes *supplements)
{
	GckBuilder builder = GCK_BUILDER_INIT;
	GHashTable *pairs;
	GHashTable *paired;
	CertificateKeyPair *pair;
	gboolean supplemented = FALSE;
	GckAttributes *attrs;
	gulong klass;
	guchar *finger;
	gchar *fingerprint;
	guchar nonce[20];
	GHashTableIter iter;
	gsize n_finger;
	GQueue *queue;
	GList *l;

	/* A table of certificate/key pairs by fingerprint */
	pairs = g_hash_table_new_full (g_str_hash, g_str_equal,
	                               g_free, g_free);

	for (l = self->queue->head; l != NULL; l = g_list_next (l)) {
		attrs = l->data;
		if (!gck_attributes_find_ulong (attrs, CKA_CLASS, &klass))
			g_return_if_reached ();

		/* Make a string fingerprint for this guy */
		finger = gcr_fingerprint_from_attributes (attrs, G_CHECKSUM_SHA1,
		                                          &n_finger);
		if (finger) {
			fingerprint = egg_hex_encode (finger, n_finger);
			g_free (finger);

			pair = g_hash_table_lookup (pairs, fingerprint);
			if (pair == NULL) {
				pair = g_new0 (CertificateKeyPair, 1);
				g_hash_table_insert (pairs, fingerprint, pair);
			} else {
				g_free (fingerprint);
			}
		} else {
			pair = NULL;
		}

		fingerprint = NULL;
		gck_builder_add_all (&builder, attrs);
		gck_builder_set_boolean (&builder, CKA_TOKEN, CK_TRUE);

		switch (klass) {
		case CKO_CERTIFICATE:
			gck_builder_set_boolean (&builder, CKA_PRIVATE, FALSE);
			break;
		case CKO_PRIVATE_KEY:
			gck_builder_set_boolean (&builder, CKA_PRIVATE, TRUE);
			gck_builder_add_boolean (&builder, CKA_DECRYPT, TRUE);
			gck_builder_add_boolean (&builder, CKA_SIGN, TRUE);
			gck_builder_add_boolean (&builder, CKA_SIGN_RECOVER, TRUE);
			gck_builder_add_boolean (&builder, CKA_UNWRAP, TRUE);
			gck_builder_add_boolean (&builder, CKA_SENSITIVE, TRUE);
			break;
		}

		gck_attributes_unref (attrs);
		l->data = attrs = gck_builder_end (&builder);

		switch (klass) {
		case CKO_CERTIFICATE:
			if (pair != NULL && pair->certificate == NULL)
				pair->certificate = attrs;
			break;
		case CKO_PRIVATE_KEY:
			if (pair != NULL && pair->private_key == NULL)
				pair->private_key = attrs;
			break;
		}
	}

	/* For generation of CKA_ID's */
	gcry_create_nonce (nonce, sizeof (nonce));

	/* A table for marking which attributes are in the pairs table */
	paired = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* Now move everything in pairs to the front */
	queue = g_queue_new ();
	g_hash_table_iter_init (&iter, pairs);
	while (g_hash_table_iter_next (&iter, (gpointer *)&fingerprint, (gpointer *)&pair)) {
		if (pair->certificate != NULL && pair->private_key != NULL) {
			/*
			 * Generate a CKA_ID based on the fingerprint and nonce,
			 * and do the same CKA_ID for both private key and certificate.
			 */

			gck_builder_add_all (&builder, pair->private_key);
			supplement_with_attributes (&builder, supplements);
			supplement_id_for_data (&builder, nonce, sizeof (nonce),
			                        fingerprint, strlen (fingerprint));
			g_queue_push_tail (queue, gck_builder_end (&builder));
			g_hash_table_insert (paired, pair->private_key, "present");

			gck_builder_add_all (&builder, pair->certificate);
			supplement_with_attributes (&builder, supplements);
			supplement_id_for_data (&builder, nonce, sizeof (nonce),
			                        fingerprint, strlen (fingerprint));
			g_queue_push_tail (queue, gck_builder_end (&builder));
			g_hash_table_insert (paired, pair->certificate, "present");

			/* Used the suplements for the pairs, don't use for unpaired stuff */
			supplemented = TRUE;
		}
	}

	/* Go through the old queue, and look for anything not paired */
	for (l = self->queue->head; l != NULL; l = g_list_next (l)) {
		attrs = l->data;
		if (!g_hash_table_lookup (paired, attrs)) {
			gck_builder_add_all (&builder, attrs);
			if (!supplemented)
				supplement_with_attributes (&builder, supplements);

			/*
			 * Generate a CKA_ID based on the location of attrs in,
			 * memory, since this together with the nonce should
			 * be unique.
			 */
			supplement_id_for_data (&builder, nonce, sizeof (nonce),
			                        &attrs, sizeof (gpointer));

			g_queue_push_tail (queue, gck_builder_end (&builder));
		}
	}

	/* And swap the new queue into place */
	g_queue_foreach (self->queue, (GFunc)gck_attributes_unref, NULL);
	g_queue_free (self->queue);
	self->queue = queue;

	g_hash_table_destroy (paired);
	g_hash_table_destroy (pairs);
}

static void
complete_supplement (GTask *task,
                     GError *error)
{
	GcrImporterData *data = g_task_get_task_data (task);
	GckAttributes *attributes;

	if (error == NULL) {
		attributes = gck_builder_end (data->supplement);
		supplement_attributes (data->importer, attributes);
		gck_attributes_unref (attributes);

		next_state (task, state_create_object);
	} else {
		g_task_return_error (task, g_steal_pointer (&error));
	}
}

static void
on_supplement_done (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GcrImporterData *data = g_task_get_task_data (task);
	GcrPkcs11Importer *self = data->importer;
	GError *error = NULL;

	gcr_import_interaction_supplement_finish (GCR_IMPORT_INTERACTION (self->interaction),
	                                          result, &error);
	complete_supplement (task, error);
	g_clear_object (&task);
}

static void
state_supplement (GTask *task,
                  gboolean async)
{
	GcrImporterData *data = g_task_get_task_data (task);
	GCancellable *cancellable = g_task_get_cancellable (task);
	GcrPkcs11Importer *self = data->importer;
	GError *error = NULL;

	if (self->interaction == NULL || !GCR_IS_IMPORT_INTERACTION (self->interaction)) {
		complete_supplement (task, NULL);

	} else if (async) {
		gcr_import_interaction_supplement_async (GCR_IMPORT_INTERACTION (self->interaction),
		                                         data->supplement, cancellable,
		                                         on_supplement_done,
		                                         g_object_ref (task));

	} else {
		gcr_import_interaction_supplement (GCR_IMPORT_INTERACTION (self->interaction),
		                                   data->supplement, cancellable, &error);
		complete_supplement (task, error);
	}
}

static void
supplement_prep (GTask *task)
{
	GcrImporterData *data = g_task_get_task_data (task);
	GcrPkcs11Importer *self = data->importer;
	const GckAttribute *the_label = NULL;
	const GckAttribute *attr;
	gboolean first = TRUE;
	GList *l;

	if (data->supplement)
		gck_builder_unref (data->supplement);
	data->supplement = gck_builder_new (GCK_BUILDER_NONE);

	/* Do we have a consistent label across all objects? */
	for (l = self->queue->head; l != NULL; l = g_list_next (l)) {
		attr = gck_attributes_find (l->data, CKA_LABEL);
		if (first)
			the_label = attr;
		else if (!gck_attribute_equal (the_label, attr))
			the_label = NULL;
		first = FALSE;
	}

	/* If consistent label, set that in supplement data */
	if (the_label != NULL)
		gck_builder_add_data (data->supplement, CKA_LABEL, the_label->value, the_label->length);
	else
		gck_builder_add_empty (data->supplement, CKA_LABEL);

	if (GCR_IS_IMPORT_INTERACTION (self->interaction))
		gcr_import_interaction_supplement_prep (GCR_IMPORT_INTERACTION (self->interaction),
		                                        data->supplement);
}

/* ---------------------------------------------------------------------------------
 * OPEN SESSION
 */

static void
complete_open_session (GTask *task,
                       GckSession *session,
                       GError *error)
{
	GcrImporterData *data = g_task_get_task_data (task);
	GcrPkcs11Importer *self = data->importer;

	if (!session) {
		g_task_return_error (task, g_steal_pointer (&error));

	} else {
		g_clear_object (&self->session);
		self->session = session;
		next_state (task, state_supplement);
	}
}

static void
on_open_session (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GError *error = NULL;
	GckSession *session;

	session = gck_session_open_finish (result, &error);
	complete_open_session (task, session, error);
	g_clear_object (&task);
}

static void
state_open_session (GTask *task,
                    gboolean async)
{
	GcrImporterData *data = g_task_get_task_data (task);
	GCancellable *cancellable = g_task_get_cancellable (task);
	GcrPkcs11Importer *self = data->importer;
	guint options = GCK_SESSION_READ_WRITE | GCK_SESSION_LOGIN_USER;
	GckSession *session;
	GError *error = NULL;

	if (async) {
		gck_session_open_async (self->slot, options, self->interaction,
		                        cancellable, on_open_session, g_object_ref (task));
	} else {
		session = gck_session_open (self->slot, options, self->interaction,
		                            cancellable, &error);
		complete_open_session (task, session, error);
	}
}

static void
_gcr_pkcs11_importer_init (GcrPkcs11Importer *self)
{
	self->queue = g_queue_new ();
}

static void
_gcr_pkcs11_importer_dispose (GObject *obj)
{
	GcrPkcs11Importer *self = GCR_PKCS11_IMPORTER (obj);

	g_clear_list (&self->objects, g_object_unref);
	g_clear_object (&self->session);
	g_clear_object (&self->interaction);

	while (!g_queue_is_empty (self->queue))
		gck_attributes_unref (g_queue_pop_head (self->queue));

	G_OBJECT_CLASS (_gcr_pkcs11_importer_parent_class)->dispose (obj);
}

static void
_gcr_pkcs11_importer_finalize (GObject *obj)
{
	GcrPkcs11Importer *self = GCR_PKCS11_IMPORTER (obj);

	g_queue_free (self->queue);
	g_clear_object (&self->slot);

	G_OBJECT_CLASS (_gcr_pkcs11_importer_parent_class)->finalize (obj);
}

static void
_gcr_pkcs11_importer_set_property (GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	GcrPkcs11Importer *self = GCR_PKCS11_IMPORTER (obj);

	switch (prop_id) {
	case PROP_SLOT:
		self->slot = g_value_dup_object (value);
		g_return_if_fail (self->slot);
		break;
	case PROP_INTERACTION:
		g_clear_object (&self->interaction);
		self->interaction = g_value_dup_object (value);
		g_object_notify (G_OBJECT (self), "interaction");
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static gchar *
calculate_label (GcrPkcs11Importer *self)
{
	GckTokenInfo *info;
	gchar *result;

	info = gck_slot_get_token_info (self->slot);
	result = g_strdup (info->label);
	gck_token_info_free (info);

	return result;
}

static gchar *
calculate_uri (GcrPkcs11Importer *self)
{
	GckUriData *data;
	gchar *uri;

	data = gck_uri_data_new ();
	data->token_info = gck_slot_get_token_info (self->slot);
	uri = gck_uri_data_build (data, GCK_URI_FOR_TOKEN);
	data->token_info = NULL;
	gck_uri_data_free (data);

	return uri;
}

static void
_gcr_pkcs11_importer_get_property (GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	GcrPkcs11Importer *self = GCR_PKCS11_IMPORTER (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_take_string (value, calculate_label (self));
		break;
	case PROP_SLOT:
		g_value_set_object (value, _gcr_pkcs11_importer_get_slot (self));
		break;
	case PROP_QUEUED:
		g_value_set_pointer (value, _gcr_pkcs11_importer_get_queued (self));
		break;
	case PROP_INTERACTION:
		g_value_set_object (value, self->interaction);
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
_gcr_pkcs11_importer_class_init (GcrPkcs11ImporterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GckBuilder builder = GCK_BUILDER_INIT;

	gobject_class->dispose = _gcr_pkcs11_importer_dispose;
	gobject_class->finalize = _gcr_pkcs11_importer_finalize;
	gobject_class->set_property = _gcr_pkcs11_importer_set_property;
	gobject_class->get_property = _gcr_pkcs11_importer_get_property;

	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_INTERACTION, "interaction");
	g_object_class_override_property (gobject_class, PROP_URI, "uri");

	g_object_class_install_property (gobject_class, PROP_SLOT,
		g_param_spec_object ("slot", "Slot", "PKCS#11 slot to import data into",
		                     GCK_TYPE_SLOT,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_QUEUED,
		g_param_spec_pointer ("queued", "Queued", "Queued attributes",
		                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_CERTIFICATE);
	gck_builder_add_ulong (&builder, CKA_CERTIFICATE_TYPE, CKC_X_509);
	gcr_importer_register (GCR_TYPE_PKCS11_IMPORTER, gck_builder_end (&builder));

	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_PRIVATE_KEY);
	gcr_importer_register (GCR_TYPE_PKCS11_IMPORTER, gck_builder_end (&builder));

	_gcr_initialize_library ();
}

static GList *
list_all_slots (void)
{
	GList *modules;
	GList *results;

	modules = gcr_pkcs11_get_modules ();
	results = gck_modules_get_slots (modules, TRUE);
	g_clear_list (&modules, g_object_unref);

	return results;
}

static const char *token_blacklist[] = {
	"pkcs11:manufacturer=Gnome%20Keyring;serial=1:SECRET:MAIN",
	"pkcs11:manufacturer=Gnome%20Keyring;serial=1%3aXDG%3aDEFAULT",
	NULL
};

static gboolean
is_slot_importable (GckSlot *slot,
                    GckTokenInfo *token)
{
	GError *error = NULL;
	GckUriData *uri;
	gboolean match;
	guint i;

	if (token->flags & CKF_WRITE_PROTECTED) {
		g_debug ("token is not importable: %s: write protected", token->label);
		return FALSE;
	}
	if (!(token->flags & CKF_TOKEN_INITIALIZED)) {
		g_debug ("token is not importable: %s: not initialized", token->label);
		return FALSE;
	}
	if ((token->flags & CKF_LOGIN_REQUIRED) &&
	    !(token->flags & CKF_USER_PIN_INITIALIZED)) {
		g_debug ("token is not importable: %s: user pin not initialized", token->label);
		return FALSE;
	}

	for (i = 0; token_blacklist[i] != NULL; i++) {
		uri = gck_uri_data_parse (token_blacklist[i], GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE, &error);
		if (uri == NULL) {
			g_warning ("couldn't parse pkcs11 blacklist uri: %s", error->message);
			g_clear_error (&error);
			continue;
		}

		match = gck_slot_match (slot, uri);
		gck_uri_data_free (uri);

		if (match) {
			g_debug ("token is not importable: %s: on the black list", token->label);
			return FALSE;
		}
	}

	return TRUE;
}

static GList *
_gcr_pkcs11_importer_create_for_parsed (GcrParsed *parsed)
{
	GcrImporter *self;
	GList *slots, *l;
	GList *results = NULL;
	GckTokenInfo *token_info;
	gboolean importable;

	slots = list_all_slots ();
	for (l = slots; l != NULL; l = g_list_next (l)) {
		token_info = gck_slot_get_token_info (l->data);
		importable = is_slot_importable (l->data, token_info);

		if (importable) {
			g_debug ("creating importer for token: %s", token_info->label);
			self = _gcr_pkcs11_importer_new (l->data);
			if (!gcr_importer_queue_for_parsed (self, parsed))
				g_assert_not_reached ();
			results = g_list_prepend (results, self);
		}

		gck_token_info_free (token_info);
	}
	g_clear_list (&slots, g_object_unref);

	return g_list_reverse (results);
}

static gboolean
_gcr_pkcs11_importer_queue_for_parsed (GcrImporter *importer,
                                       GcrParsed *parsed)
{
	GcrPkcs11Importer *self = GCR_PKCS11_IMPORTER (importer);
	GckAttributes *attrs;
	const gchar *label;

	attrs = gcr_parsed_get_attributes (parsed);
	label = gcr_parsed_get_label (parsed);
	_gcr_pkcs11_importer_queue (self, label, attrs);

	return TRUE;
}

static void
_gcr_pkcs11_importer_import_async (GcrImporter *importer,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	GTask *task;
	GcrImporterData *data;

	task = g_task_new (importer, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gcr_pkcs11_importer_import_async);

	data = g_new0 (GcrImporterData, 1);
	data->async = TRUE;
	data->importer = GCR_PKCS11_IMPORTER (g_object_ref (importer));
	g_task_set_task_data (task, data, gcr_importer_data_free);

	supplement_prep (task);

	next_state (task, state_open_session);
	g_clear_object (&task);
}

static gboolean
_gcr_pkcs11_importer_import_finish (GcrImporter *importer,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, importer), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
_gcr_pkcs11_importer_init_iface (GcrImporterInterface *iface)
{
	iface->create_for_parsed = _gcr_pkcs11_importer_create_for_parsed;
	iface->queue_for_parsed = _gcr_pkcs11_importer_queue_for_parsed;
	iface->import_async = _gcr_pkcs11_importer_import_async;
	iface->import_finish = _gcr_pkcs11_importer_import_finish;
}

/**
 * _gcr_pkcs11_importer_new:
 *
 * Create a new #GcrPkcs11Importer.
 *
 * Returns: (transfer full) (type Gcr.Pkcs11Importer): the new importer
 */
GcrImporter *
_gcr_pkcs11_importer_new (GckSlot *slot)
{
	g_return_val_if_fail (GCK_IS_SLOT (slot), NULL);

	return g_object_new (GCR_TYPE_PKCS11_IMPORTER,
	                     "slot", slot,
	                     NULL);
}

GckSlot *
_gcr_pkcs11_importer_get_slot (GcrPkcs11Importer *self)
{
	g_return_val_if_fail (GCR_IS_PKCS11_IMPORTER (self), NULL);
	return self->slot;
}

GList *
_gcr_pkcs11_importer_get_queued (GcrPkcs11Importer *self)
{
	g_return_val_if_fail (GCR_IS_PKCS11_IMPORTER (self), NULL);
	return g_list_copy (self->queue->head);
}

void
_gcr_pkcs11_importer_queue (GcrPkcs11Importer *self,
                            const gchar *label,
                            GckAttributes *attrs)
{
	GckBuilder builder = GCK_BUILDER_INIT;

	g_return_if_fail (GCR_IS_PKCS11_IMPORTER (self));
	g_return_if_fail (attrs != NULL);

	if (label != NULL && !gck_attributes_find (attrs, CKA_LABEL)) {
		gck_builder_add_all (&builder, attrs);
		gck_builder_add_string (&builder, CKA_LABEL, label);
		attrs = gck_builder_end (&builder);
	} else {
		gck_attributes_ref (attrs);
	}

	g_queue_push_tail (self->queue, attrs);
}
