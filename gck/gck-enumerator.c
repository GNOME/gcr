/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck-enumerator.c - the GObject PKCS#11 wrapper library

   Copyright (C) 2010, Stefan Walter

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

   Author: Stef Walter <nielsen@memberwebs.com>
*/

#include "config.h"

#include "gck.h"
#include "gck-private.h"

#include <string.h>

/**
 * GckEnumerator:
 *
 * Can be used to enumerate through PKCS#11 objects. It will automatically
 * create sessions as necessary.
 *
 * Use [func@modules_enumerate_objects] or [func@modules_enumerate_uri] to
 * create an enumerator. To get the objects, use [method@Enumerator.next] or
 * [method@Enumerator.next_async] functions.
 */

enum {
	PROP_0,
	PROP_INTERACTION,
	PROP_OBJECT_TYPE,
	PROP_CHAINED
};

typedef struct _GckEnumeratorResult {
	gulong handle;
	GckSession *session;
	GckAttributes *attrs;
} GckEnumeratorResult;

typedef struct _GckEnumeratorState GckEnumeratorState;

typedef gpointer (*GckEnumeratorFunc)     (GckEnumeratorState *args,
                                           gboolean forward);

struct _GckEnumeratorState {
	gpointer enumerator;
	GckEnumeratorState *chained;

	/* For the current call */
	gint want_objects;

	/* The state we're currently in */
	GckEnumeratorFunc handler;

	/* Input to enumerator */
	GList *modules;
	GckUriData *match;
	GckSessionOptions session_options;
	GTlsInteraction *interaction;

	/* The type of objects to create */
	GType object_type;
	gpointer object_class;
	const gulong *attr_types;
	gint attr_count;

	/* state_slots */
	GList *slots;

	/* state_slot */
	GckSlot *slot;
	GckTokenInfo *token_info;
	CK_FUNCTION_LIST_PTR funcs;

	/* state_session */
	GckSession *session;

	/* state_find */
	GQueue *found;

	/* state_results */
	GQueue *results;
};

struct _GckEnumerator {
	GObject parent;

	GMutex mutex;
	GckEnumeratorState *the_state;
	GTlsInteraction *interaction;
	GType object_type;
	GckObjectClass *object_class;
	gulong *attr_types;
	gint attr_count;
	GckEnumerator *chained;
};

G_DEFINE_TYPE (GckEnumerator, gck_enumerator, G_TYPE_OBJECT);

static gpointer state_modules        (GckEnumeratorState *args,
                                      gboolean forward);

static gpointer state_slots          (GckEnumeratorState *args,
                                      gboolean forward);

static gpointer state_slot           (GckEnumeratorState *args,
                                      gboolean forward);

static gpointer state_session        (GckEnumeratorState *args,
                                      gboolean forward);

static gpointer state_find           (GckEnumeratorState *args,
                                      gboolean forward);

static gpointer state_results        (GckEnumeratorState *args,
                                      gboolean forward);


static void
_gck_enumerator_result_free (gpointer data)
{
	GckEnumeratorResult *result = data;
	g_object_unref (result->session);
	if (result->attrs)
		gck_attributes_unref (result->attrs);
	g_free (result);
}

static gpointer
rewind_state (GckEnumeratorState *args, GckEnumeratorFunc handler)
{
	g_assert (args);
	g_assert (handler);
	g_assert (args->handler);

	while (handler != args->handler) {
		args->handler = (args->handler) (args, FALSE);
		g_assert (args->handler);
	}

	return handler;
}

static void
cleanup_state (GckEnumeratorState *args)
{
	g_assert (args);

	/* Have each state cleanup */
	rewind_state (args, state_modules);

	/* state_slots */
	g_assert (!args->slots);

	/* state_slot */
	g_assert (!args->slot);
	g_assert (!args->token_info);
	g_assert (!args->funcs);

	/* state_session */
	g_assert (!args->session);

	/* state_find */
	if (args->found) {
		g_queue_foreach (args->found, (GFunc) _gck_enumerator_result_free, NULL);
		g_queue_free (args->found);
		args->found = NULL;
	}

	/* state_results */
	if (args->results) {
		g_queue_foreach (args->results, (GFunc) _gck_enumerator_result_free, NULL);
		g_queue_free (args->results);
		args->results = NULL;
	}

	g_clear_list (&args->modules, g_object_unref);
	g_clear_object (&args->interaction);

	if (args->object_class)
		g_type_class_unref (args->object_class);
	args->object_class = NULL;
	args->object_type = 0;

	if (args->match) {
		gck_uri_data_free (args->match);
		args->match = NULL;
	}
}

static gpointer
state_modules (GckEnumeratorState *args, gboolean forward)
{
	GckModule *module;

	g_assert (args->slots == NULL);

	if (forward) {

		/* There are no more modules? */
		if (!args->modules) {
			g_debug ("no more modules, stopping enumerator");
			return NULL;
		}

		/* Pop off the current module */
		module = args->modules->data;
		g_assert (GCK_IS_MODULE (module));
		args->modules = g_list_delete_link (args->modules, args->modules);

		args->slots = gck_module_get_slots (module, TRUE);

		GckModuleInfo *info = gck_module_get_info (module);
		g_debug ("enumerating into module: %s", info->library_description);
		gck_module_info_free (info);

		g_object_unref (module);
		return state_slots;
	}

	/* Should never be asked to go backward from start state */
	g_assert_not_reached ();
}

static gpointer
state_slots (GckEnumeratorState *args, gboolean forward)
{
	GckSlot *slot;
	GckModule *module;
	GckTokenInfo *token_info;
	gboolean matched;

	g_assert (args->slot == NULL);

	/* slots to slot state */
	if (forward) {

		/* If there are no more slots go back to start state */
		if (!args->slots) {
			g_debug ("no more slots, want next module");
			return rewind_state (args, state_modules);
		}

		/* Pop the next slot off the stack */
		slot = args->slots->data;
		args->slots = g_list_delete_link (args->slots, args->slots);

		token_info = gck_slot_get_token_info (slot);
		if (!token_info) {
			g_message ("couldn't get token info for slot while enumerating");
			g_object_unref (slot);

			/* Skip over this slot to the next slot */
			return state_slots;
		}

		/* Do we have unrecognized matches? */
		if (args->match->any_unrecognized) {
			g_debug ("token uri had unrecognized, not matching any tokens");
			matched = FALSE;

		/* Are we trying to match the slot? */
		} else if (args->match->token_info) {
			/* No match? Go to next slot */
			matched = _gck_token_info_match (args->match->token_info, token_info);

			g_debug ("%s token: %s", matched ? "matched" : "did not match",
			         token_info->label);

		} else {
			g_debug ("matching all tokens: %s", token_info->label);
			matched = TRUE;
		}

		if (!matched) {
			g_object_unref (slot);
			gck_token_info_free (token_info);
			return state_slots;
		}

		module = gck_slot_get_module (slot);
		args->funcs = gck_module_get_functions (module);
		g_assert (args->funcs);
		g_object_unref (module);

		/* We have a slot */
		args->slot = slot;
		args->token_info = token_info;
		return state_slot;

	/* slots state to modules state */
	} else {
		g_clear_list (&args->slots, g_object_unref);
		return state_modules;
	}
}

static gpointer
state_slot (GckEnumeratorState *args, gboolean forward)
{
	CK_SESSION_HANDLE session;
	CK_FLAGS flags;
	CK_RV rv;

	g_assert (args->slot);
	g_assert (args->funcs);
	g_assert (args->session == NULL);

	/* slot to session state */
	if (forward) {
		flags = CKF_SERIAL_SESSION;
		if ((args->session_options & GCK_SESSION_READ_WRITE) == GCK_SESSION_READ_WRITE)
			flags |= CKF_RW_SESSION;

		rv = (args->funcs->C_OpenSession) (gck_slot_get_handle (args->slot),
		                                   flags, NULL, NULL, &session);

		if (rv != CKR_OK) {
			g_message ("couldn't open session on module while enumerating objects: %s",
			           gck_message_from_rv (rv));
			return rewind_state (args, state_slots);
		}

		g_debug ("opened %s session", flags & CKF_RW_SESSION ? "read-write" : "read-only");
		args->session = gck_session_from_handle (args->slot, session, args->session_options);
		return state_session;

	/* slot to slots state */
	} else {
		g_object_unref (args->slot);
		args->slot = NULL;
		args->funcs = NULL;

		gck_token_info_free (args->token_info);
		args->token_info = NULL;

		return state_slots;
	}
}

static gpointer
state_session (GckEnumeratorState *args, gboolean forward)
{
	CK_RV rv;

	g_assert (args->funcs);
	g_assert (args->session);
	g_assert (args->token_info);

	/* session to authenticated state */
	if (forward) {

		/* Don't want to authenticate? */
		if ((args->session_options & GCK_SESSION_LOGIN_USER) == 0) {
			g_debug ("no authentication necessary, skipping");
			return state_find;
		}

		rv = _gck_session_authenticate_token (args->funcs,
		                                      gck_session_get_handle (args->session),
		                                      args->slot, args->interaction, NULL);

		if (rv != CKR_OK)
			g_message ("couldn't authenticate when enumerating: %s", gck_message_from_rv (rv));

		/* We try to proceed anyway with the enumeration */
		return state_find;

	/* Session to slot state */
	} else {
		g_clear_object (&args->session);
		return state_slot;
	}
}

static gpointer
state_find (GckEnumeratorState *args,
            gboolean forward)
{
	CK_OBJECT_HANDLE objects[128];
	CK_SESSION_HANDLE session;
	CK_ATTRIBUTE_PTR attrs;
	CK_ULONG n_attrs, i,count;
	GckEnumeratorResult *result;
	CK_RV rv;

	/* Just go back, no logout */
	if (!forward)
		return state_session;

	/* This is where we do the actual searching */

	g_assert (args->session != NULL);
	g_assert (args->want_objects > 0);
	g_assert (args->funcs != NULL);

	if (!args->found)
		args->found = g_queue_new ();

	if (args->match->attributes) {
		attrs = _gck_attributes_commit_out (args->match->attributes, &n_attrs);
		gchar *string = gck_attributes_to_string (args->match->attributes);
		g_debug ("finding objects matching: %s", string);
		g_free (string);
	} else {
		attrs = NULL;
		n_attrs = 0;
		g_debug ("finding all objects");
	}

	session = gck_session_get_handle (args->session);
	g_return_val_if_fail (session, NULL);

	/* Get all the objects */
	rv = (args->funcs->C_FindObjectsInit) (session, attrs, n_attrs);

	if (rv == CKR_OK) {
		for(;;) {
			rv = (args->funcs->C_FindObjects) (session, objects, G_N_ELEMENTS (objects), &count);
			if (rv != CKR_OK || count == 0)
				break;

			g_debug ("matched %lu objects", count);

			for (i = 0; i < count; i++) {
				result = g_new0 (GckEnumeratorResult, 1);
				result->handle = objects[i];
				result->session = g_object_ref (args->session);
				g_queue_push_tail (args->found, result);
			}
		}

		(args->funcs->C_FindObjectsFinal) (session);
	}

	g_debug ("finding objects completed with: %s", _gck_stringize_rv (rv));
	return state_results;
}

static gpointer
state_results (GckEnumeratorState *args,
               gboolean forward)
{
	GckEnumeratorResult *result;
	GckBuilder builder;
	GckAttributes *attrs;
	CK_ATTRIBUTE_PTR template;
	CK_ULONG n_template;
	CK_SESSION_HANDLE session;
	gint count;
	CK_RV rv;
	gint i;

	g_assert (args->funcs != NULL);
	g_assert (args->object_class != NULL);
	g_assert (args->found != NULL);

	/* No cleanup, just unwind */
	if (!forward)
		return state_find;

	if (!args->results)
		args->results = g_queue_new ();

	session = gck_session_get_handle (args->session);
	g_return_val_if_fail (session, NULL);

	/* Get the attributes for want_objects */
	for (count = 0; count < args->want_objects; count++) {
		result = g_queue_pop_head (args->found);
		if (result == NULL) {
			g_debug ("wanted %d objects, have %d, looking for more",
			         args->want_objects, g_queue_get_length (args->results));
			return rewind_state (args, state_slots);
		}

		/* If no request for attributes, just go forward */
		if (args->attr_count == 0) {
			g_queue_push_tail (args->results, result);
			continue;
		}

		gck_builder_init (&builder);

		for (i = 0; i < args->attr_count; ++i)
			gck_builder_add_empty (&builder, args->attr_types[i]);

		/* Ask for attribute sizes */
		template = _gck_builder_prepare_in (&builder, &n_template);

		rv = (args->funcs->C_GetAttributeValue) (session, result->handle, template, n_template);
		if (GCK_IS_GET_ATTRIBUTE_RV_OK (rv)) {

			/* Allocate memory for each value */
			template = _gck_builder_commit_in (&builder, &n_template);

			/* Now get the actual values */
			rv = (args->funcs->C_GetAttributeValue) (session, result->handle, template, n_template);
		}

		attrs = gck_builder_end (&builder);

		if (GCK_IS_GET_ATTRIBUTE_RV_OK (rv)) {
			gchar *string = gck_attributes_to_string (attrs);
			g_debug ("retrieved attributes for object %lu: %s",
			         result->handle, string);
			g_free (string);
			result->attrs = attrs;
			g_queue_push_tail (args->results, result);

		} else {
			g_message ("couldn't retrieve attributes when enumerating: %s",
			           gck_message_from_rv (rv));
			gck_attributes_unref (attrs);
			_gck_enumerator_result_free (result);
		}
	}

	g_debug ("wanted %d objects, returned %d objects",
	         args->want_objects, g_queue_get_length (args->results));

	/* We got all the results we wanted */
	return NULL;
}

static void
gck_enumerator_init (GckEnumerator *self)
{
	g_mutex_init (&self->mutex);
	self->the_state = g_new0 (GckEnumeratorState, 1);
	self->object_type = GCK_TYPE_OBJECT;
	self->object_class = g_type_class_ref (self->object_type);
	g_assert (self->object_class);
}

static void
gck_enumerator_get_property (GObject *obj,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	GckEnumerator *self = GCK_ENUMERATOR (obj);

	switch (prop_id) {
	case PROP_INTERACTION:
		g_value_take_object (value, gck_enumerator_get_interaction (self));
		break;
	case PROP_OBJECT_TYPE:
		g_value_set_gtype (value, gck_enumerator_get_object_type (self));
		break;
	case PROP_CHAINED:
		g_value_set_object (value, gck_enumerator_get_chained (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gck_enumerator_set_property (GObject *obj,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	GckEnumerator *self = GCK_ENUMERATOR (obj);

	switch (prop_id) {
	case PROP_INTERACTION:
		gck_enumerator_set_interaction (self, g_value_get_object (value));
		break;
	case PROP_OBJECT_TYPE:
		gck_enumerator_set_object_type (self, g_value_get_gtype (value));
		break;
	case PROP_CHAINED:
		gck_enumerator_set_chained (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gck_enumerator_dispose (GObject *obj)
{
	GckEnumerator *self = GCK_ENUMERATOR (obj);

	gck_enumerator_set_interaction (self, NULL);
	gck_enumerator_set_chained (self, NULL);

	G_OBJECT_CLASS (gck_enumerator_parent_class)->dispose (obj);
}

static void
gck_enumerator_finalize (GObject *obj)
{
	GckEnumerator *self = GCK_ENUMERATOR (obj);

	g_assert (self->interaction == NULL);

	g_assert (self->the_state != NULL);
	cleanup_state (self->the_state);
	g_free (self->the_state);

	g_mutex_clear (&self->mutex);
	g_type_class_unref (self->object_class);
	g_free (self->attr_types);

	G_OBJECT_CLASS (gck_enumerator_parent_class)->finalize (obj);
}

static void
gck_enumerator_class_init (GckEnumeratorClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;

	gobject_class->get_property = gck_enumerator_get_property;
	gobject_class->set_property = gck_enumerator_set_property;
	gobject_class->dispose = gck_enumerator_dispose;
	gobject_class->finalize = gck_enumerator_finalize;

	/**
	 * GckEnumerator:interaction:
	 *
	 * Interaction object used to ask the user for pins when opening
	 * sessions. Used if the session_options of the enumerator have
	 * %GCK_SESSION_LOGIN_USER
	 */
	g_object_class_install_property (gobject_class, PROP_INTERACTION,
		g_param_spec_object ("interaction", "Interaction", "Interaction asking for pins",
		                     G_TYPE_TLS_INTERACTION,
		                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GckEnumerator:object-type: (skip)
	 *
	 * The type of objects that are created by the enumerator. Must be
	 * GckObject or derived from it.
	 */
	g_object_class_install_property (gobject_class, PROP_OBJECT_TYPE,
		g_param_spec_gtype ("object-type", "Object Type", "Type of objects created",
		                    GCK_TYPE_OBJECT,
		                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GckEnumerator:chained:
	 *
	 * Chained enumerator, which will be enumerated when this enumerator
	 * has enumerated all its objects.
	 */
	g_object_class_install_property (gobject_class, PROP_CHAINED,
		g_param_spec_object ("chained", "Chained", "Chained enumerator",
		                     GCK_TYPE_ENUMERATOR,
		                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
created_enumerator (GckUriData *uri_data,
                    const gchar *type)
{
	gchar *attrs, *uri;
	attrs = uri_data->attributes ? gck_attributes_to_string (uri_data->attributes) : NULL;
	uri = uri_data ? gck_uri_data_build (uri_data, GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE) : NULL;
	g_debug ("for = %s, tokens = %s, objects = %s", type, uri, attrs);
	g_free (attrs);
	g_free (uri);
}

GckEnumerator *
_gck_enumerator_new_for_modules (GList *modules,
                                 GckSessionOptions session_options,
                                 GckUriData *uri_data)
{
	GckEnumerator *self;
	GckEnumeratorState *state;

	self = g_object_new (GCK_TYPE_ENUMERATOR, NULL);
	state = self->the_state;

	state->session_options = session_options;

	state->modules = g_list_copy_deep (modules, (GCopyFunc) g_object_ref, NULL);
	state->slots = NULL;
	state->handler = state_modules;
	state->match = uri_data;

	created_enumerator (uri_data, "modules");
	return self;
}

GckEnumerator *
_gck_enumerator_new_for_slots (GList *slots,
                               GckSessionOptions session_options,
                               GckUriData *uri_data)
{
	GckEnumerator *self;
	GckEnumeratorState *state;

	self = g_object_new (GCK_TYPE_ENUMERATOR, NULL);
	state = self->the_state;

	state->session_options = session_options;

	state->slots = g_list_copy_deep (slots, (GCopyFunc) g_object_ref, NULL);
	state->modules = NULL;
	state->handler = state_slots;
	state->match = uri_data;

	created_enumerator (uri_data, "slots");
	return self;
}

GckEnumerator *
_gck_enumerator_new_for_session (GckSession *session,
                                 GckUriData *uri_data)
{
	GckEnumerator *self;
	GckEnumeratorState *state;
	GckModule *module;

	self = g_object_new (GCK_TYPE_ENUMERATOR, NULL);
	state = self->the_state;

	state->session = g_object_ref (session);
	state->modules = NULL;
	state->slots = NULL;
	state->handler = state_session;
	state->match = uri_data;

	state->slot = gck_session_get_slot (session);
	state->token_info = gck_slot_get_token_info (state->slot);

	module = gck_session_get_module (session);
	state->funcs = gck_module_get_functions (module);
	g_object_unref (module);

	created_enumerator (uri_data, "session");
	return self;
}

typedef struct _EnumerateNext {
	GckArguments base;
	GckEnumeratorState *state;
	gint want_objects;
} EnumerateNext;

static CK_RV
perform_enumerate_next (EnumerateNext *args)
{
	GckEnumeratorFunc handler;
	GckEnumeratorState *state;
	gint count = 0;

	g_assert (args->state);

	for (state = args->state; state != NULL; state = state->chained) {
		g_assert (state->handler);
		state->want_objects = args->want_objects - count;
		for (;;) {
			handler = (state->handler) (state, TRUE);
			if (!handler)
				break;
			state->handler = handler;
		}

		count += state->results ? g_queue_get_length (state->results) : 0;
		if (count >= args->want_objects)
			break;
	}

	/* TODO: In some modes, errors */
	return CKR_OK;
}

static void
free_enumerate_next (EnumerateNext *args)
{
	/* Should have been assigned back to enumerator */
	g_assert (!args->state);

	g_free (args);
}

/**
 * gck_enumerator_get_object_type:
 * @self: an enumerator
 *
 * Get the type of objects created by this enumerator. The type will always
 * either be #GckObject or derived from it.
 *
 * Returns: the type of objects created
 */
GType
gck_enumerator_get_object_type (GckEnumerator *self)
{
	GType result;

	g_return_val_if_fail (GCK_IS_ENUMERATOR (self), 0);

	g_mutex_lock (&self->mutex);

		result = self->object_type;

	g_mutex_unlock (&self->mutex);

	return result;
}

/**
 * gck_enumerator_set_object_type: (skip)
 * @self: an enumerator
 * @object_type: the type of objects to create
 *
 * Set the type of objects to be created by this enumerator. The type must
 * always be either #GckObject or derived from it.
 *
 * If the #GckObjectCache interface is implemented on the derived class
 * and the default_types class field is set, then the enumerator will retrieve
 * attributes for each object.
 */
void
gck_enumerator_set_object_type (GckEnumerator *self,
                                GType object_type)
{
	gck_enumerator_set_object_type_full (self, object_type, NULL, 0);
}

/**
 * gck_enumerator_set_object_type_full: (rename-to gck_enumerator_set_object_type)
 * @self: an enumerator
 * @object_type: the type of objects to create
 * @attr_types: (array length=attr_count): types of attributes to retrieve for objects
 * @attr_count: the number of attributes to retrieve
 *
 * Set the type of objects to be created by this enumerator. The type must
 * always be either #GckObject or derived from it.
 *
 * If @attr_types and @attr_count are non-NULL and non-zero respectively,
 * then the #GckObjectCache interface is expected to be implemented on the
 * derived class, then the enumerator will retrieve attributes for each object.
 */
void
gck_enumerator_set_object_type_full (GckEnumerator *self,
                                     GType object_type,
                                     const gulong *attr_types,
                                     gint attr_count)
{
	gpointer klass;

	g_return_if_fail (GCK_IS_ENUMERATOR (self));

	if (!g_type_is_a (object_type, GCK_TYPE_OBJECT)) {
		g_warning ("the object_type '%s' is not a derived type of GckObject",
		           g_type_name (object_type));
		return;
	}

	klass = g_type_class_ref (object_type);

	g_mutex_lock (&self->mutex);

		if (self->object_type)
			g_type_class_unref (self->object_class);
		self->object_type = object_type;
		self->object_class = klass;

		g_free (self->attr_types);
		self->attr_types = NULL;
		self->attr_count = 0;

		if (attr_types) {
			self->attr_types = g_memdup2 (attr_types, sizeof (gulong) * attr_count);
			self->attr_count = attr_count;
		}

	g_mutex_unlock (&self->mutex);
}

/**
 * gck_enumerator_get_chained:
 * @self: the enumerator
 *
 * Get the enumerator that will be run after all objects from this one
 * are seen.
 *
 * Returns: (transfer full) (nullable): the chained enumerator or %NULL
 */
GckEnumerator *
gck_enumerator_get_chained (GckEnumerator *self)
{
	GckEnumerator *chained = NULL;

	g_return_val_if_fail (GCK_IS_ENUMERATOR (self), NULL);

	g_mutex_lock (&self->mutex);

		if (self->chained)
			chained = g_object_ref (self->chained);

	g_mutex_unlock (&self->mutex);

	return chained;
}

/**
 * gck_enumerator_set_chained:
 * @self: the enumerator
 * @chained: (nullable): the chained enumerator or %NULL
 *
 * Set a chained enumerator that will be run after all objects from this one
 * are seen.
 */
void
gck_enumerator_set_chained (GckEnumerator *self,
                            GckEnumerator *chained)
{
	GckEnumerator *old_chained = NULL;

	g_return_if_fail (GCK_IS_ENUMERATOR (self));
	g_return_if_fail (chained == NULL || GCK_IS_ENUMERATOR (chained));

	g_mutex_lock (&self->mutex);

		old_chained = self->chained;
		if (chained)
			g_object_ref (chained);
		self->chained = chained;

	g_mutex_unlock (&self->mutex);

	if (old_chained)
		g_object_unref (old_chained);

	g_object_notify (G_OBJECT (self), "chained");
}

/**
 * gck_enumerator_get_interaction:
 * @self: the enumerator
 *
 * Get the interaction used when a pin is needed
 *
 * Returns: (transfer full) (nullable): the interaction or %NULL
 */
GTlsInteraction *
gck_enumerator_get_interaction (GckEnumerator *self)
{
	GTlsInteraction *result = NULL;

	g_return_val_if_fail (GCK_IS_ENUMERATOR (self), NULL);

	g_mutex_lock (&self->mutex);

		if (self->interaction)
			result = g_object_ref (self->interaction);

	g_mutex_unlock (&self->mutex);

	return result;
}

/**
 * gck_enumerator_set_interaction:
 * @self: the enumerator
 * @interaction: (nullable): the interaction or %NULL
 *
 * Set the interaction used when a pin is needed
 */
void
gck_enumerator_set_interaction (GckEnumerator *self,
                                GTlsInteraction *interaction)
{
	GTlsInteraction *previous = NULL;

	g_return_if_fail (GCK_IS_ENUMERATOR (self));
	g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));

	g_mutex_lock (&self->mutex);

		if (interaction != self->interaction) {
			previous = self->interaction;
			self->interaction = interaction;
			if (interaction)
				g_object_ref (interaction);
		}

	g_mutex_unlock (&self->mutex);

	g_clear_object (&previous);
	g_object_notify (G_OBJECT (self), "interaction");
}

static GckEnumeratorState *
check_out_enumerator_state (GckEnumerator *self)
{
	GckEnumeratorState *state = NULL;
	GTlsInteraction *old_interaction = NULL;
	gpointer old_object_class = NULL;
	GckEnumeratorState *chained_state = NULL;
	GckObjectCacheInterface *object_iface;
	GckEnumerator *chained;

	chained = gck_enumerator_get_chained (self);
	if (chained) {
		chained_state = check_out_enumerator_state (chained);
		g_object_unref (chained);
	}

	g_mutex_lock (&self->mutex);

		if (self->the_state) {
			state = self->the_state;
			self->the_state = NULL;

			state->enumerator = g_object_ref (self);
			g_assert (state->chained == NULL);
			state->chained = chained_state;

			old_interaction = state->interaction;
			if (self->interaction)
				state->interaction = g_object_ref (self->interaction);
			else
				state->interaction = NULL;

			old_object_class = state->object_class;

			/* Must already be holding a reference, state also holds a ref */
			state->object_type = self->object_type;
			state->object_class = g_type_class_peek (state->object_type);
			g_assert (state->object_class == self->object_class);

			object_iface = g_type_interface_peek (state->object_class,
			                                      GCK_TYPE_OBJECT_CACHE);

			if (self->attr_types) {
				state->attr_types = self->attr_types;
				state->attr_count = self->attr_count;
			} else if (object_iface && object_iface->default_types) {
				state->attr_types = object_iface->default_types;
				state->attr_count = object_iface->n_default_types;
			}

			g_type_class_ref (state->object_type);
		}

	g_mutex_unlock (&self->mutex);

	if (state == NULL)
		g_warning ("this enumerator is already running a next operation");

	/* Free these outside the lock */
	if (old_interaction)
		g_object_unref (old_interaction);
	if (old_object_class)
		g_type_class_unref (old_object_class);

	return state;
}

static void
check_in_enumerator_state (GckEnumeratorState *state)
{
	GckEnumeratorState *chained = NULL;
	GckEnumerator *self;

	g_assert (GCK_IS_ENUMERATOR (state->enumerator));
	self = state->enumerator;

	g_mutex_lock (&self->mutex);

		state->enumerator = NULL;
		g_assert (self->the_state == NULL);
		self->the_state = state;
		chained = state->chained;
		state->chained = NULL;

	g_mutex_unlock (&self->mutex);

	/* matches ref in check_in */
	g_object_unref (self);

	if (chained)
		check_in_enumerator_state (chained);
}

static GckObject *
extract_result (GckEnumeratorState *state)
{
	GckEnumeratorResult *result = NULL;
	GckModule *module;
	GckObject *object;

	g_assert (state != NULL);

	if (state->results != NULL)
		result = g_queue_pop_head (state->results);
	if (result == NULL) {
		if (state->chained)
			return extract_result (state->chained);
		return NULL;
	}

	module = gck_session_get_module (result->session);
	object = g_object_new (state->object_type,
	                       "module", module,
	                       "handle", result->handle,
	                       "session", result->session,
	                       result->attrs ? "attributes" : NULL, result->attrs,
	                       NULL);
	g_object_unref (module);

	_gck_enumerator_result_free (result);
	return object;
}

static GList *
extract_results (GckEnumeratorState *state,
                 gint *want_objects)
{
	GList *objects = NULL;
	GckObject *object;
	gint i;

	g_assert (state != NULL);
	g_assert (want_objects != NULL);

	for (i = 0; i < *want_objects; i++) {
		object = extract_result (state);
		if (object == NULL)
			break;
		objects = g_list_prepend (objects, object);
	}

	*want_objects -= i;
	return g_list_reverse (objects);
}

/**
 * gck_enumerator_next:
 * @self: The enumerator
 * @cancellable: A #GCancellable or %NULL
 * @error: A location to store an error on failure
 *
 * Get the next object in the enumerator, or %NULL if there are no more objects.
 *
 * %NULL is also returned if the function fails. Use the @error to determine
 * whether a failure occurred or not.
 *
 * Returns: (transfer full) (nullable): The next object, which must be released
 * using g_object_unref, or %NULL.
 */
GckObject *
gck_enumerator_next (GckEnumerator *self,
                     GCancellable *cancellable,
                     GError **error)
{
	EnumerateNext args = { GCK_ARGUMENTS_INIT, NULL, 0, };
	GckObject *result = NULL;

	g_return_val_if_fail (GCK_IS_ENUMERATOR (self), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	args.state = check_out_enumerator_state (self);
	g_return_val_if_fail (args.state != NULL, NULL);

	/* A result from a previous run? */
	result = extract_result (args.state);
	if (result == NULL) {
		args.want_objects = 1;

		/* Run the operation and steal away the results */
		if (_gck_call_sync (NULL, perform_enumerate_next, NULL, &args, cancellable, error))
			result = extract_result (args.state);

		args.want_objects = 0;
	}

	/* Put the state back */
	check_in_enumerator_state (args.state);

	return result;
}

/**
 * gck_enumerator_next_n:
 * @self: An enumerator
 * @max_objects: The maximum amount of objects to enumerate
 * @cancellable: A #GCancellable or %NULL
 * @error: A location to store an error on failure
 *
 * Get the next set of objects from the enumerator. The maximum number of
 * objects can be specified with @max_objects. If -1 is specified, then all
 * the remaining objects will be returned.
 *
 * %NULL is also returned if the function fails. Use the @error to determine
 * whether a failure occurred or not.
 *
 * Returns: (transfer full) (element-type Gck.Object): A list of `Gck.Object`s
 */
GList *
gck_enumerator_next_n (GckEnumerator *self,
                       gint max_objects,
                       GCancellable *cancellable,
                       GError **error)
{
	EnumerateNext args = { GCK_ARGUMENTS_INIT, NULL, 0, };
	GList *results = NULL;
	gint want_objects;

	g_return_val_if_fail (GCK_IS_ENUMERATOR (self), NULL);
	g_return_val_if_fail (max_objects == -1 || max_objects > 0, NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	/* Remove the state and own it ourselves */
	args.state = check_out_enumerator_state (self);
	g_return_val_if_fail (args.state != NULL, NULL);

	want_objects = max_objects <= 0 ? G_MAXINT : max_objects;

	/* A result from a previous run? */
	results = extract_results (args.state, &want_objects);
	if (want_objects > 0) {
		args.want_objects = want_objects;

		/* Run the operation and steal away the results */
		if (_gck_call_sync (NULL, perform_enumerate_next, NULL, &args, cancellable, error))
			results = g_list_concat (results, extract_results (args.state, &want_objects));

		args.want_objects = 0;
	}

	/* Put the state back */
	check_in_enumerator_state (args.state);

	if (results)
		g_clear_error (error);

	return results;
}

/**
 * gck_enumerator_next_async:
 * @self: An enumerator
 * @max_objects: The maximum number of objects to get
 * @cancellable: A #GCancellable or %NULL
 * @callback: Called when the result is ready
 * @user_data: Data to pass to the callback
 *
 * Get the next set of objects from the enumerator. This operation completes
 * asynchronously.The maximum number of objects can be specified with
 * @max_objects. If -1 is specified, then all the remaining objects will be
 * enumerated.
 */
void
gck_enumerator_next_async (GckEnumerator *self, gint max_objects, GCancellable *cancellable,
                           GAsyncReadyCallback callback, gpointer user_data)
{
	GckEnumeratorState *state;
	EnumerateNext *args;
	GckCall *call;

	g_return_if_fail (GCK_IS_ENUMERATOR (self));
	g_return_if_fail (max_objects == -1 || max_objects > 0);

	g_object_ref (self);

	/* Remove the state and own it ourselves */
	state = check_out_enumerator_state (self);
	g_return_if_fail (state != NULL);

	call = _gck_call_async_prep (NULL, perform_enumerate_next, NULL,
	                             sizeof (*args), free_enumerate_next);
	args = _gck_call_get_arguments (call);
	args->want_objects = max_objects <= 0 ? G_MAXINT : max_objects;

	args->state = state;
	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
	g_object_unref (self);
}

/**
 * gck_enumerator_next_finish:
 * @self: An enumerator
 * @result: The result passed to the callback
 * @error: A location to raise an error on failure.
 *
 * Complete an operation to enumerate next objects.
 *
 * %NULL is also returned if the function fails. Use the @error to determine
 * whether a failure occurred or not.
 *
 * Returns: (transfer full) (element-type Gck.Object): A list of `Gck.Object`s
 */
GList*
gck_enumerator_next_finish (GckEnumerator *self, GAsyncResult *result, GError **error)
{
	EnumerateNext *args;
	GckEnumeratorState *state;
	GList *results = NULL;
	gint want_objects;

	g_object_ref (self);

	args = _gck_call_async_result_arguments (result, EnumerateNext);
	state = args->state;
	args->state = NULL;
	want_objects = args->want_objects;
	args->want_objects = 0;

	if (_gck_call_basic_finish (result, error))
		results = extract_results (state, &want_objects);

	/* Put the state back */
	check_in_enumerator_state (state);

	g_object_unref (self);

	return results;
}
