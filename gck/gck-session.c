/* gck-session.h - the GObject PKCS#11 wrapper library

   Copyright (C) 2008, Stefan Walter

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

#include "gck/gck-marshal.h"

#include <string.h>

#include <glib/gi18n-lib.h>

/**
 * GckSession:
 *
 * Represents an open PKCS11 session.
 *
 * Before performing any PKCS11 operations, a session must be opened. This is
 * analogous to an open database handle, or a file handle.
 */

/**
 * GckSessionOptions:
 * @GCK_SESSION_READ_ONLY: Open session as read only
 * @GCK_SESSION_READ_WRITE: Open sessions as read/write
 * @GCK_SESSION_LOGIN_USER: Login as user on new sessions
 * @GCK_SESSION_AUTHENTICATE: Authenticate as necessary
 *
 * Options for creating sessions.
 */

/**
 * GckMechanism:
 * @type: The mechanism type
 * @parameter: Mechanism specific data.
 * @n_parameter: Length of mechanism specific data.
 *
 * Represents a mechanism used with crypto operations.
 */

enum {
	DISCARD_HANDLE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_MODULE,
	PROP_HANDLE,
	PROP_INTERACTION,
	PROP_SLOT,
	PROP_OPTIONS,
	PROP_OPENING_FLAGS,
	PROP_APP_DATA
};

typedef struct {
	/* Not modified after construct/init */
	GckSlot *slot;
	CK_SESSION_HANDLE handle;
	GckSessionOptions options;
	gulong opening_flags;
	gpointer app_data;

	/* Changable data locked by mutex */
	GMutex mutex;
	GTlsInteraction *interaction;
	gboolean discarded;
} GckSessionPrivate;

static void    gck_session_initable_iface        (GInitableIface *iface);

static void    gck_session_async_initable_iface  (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GckSession, gck_session, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GckSession);
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gck_session_initable_iface);
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, gck_session_async_initable_iface);
);

static guint signals[LAST_SIGNAL] = { 0 };

/* ----------------------------------------------------------------------------
 * OBJECT
 */

static gboolean
gck_session_real_discard_handle (GckSession *self, CK_OBJECT_HANDLE handle)
{
	CK_FUNCTION_LIST_PTR funcs;
	GckModule *module;
	CK_RV rv;

	/* The default functionality, close the handle */

	module = gck_session_get_module (self);
	g_return_val_if_fail (module != NULL, FALSE);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, FALSE);

	rv = (funcs->C_CloseSession) (handle);
	if (rv != CKR_OK) {
		g_warning ("couldn't close session properly: %s",
		           gck_message_from_rv (rv));
	}

	g_object_unref (module);
	return TRUE;
}

static void
gck_session_init (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	g_mutex_init (&priv->mutex);
}

static void
gck_session_get_property (GObject *obj, guint prop_id, GValue *value,
                           GParamSpec *pspec)
{
	GckSession *self = GCK_SESSION (obj);

	switch (prop_id) {
	case PROP_MODULE:
		g_value_take_object (value, gck_session_get_module (self));
		break;
	case PROP_HANDLE:
		g_value_set_ulong (value, gck_session_get_handle (self));
		break;
	case PROP_SLOT:
		g_value_take_object (value, gck_session_get_slot (self));
		break;
	case PROP_OPTIONS:
		g_value_set_uint (value, gck_session_get_options (self));
		break;
	case PROP_INTERACTION:
		g_value_take_object (value, gck_session_get_interaction (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gck_session_set_property (GObject *obj, guint prop_id, const GValue *value,
                           GParamSpec *pspec)
{
	GckSession *self = GCK_SESSION (obj);
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	/* Only valid calls are from constructor */

	switch (prop_id) {
	case PROP_HANDLE:
		g_return_if_fail (!priv->handle);
		priv->handle = g_value_get_ulong (value);
		break;
	case PROP_INTERACTION:
		gck_session_set_interaction (self, g_value_get_object (value));
		break;
	case PROP_SLOT:
		g_return_if_fail (!priv->slot);
		priv->slot = g_value_dup_object (value);
		g_return_if_fail (priv->slot);
		break;
	case PROP_OPTIONS:
		g_return_if_fail (!priv->options);
		priv->options = g_value_get_flags (value);
		break;
	case PROP_OPENING_FLAGS:
		priv->opening_flags = g_value_get_ulong (value);
		break;
	case PROP_APP_DATA:
		priv->app_data = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gck_session_constructed (GObject *obj)
{
	GckSession *self = GCK_SESSION (obj);
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	G_OBJECT_CLASS (gck_session_parent_class)->constructed (obj);

	priv->opening_flags |= CKF_SERIAL_SESSION;
	if (priv->options & GCK_SESSION_READ_WRITE)
		priv->opening_flags |= CKF_RW_SESSION;
}

static void
gck_session_dispose (GObject *obj)
{
	GckSession *self = GCK_SESSION (obj);
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	gboolean discard = FALSE;
	gboolean handled;

	g_return_if_fail (GCK_IS_SESSION (self));

	if (priv->handle != 0) {
		g_mutex_lock (&priv->mutex);
			discard = !priv->discarded;
			priv->discarded = TRUE;
		g_mutex_unlock (&priv->mutex);
	}

	if (discard) {
		/*
		 * Let the world know that we're discarding the session
		 * handle. This allows any necessary session reuse to work.
		 */

		g_signal_emit_by_name (self, "discard-handle", priv->handle, &handled);
		g_return_if_fail (handled);
	}

	G_OBJECT_CLASS (gck_session_parent_class)->dispose (obj);
}

static void
gck_session_finalize (GObject *obj)
{
	GckSession *self = GCK_SESSION (obj);
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	g_assert (priv->handle == 0 || priv->discarded);

	g_clear_object (&priv->interaction);
	g_clear_object (&priv->slot);

	g_mutex_clear (&priv->mutex);

	G_OBJECT_CLASS (gck_session_parent_class)->finalize (obj);
}

static void
gck_session_class_init (GckSessionClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	gck_session_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructed = gck_session_constructed;
	gobject_class->get_property = gck_session_get_property;
	gobject_class->set_property = gck_session_set_property;
	gobject_class->dispose = gck_session_dispose;
	gobject_class->finalize = gck_session_finalize;

	klass->discard_handle = gck_session_real_discard_handle;

	/**
	 * GckSession:module:
	 *
	 * The GckModule that this session is opened on.
	 */
	g_object_class_install_property (gobject_class, PROP_MODULE,
		g_param_spec_object ("module", "Module", "PKCS11 Module",
		                     GCK_TYPE_MODULE,
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GckSession:handle:
	 *
	 * The raw CK_SESSION_HANDLE handle of this session.
	 */
	g_object_class_install_property (gobject_class, PROP_HANDLE,
		g_param_spec_ulong ("handle", "Session Handle", "PKCS11 Session Handle",
		                    0, G_MAXULONG, 0,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GckSession:slot:
	 *
	 * The GckSlot this session is opened on.
	 */
	g_object_class_install_property (gobject_class, PROP_SLOT,
		g_param_spec_object ("slot", "Slot that this session uses", "PKCS11 Slot",
		                     GCK_TYPE_SLOT,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GckSession:options:
	 *
	 * The options this session was opened with.
	 */
	g_object_class_install_property (gobject_class, PROP_OPTIONS,
		g_param_spec_flags ("options", "Session Options", "Session Options",
		                    GCK_TYPE_SESSION_OPTIONS, GCK_SESSION_READ_ONLY,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GckSession:interaction:
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
	 * GckSession:opening-flags:
	 *
	 * Raw PKCS#11 flags used to open the PKCS#11 session.
	 */
	g_object_class_install_property (gobject_class, PROP_OPENING_FLAGS,
		g_param_spec_ulong ("opening-flags", "Opening flags", "PKCS#11 open session flags",
		                    0, G_MAXULONG, 0,
		                    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GckSession:app-data:
	 *
	 * Raw PKCS#11 application data used to open the PKCS#11 session.
	 */
	g_object_class_install_property (gobject_class, PROP_APP_DATA,
		g_param_spec_pointer ("app-data", "App data", "PKCS#11 application data",
		                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	/**
	 * GckSession::discard-handle:
	 * @session: The session.
	 * @handle: The handle being discarded.
	 *
	 * When a GckSession is being disposed of it emits this signal to allow
	 * a session pool to pick up the handle and keep it around.
	 *
	 * If no signal handler claims the handle, then it is closed.
	 *
	 * Returns: Whether or not this handle was claimed.
	 */
	signals[DISCARD_HANDLE] = g_signal_new ("discard-handle", GCK_TYPE_SESSION,
	                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GckSessionClass, discard_handle),
			g_signal_accumulator_true_handled, NULL,
			_gck_marshal_BOOLEAN__ULONG, G_TYPE_BOOLEAN, 1, G_TYPE_ULONG);
}

typedef struct OpenSession {
	GckArguments base;
	GTlsInteraction *interaction;
	GckSlot *slot;
	gulong flags;
	gpointer app_data;
	CK_NOTIFY notify;
	gboolean auto_login;
	CK_SESSION_HANDLE session;
} OpenSession;

static void
free_open_session (OpenSession *args)
{
	g_clear_object (&args->interaction);
	g_clear_object (&args->slot);
	g_free (args);
}

static CK_RV
perform_open_session (OpenSession *args)
{
	CK_RV rv = CKR_OK;

	/* First step, open session */
	if (!args->session) {
		rv = (args->base.pkcs11->C_OpenSession) (args->base.handle, args->flags,
		                                         args->app_data, args->notify, &args->session);
	}

	if (rv != CKR_OK || !args->auto_login)
		return rv;

	rv = _gck_session_authenticate_token (args->base.pkcs11, args->session,
	                                      args->slot, args->interaction, NULL);

	return rv;
}

static gboolean
gck_session_initable_init (GInitable *initable,
                           GCancellable *cancellable,
                           GError **error)
{
	GckSession *self = GCK_SESSION (initable);
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	OpenSession args = { GCK_ARGUMENTS_INIT, 0,  };
	GckModule *module = NULL;
	gboolean ret = FALSE;
	gboolean want_login;

	want_login = (priv->options & GCK_SESSION_LOGIN_USER) == GCK_SESSION_LOGIN_USER;

	/* Already have a session setup? */
	if (priv->handle && !want_login)
		return TRUE;

	g_object_ref (self);
	module = gck_session_get_module (self);

	/* Open a new session */
	args.slot = priv->slot;
	args.app_data = priv->app_data;
	args.notify = NULL;
	args.session = priv->handle;
	args.flags = priv->opening_flags;
	args.interaction = priv->interaction ? g_object_ref (priv->interaction) : NULL;
	args.auto_login = want_login;

	if (_gck_call_sync (priv->slot, perform_open_session, NULL, &args, cancellable, error)) {
		priv->handle = args.session;
		ret = TRUE;
	}

	g_clear_object (&args.interaction);
	g_object_unref (module);
	g_object_unref (self);

	return ret;
}

static void
gck_session_initable_iface (GInitableIface *iface)
{
	iface->init = gck_session_initable_init;
}

static void
gck_session_initable_init_async (GAsyncInitable *initable,
                                 int io_priority,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	GckSession *self = GCK_SESSION (initable);
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	OpenSession *args;
	gboolean want_login;
	GckCall *call;

	g_object_ref (self);

	call =  _gck_call_async_prep (priv->slot, perform_open_session, NULL,
	                              sizeof (*args), free_open_session);

	args = _gck_call_get_arguments (call);
	want_login = (priv->options & GCK_SESSION_LOGIN_USER) == GCK_SESSION_LOGIN_USER;
	args->session = priv->handle;

	_gck_call_async_ready (call, self, cancellable, callback, user_data);

	/* Already have a session setup? */
	if (priv->handle && !want_login) {
		_gck_call_async_short (call, CKR_OK);
		g_object_unref (self);
		return;
	}

	args->app_data = priv->app_data;
	args->notify = NULL;
	args->slot = g_object_ref (priv->slot);
	args->interaction = priv->interaction ? g_object_ref (priv->interaction) : NULL;
	args->auto_login = want_login;
	args->flags = priv->opening_flags;

	_gck_call_async_go (call);
	g_object_unref (self);
}

static gboolean
gck_session_initable_init_finish (GAsyncInitable *initable,
                                  GAsyncResult *result,
                                  GError **error)
{
	GckSession *self = GCK_SESSION (initable);
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	gboolean ret = FALSE;

	g_object_ref (self);

	{
		OpenSession *args;

		if (_gck_call_basic_finish (result, error)) {
			args = _gck_call_async_result_arguments (result, OpenSession);
			priv->handle = args->session;
			ret = TRUE;
		}
	}

	g_object_unref (self);

	return ret;
}

static void
gck_session_async_initable_iface (GAsyncInitableIface *iface)
{
	iface->init_async = gck_session_initable_init_async;
	iface->init_finish = gck_session_initable_init_finish;
}

/* ----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * GckSessionInfo:
 * @slot_id: The handle of the PKCS11 slot that this session is opened on.
 * @state: The user login state of the session.
 * @flags: Various PKCS11 flags.
 * @device_error: The last device error that occurred from an operation on this session.
 *
 * Information about the session. This is analogous to a CK_SESSION_INFO structure.
 *
 * When done with this structure, release it using gck_session_info_free().
 */

G_DEFINE_BOXED_TYPE (GckSessionInfo, gck_session_info,
                     gck_session_info_copy, gck_session_info_free)

/**
 * gck_session_info_copy:
 * @session_info: a session info structure
 *
 * Make a new copy of a session info structure.
 *
 * Returns: (transfer full): a new copy of the session info
 */
GckSessionInfo *
gck_session_info_copy (GckSessionInfo *session_info)
{
	return g_memdup2 (session_info, sizeof (GckSessionInfo));
}

/**
 * gck_session_info_free:
 * @session_info: Session info to free.
 *
 * Free the GckSessionInfo structure and all associated memory.
 **/
void
gck_session_info_free (GckSessionInfo *session_info)
{
	if (!session_info)
		return;
	g_free (session_info);
}

/**
 * gck_session_from_handle:
 * @slot: The slot which the session belongs to.
 * @session_handle: the raw PKCS#11 handle of the session
 * @options: Session options. Those which are used during opening a session have no effect.
 *
 * Initialize a session object from a raw PKCS#11 session handle.
 * Usually one would use the [method@Slot.open_session] function to
 * create a session.
 *
 * Returns: (transfer full): the new GckSession object
 **/
GckSession *
gck_session_from_handle (GckSlot *slot,
                         gulong session_handle,
                         GckSessionOptions options)
{
	GckSession *session;

	g_return_val_if_fail (GCK_IS_SLOT (slot), NULL);

	session = g_object_new (GCK_TYPE_SESSION,
	                        "handle", session_handle,
	                        "slot", slot,
	                        "options", options,
	                        NULL);

	return session;
}

/**
 * gck_session_get_handle:
 * @self: The session object.
 *
 * Get the raw PKCS#11 session handle from a session object.
 *
 * Return value: The raw session handle.
 **/
gulong
gck_session_get_handle (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_SESSION (self), (CK_SESSION_HANDLE)-1);

	return priv->handle;
}

/**
 * gck_session_get_module:
 * @self: The session object.
 *
 * Get the PKCS#11 module to which this session belongs.
 *
 * Returns: (transfer full): the module, which should be unreffed after use
 **/
GckModule *
gck_session_get_module (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);

	return gck_slot_get_module (priv->slot);
}

/**
 * gck_session_get_slot:
 * @self: The session object.
 *
 * Get the PKCS#11 slot to which this session belongs.
 *
 * Return value: (transfer full): The slot, which should be unreffed after use.
 **/
GckSlot *
gck_session_get_slot (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);
	g_return_val_if_fail (GCK_IS_SLOT (priv->slot), NULL);

	return g_object_ref (priv->slot);
}

/**
 * gck_session_get_info:
 * @self: The session object.
 *
 * Get information about the session.
 *
 * Returns: (transfer full): the session info. Use the gck_session_info_free()
 *          to release when done
 **/
GckSessionInfo*
gck_session_get_info (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	GckSessionInfo *sessioninfo;
	CK_FUNCTION_LIST_PTR funcs;
	CK_SESSION_INFO info;
	GckModule *module;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);

	module = gck_session_get_module (self);
	g_return_val_if_fail (GCK_IS_MODULE (module), NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, NULL);

	memset (&info, 0, sizeof (info));
	rv = (funcs->C_GetSessionInfo) (priv->handle, &info);

	g_object_unref (module);

	if (rv != CKR_OK) {
		g_warning ("couldn't get session info: %s", gck_message_from_rv (rv));
		return NULL;
	}

	sessioninfo = g_new0 (GckSessionInfo, 1);
	sessioninfo->flags = info.flags;
	sessioninfo->slot_id = info.slotID;
	sessioninfo->state = info.state;
	sessioninfo->device_error = info.ulDeviceError;

	return sessioninfo;
}

/**
 * gck_session_get_state:
 * @self: the session
 *
 * Get the session state. The state is the various PKCS#11 CKS_XXX flags.
 *
 * Returns: the session state
 */
gulong
gck_session_get_state (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	CK_FUNCTION_LIST_PTR funcs;
	CK_SESSION_INFO info;
	GckModule *module;
	CK_RV rv;

	g_return_val_if_fail (GCK_IS_SESSION (self), 0);

	module = gck_session_get_module (self);
	g_return_val_if_fail (GCK_IS_MODULE (module), 0);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (funcs, 0);

	memset (&info, 0, sizeof (info));
	rv = (funcs->C_GetSessionInfo) (priv->handle, &info);

	g_object_unref (module);

	if (rv != CKR_OK) {
		g_warning ("couldn't get session info: %s", gck_message_from_rv (rv));
		return 0;
	}

	return info.state;
}

/**
 * gck_session_get_options:
 * @self: The session to get options from.
 *
 * Get the options this session was opened with.
 *
 * Return value: The session options.
 **/
GckSessionOptions
gck_session_get_options (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_SESSION (self), 0);

	return priv->options;
}

/**
 * gck_session_get_interaction:
 * @self: the session
 *
 * Get the interaction object set on this session, which is used to prompt
 * for pins and the like.
 *
 * Returns: (transfer full) (nullable): the interaction object, or %NULL
 */
GTlsInteraction *
gck_session_get_interaction (GckSession *self)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);

	if (priv->interaction)
		return g_object_ref (priv->interaction);

	return NULL;
}

/**
 * gck_session_set_interaction:
 * @self: the session
 * @interaction: (nullable): the interaction or %NULL
 *
 * Set the interaction object on this session, which is used to prompt for
 * pins and the like.
 */
void
gck_session_set_interaction (GckSession *self,
                             GTlsInteraction *interaction)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);

	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));

	g_mutex_lock (&priv->mutex);
	g_set_object (&priv->interaction, interaction);
	g_mutex_unlock (&priv->mutex);
}

/**
 * gck_session_open:
 * @slot: the slot to open session on
 * @options: session options
 * @interaction: (nullable): optional interaction for logins or object authentication
 * @cancellable: (nullable): optional cancellation object
 * @error: location to place error or %NULL
 *
 * Open a session on the slot. This call may block for an indefinite period.
 *
 * Returns: (transfer full): the new session
 */
GckSession *
gck_session_open (GckSlot *slot,
                  GckSessionOptions options,
                  GTlsInteraction *interaction,
                  GCancellable *cancellable,
                  GError **error)
{
	return g_initable_new (GCK_TYPE_SESSION, cancellable, error,
	                       "slot", slot,
	                       "interaction", interaction,
	                       "options", options,
	                       NULL);
}

/**
 * gck_session_open_async:
 * @slot: the slot to open session on
 * @options: session options
 * @interaction: (nullable): optional interaction for logins or object authentication
 * @cancellable: optional cancellation object
 * @callback: called when the operation completes
 * @user_data: data to pass to callback
 *
 * Open a session on the slot. This call will return immediately and complete
 * asynchronously.
 */
void
gck_session_open_async (GckSlot *slot,
                        GckSessionOptions options,
                        GTlsInteraction *interaction,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
	g_async_initable_new_async (GCK_TYPE_SESSION, G_PRIORITY_DEFAULT,
	                            cancellable, callback, user_data,
	                            "slot", slot,
	                            "interaction", interaction,
	                            "options", options,
	                            NULL);
}

/**
 * gck_session_open_finish:
 * @result: the result passed to the callback
 * @error: location to return an error or %NULL
 *
 * Get the result of an open session operation.
 *
 * Returns: (transfer full): the new session
 */
GckSession *
gck_session_open_finish (GAsyncResult *result,
                         GError **error)
{
	GObject *ret;
	GObject *source;

	source = g_async_result_get_source_object (result);
	ret = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);
	g_object_unref (source);

	return ret ? GCK_SESSION (ret) : NULL;
}

/* ---------------------------------------------------------------------------------------------
 * INIT PIN
 */

typedef struct _InitPin {
	GckArguments base;
	guchar *pin;
	gsize n_pin;
} InitPin;


static void
free_init_pin (InitPin *args)
{
	g_free (args->pin);
	g_free (args);
}

static CK_RV
perform_init_pin (InitPin *args)
{
	return (args->base.pkcs11->C_InitPIN) (args->base.handle, (CK_BYTE_PTR)args->pin,
	                                       args->n_pin);
}

/**
 * gck_session_init_pin:
 * @self: Initialize PIN for this session's slot.
 * @pin: (nullable) (array length=n_pin): the user's PIN, or %NULL for
 *       protected authentication path
 * @n_pin: the length of the PIN
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error.
 *
 * Initialize the user's pin on this slot that this session is opened on.
 * According to the PKCS#11 standards, the session must be logged in with
 * the CKU_SO user type.
 *
 * This call may block for an indefinite period.
 *
 * Return value: Whether successful or not.
 **/
gboolean
gck_session_init_pin (GckSession *self, const guchar *pin, gsize n_pin,
                      GCancellable *cancellable, GError **error)
{
	InitPin args = { GCK_ARGUMENTS_INIT, (guchar*)pin, n_pin };
	return _gck_call_sync (self, perform_init_pin, NULL, &args, cancellable, error);

}

/**
 * gck_session_init_pin_async:
 * @self: Initialize PIN for this session's slot.
 * @pin: (nullable) (array length=n_pin): the user's PIN, or %NULL for protected authentication path
 * @n_pin: the length of the PIN
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Initialize the user's pin on this slot that this session is opened on.
 * According to the PKCS#11 standards, the session must be logged in with
 * the `CKU_SO` user type.
 *
 * This call will return immediately and completes asynchronously.
 **/
void
gck_session_init_pin_async (GckSession *self, const guchar *pin, gsize n_pin,
                             GCancellable *cancellable, GAsyncReadyCallback callback,
                             gpointer user_data)
{
	GckCall *call;
	InitPin* args;

	call = _gck_call_async_prep (self, perform_init_pin, NULL, sizeof (*args), free_init_pin);
	args = _gck_call_get_arguments (call);

	args->pin = pin && n_pin ? g_memdup2 (pin, n_pin) : NULL;
	args->n_pin = n_pin;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_init_pin_finish:
 * @self: The session.
 * @result: The result passed to the callback.
 * @error: A location to return an error.
 *
 * Get the result of initializing a user's PIN.
 *
 * Return value: Whether the operation was successful or not.
 **/
gboolean
gck_session_init_pin_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	return _gck_call_basic_finish (result, error);
}


/* ---------------------------------------------------------------------------------------------
 * SET PIN
 */

typedef struct _SetPin {
	GckArguments base;
	guchar *old_pin;
	gsize n_old_pin;
	guchar *new_pin;
	gsize n_new_pin;
} SetPin;

static void
free_set_pin (SetPin *args)
{
	g_free (args->old_pin);
	g_free (args->new_pin);
	g_free (args);
}

static CK_RV
perform_set_pin (SetPin *args)
{
	return (args->base.pkcs11->C_SetPIN) (args->base.handle, (CK_BYTE_PTR)args->old_pin,
	                                      args->n_old_pin, args->new_pin, args->n_new_pin);
}

/**
 * gck_session_set_pin:
 * @self: Change the PIN for this session's slot.
 * @old_pin: (nullable) (array length=n_old_pin): the user's old PIN, or %NULL
 *           for protected authentication path.
 * @n_old_pin: The length of the PIN.
 * @new_pin: (nullable) (array length=n_new_pin): the user's new PIN, or %NULL
 *           for protected authentication path
 * @n_new_pin: The length of the PIN.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error.
 *
 * Change the user's pin on this slot that this session is opened on.
 *
 * This call may block for an indefinite period.
 *
 * Return value: Whether successful or not.
 **/
gboolean
gck_session_set_pin (GckSession *self, const guchar *old_pin, gsize n_old_pin,
                     const guchar *new_pin, gsize n_new_pin, GCancellable *cancellable,
                     GError **error)
{
	SetPin args = { GCK_ARGUMENTS_INIT, (guchar*)old_pin, n_old_pin, (guchar*)new_pin, n_new_pin };
	return _gck_call_sync (self, perform_set_pin, NULL, &args, cancellable, error);
}

/**
 * gck_session_set_pin_async:
 * @self: Change the PIN for this session's slot.
 * @old_pin: (nullable) (array length=n_new_pin): the user's old PIN, or %NULL
 *           for protected authentication path
 * @n_old_pin: the length of the old PIN
 * @new_pin: (nullable) (array length=n_new_pin): the user's new PIN, or %NULL
 *           for protected authentication path
 * @n_new_pin: the length of the new PIN
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Change the user's pin on this slot that this session is opened on.
 *
 * This call will return immediately and completes asynchronously.
 **/
void
gck_session_set_pin_async (GckSession *self, const guchar *old_pin, gsize n_old_pin,
                            const guchar *new_pin, gsize n_new_pin, GCancellable *cancellable,
                            GAsyncReadyCallback callback, gpointer user_data)
{
	SetPin* args;
	GckCall *call;

	call = _gck_call_async_prep (self, perform_set_pin, NULL, sizeof (*args), free_set_pin);
	args = _gck_call_get_arguments (call);

	args->old_pin = old_pin && n_old_pin ? g_memdup2 (old_pin, n_old_pin) : NULL;
	args->n_old_pin = n_old_pin;
	args->new_pin = new_pin && n_new_pin ? g_memdup2 (new_pin, n_new_pin) : NULL;
	args->n_new_pin = n_new_pin;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_set_pin_finish:
 * @self: The session.
 * @result: The result passed to the callback.
 * @error: A location to return an error.
 *
 * Get the result of changing a user's PIN.
 *
 * Return value: Whether the operation was successful or not.
 **/
gboolean
gck_session_set_pin_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	return _gck_call_basic_finish (result, error);
}


/* ---------------------------------------------------------------------------------------------
 * LOGIN
 */

typedef struct _Login {
	GckArguments base;
	gulong user_type;
	guchar *pin;
	gsize n_pin;
} Login;

static void
free_login (Login *args)
{
	g_free (args->pin);
	g_free (args);
}

static CK_RV
perform_login (Login *args)
{
	return (args->base.pkcs11->C_Login) (args->base.handle, args->user_type,
	                                     (CK_BYTE_PTR)args->pin, args->n_pin);
}

/**
 * gck_session_login:
 * @self: Log in to this session.
 * @user_type: The type of login user.
 * @pin: (nullable) (array length=n_pin): the user's PIN, or %NULL for
 *       protected authentication path
 * @n_pin: The length of the PIN.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error.
 *
 * Login the user on the session. This call may block for
 * an indefinite period.
 *
 * Return value: Whether successful or not.
 **/
gboolean
gck_session_login (GckSession *self, gulong user_type, const guchar *pin,
                   gsize n_pin, GCancellable *cancellable, GError **error)
{
	Login args = { GCK_ARGUMENTS_INIT, user_type, (guchar*)pin, n_pin };
	return _gck_call_sync (self, perform_login, NULL, &args, cancellable, error);

}

/**
 * gck_session_login_async:
 * @self: Log in to this session.
 * @user_type: The type of login user.
 * @pin: (nullable) (array length=n_pin): the user's PIN, or %NULL for
 *       protected authentication path
 * @n_pin: The length of the PIN.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Login the user on the session. This call will return
 * immediately and completes asynchronously.
 **/
void
gck_session_login_async (GckSession *self, gulong user_type, const guchar *pin,
                          gsize n_pin, GCancellable *cancellable, GAsyncReadyCallback callback,
                          gpointer user_data)
{
	Login* args;
	GckCall *call;

	call = _gck_call_async_prep (self, perform_login, NULL, sizeof (*args), free_login);
	args = _gck_call_get_arguments (call);

	args->user_type = user_type;
	args->pin = pin && n_pin ? g_memdup2 (pin, n_pin) : NULL;
	args->n_pin = n_pin;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_login_finish:
 * @self: The session logged into.
 * @result: The result passed to the callback.
 * @error: A location to return an error.
 *
 * Get the result of a login operation.
 *
 * Return value: Whether the operation was successful or not.
 **/
gboolean
gck_session_login_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	return _gck_call_basic_finish (result, error);
}

typedef struct _Interactive {
	GckArguments base;
	GTlsInteraction *interaction;
	GCancellable *cancellable;
	GckSlot *token;
} Interactive;

static void
free_interactive (Interactive *args)
{
	g_clear_object (&args->token);
	g_clear_object (&args->cancellable);
	g_clear_object (&args->interaction);
	g_free (args);
}

static CK_RV
perform_interactive (Interactive *args)
{
	return _gck_session_authenticate_token (args->base.pkcs11, args->base.handle,
	                                        args->token, args->interaction, args->cancellable);
}

/**
 * gck_session_login_interactive:
 * @self: session to use for login
 * @user_type: the type of login user
 * @interaction: (nullable): interaction to request PIN when necessary
 * @cancellable: (nullable): optional cancellation object, or %NULL
 * @error: location to return an error
 *
 * Login the user on the session requesting the password interactively
 * when necessary. This call may block for an indefinite period.
 *
 * Return value: Whether successful or not.
 */
gboolean
gck_session_login_interactive (GckSession *self,
                               gulong user_type,
                               GTlsInteraction *interaction,
                               GCancellable *cancellable,
                               GError **error)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	Interactive args = { GCK_ARGUMENTS_INIT, interaction, cancellable, NULL, };

	g_return_val_if_fail (GCK_IS_SESSION (self), FALSE);
	g_return_val_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* TODO: For now this is all we support */
	g_return_val_if_fail (user_type == CKU_USER, FALSE);

	args.token = priv->slot;

	return _gck_call_sync (self, perform_interactive, NULL, &args, cancellable, error);
}

/**
 * gck_session_login_interactive_async:
 * @self: session to use for login
 * @user_type: the type of login user
 * @interaction: (nullable): interaction to request PIN when necessary
 * @cancellable: (nullable): optional cancellation object, or %NULL
 * @callback: called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Login the user on the session prompting for passwords interactively when
 * necessary. This call will return immediately and completes asynchronously.
 **/
void
gck_session_login_interactive_async (GckSession *self,
                                     gulong user_type,
                                     GTlsInteraction *interaction,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
	GckSessionPrivate *priv = gck_session_get_instance_private (self);
	Interactive* args;
	GckCall *call;

	call = _gck_call_async_prep (self, perform_interactive, NULL, sizeof (*args), free_interactive);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	/* TODO: For now this is all we support */
	g_return_if_fail (user_type == CKU_USER);

	args->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	args->interaction = interaction ? g_object_ref (interaction) : NULL;
	args->token = g_object_ref (priv->slot);

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_login_interactive_finish:
 * @self: the session logged into
 * @result: the result passed to the callback
 * @error: location to return an error
 *
 * Get the result of a login operation.
 *
 * Return value: Whether the operation was successful or not.
 **/
gboolean
gck_session_login_interactive_finish (GckSession *self,
                                      GAsyncResult *result,
                                      GError **error)
{
	g_return_val_if_fail (GCK_IS_SESSION (self), FALSE);

	return _gck_call_basic_finish (result, error);
}

/* LOGOUT */

static CK_RV
perform_logout (GckArguments *args)
{
	return (args->pkcs11->C_Logout) (args->handle);
}

/**
 * gck_session_logout:
 * @self: Logout of this session.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error.
 *
 * Log out of the session. This call may block for an indefinite period.
 *
 * Return value: Whether the logout was successful or not.
 **/
gboolean
gck_session_logout (GckSession *self, GCancellable *cancellable, GError **error)
{
	GckArguments args = GCK_ARGUMENTS_INIT;
	return _gck_call_sync (self, perform_logout, NULL, &args, cancellable, error);
}

/**
 * gck_session_logout_async:
 * @self: Logout of this session.
 * @cancellable: Optional cancellation object, or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Log out of the session. This call returns immediately and completes
 * asynchronously.
 **/
void
gck_session_logout_async (GckSession *self, GCancellable *cancellable,
                           GAsyncReadyCallback callback, gpointer user_data)
{
	GckCall *call;

	call = _gck_call_async_prep (self, perform_logout, NULL, 0, NULL);
	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_logout_finish:
 * @self: Logout of this session.
 * @result: The result passed to the callback.
 * @error: A location to return an error.
 *
 * Get the result of logging out of a session.
 *
 * Return value: Whether the logout was successful or not.
 **/
gboolean
gck_session_logout_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	return _gck_call_basic_finish (result, error);
}


/* CREATE OBJECT */

typedef struct _CreateObject {
	GckArguments base;
	GckAttributes *attrs;
	CK_OBJECT_HANDLE object;
} CreateObject;

static void
free_create_object (CreateObject *args)
{
	gck_attributes_unref (args->attrs);
	g_free (args);
}

static CK_RV
perform_create_object (CreateObject *args)
{
	CK_ATTRIBUTE_PTR attrs;
	CK_ULONG n_attrs;
	CK_RV rv;

	attrs = _gck_attributes_commit_out (args->attrs, &n_attrs);

	rv = (args->base.pkcs11->C_CreateObject) (args->base.handle,
	                                          attrs, n_attrs,
	                                          &args->object);

	gchar *string = gck_attributes_to_string (args->attrs);
	if (rv == CKR_OK)
		g_debug ("created object: %s", string);
	else
		g_debug ("failed %s to create object: %s",
		         _gck_stringize_rv (rv), string);
	g_free (string);

	return rv;
}

/**
 * gck_session_create_object:
 * @self: The session to create the object on.
 * @attrs: The attributes to create the object with.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Create a new PKCS#11 object. This call may block for an
 * indefinite period.
 *
 * Returns: (transfer full): the newly created object or %NULL if an error occurred
 **/
GckObject *
gck_session_create_object (GckSession *self, GckAttributes *attrs,
                           GCancellable *cancellable, GError **error)
{
	CreateObject args = { GCK_ARGUMENTS_INIT, attrs, 0 };
	gboolean ret;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);
	g_return_val_if_fail (attrs != NULL, NULL);

	ret = _gck_call_sync (self, perform_create_object, NULL, &args, cancellable, error);

	if (!ret)
		return NULL;

	return gck_object_from_handle (self, args.object);
}

/**
 * gck_session_create_object_async:
 * @self: The session to create the object on.
 * @attrs: The attributes to create the object with.
 * @cancellable: (nullable): Optional cancellation object or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Create a new PKCS#11 object. This call will return immediately
 * and complete asynchronously.
 *
 * If @attrs is a floating reference, it is consumed.
 **/
void
gck_session_create_object_async (GckSession *self, GckAttributes *attrs,
                                  GCancellable *cancellable, GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GckCall *call;
	CreateObject *args;

	call = _gck_call_async_prep (self, perform_create_object,
	                              NULL, sizeof (*args), free_create_object);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (attrs);

	args->attrs = gck_attributes_ref (attrs);

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_create_object_finish:
 * @self: The session to create the object on.
 * @result: The result passed to the callback.
 * @error: A location to return an error, or %NULL.
 *
 * Get the result of creating a new PKCS#11 object.
 *
 * Return value: (transfer full): the newly created object or %NULL if an error occurred
 **/
GckObject *
gck_session_create_object_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	CreateObject *args;

	args = _gck_call_async_result_arguments (result, CreateObject);

	if (!_gck_call_basic_finish (result, error))
		return NULL;
	return gck_object_from_handle (self, args->object);
}


/* FIND OBJECTS */

typedef struct _FindObjects {
	GckArguments base;
	GckAttributes *attrs;
	CK_OBJECT_HANDLE_PTR objects;
	CK_ULONG n_objects;
} FindObjects;

static void
free_find_objects (FindObjects *args)
{
	gck_attributes_unref (args->attrs);
	g_free (args->objects);
	g_free (args);
}

static CK_RV
perform_find_objects (FindObjects *args)
{
	CK_OBJECT_HANDLE_PTR batch;
	CK_ULONG n_batch, n_found;
	CK_ATTRIBUTE_PTR attrs;
	CK_ULONG n_attrs;
	GArray *array;
	CK_RV rv;

	gchar *string = gck_attributes_to_string (args->attrs);
	g_debug ("matching: %s", string);
	g_free (string);

	attrs = _gck_attributes_commit_out (args->attrs, &n_attrs);

	rv = (args->base.pkcs11->C_FindObjectsInit) (args->base.handle,
	                                             attrs, n_attrs);
	if (rv != CKR_OK)
		return rv;

	batch = NULL;
	n_found = n_batch = 4;
	array = g_array_new (0, 1, sizeof (CK_OBJECT_HANDLE));

	do {
		/*
		 * Reallocate and double in size:
		 *  - First time.
		 *  - Each time we found as many as batch
		 */

		if (n_found == n_batch) {
			n_batch *= 2;
			batch = g_realloc (batch, sizeof (CK_OBJECT_HANDLE) * n_batch);
		}

		rv = (args->base.pkcs11->C_FindObjects) (args->base.handle,
		                                         batch, n_batch, &n_found);
		if (rv != CKR_OK)
			break;

		g_array_append_vals (array, batch, n_found);

	} while (n_found > 0);

	g_free (batch);

	if (rv == CKR_OK) {
		args->n_objects = array->len;
		args->objects = (CK_OBJECT_HANDLE_PTR)g_array_free (array, FALSE);
		rv = (args->base.pkcs11->C_FindObjectsFinal) (args->base.handle);
	} else {
		args->objects = NULL;
		args->n_objects = 0;
		g_array_free (array, TRUE);
	}

	return rv;
}

/**
 * gck_session_find_handles:
 * @self: the session to find objects on
 * @match: the attributes to match against objects
 * @cancellable: (nullable): optional cancellation object or %NULL
 * @n_handles: location to return number of handles
 * @error: a location to return an error or %NULL
 *
 * Find the objects matching the passed attributes. This call may
 * block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_handles) (nullable): a list of
 *          the matching objects, which may be empty
 **/
gulong *
gck_session_find_handles (GckSession *self,
                          GckAttributes *match,
                          GCancellable *cancellable,
                          gulong *n_handles,
                          GError **error)
{
	FindObjects args = { GCK_ARGUMENTS_INIT, match, NULL, 0 };
	gulong *results = NULL;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);
	g_return_val_if_fail (match != NULL, NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (n_handles != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (_gck_call_sync (self, perform_find_objects, NULL, &args, cancellable, error)) {
		results = args.objects;
		*n_handles = args.n_objects;
		args.objects = NULL;
	}

	g_free (args.objects);
	return results;
}

/**
 * gck_session_find_handles_async:
 * @self: the session to find objects on
 * @match: the attributes to match against the objects
 * @cancellable: (nullable): optional cancellation object or %NULL
 * @callback: called when the operation completes
 * @user_data: data to pass to the callback
 *
 * Find the objects matching the passed attributes. This call will
 * return immediately and complete asynchronously.
 *
 * If @match is a floating reference, it is consumed.
 **/
void
gck_session_find_handles_async (GckSession *self,
                                GckAttributes *match,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	GckCall *call;
	FindObjects *args;

	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (match != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	call = _gck_call_async_prep (self, perform_find_objects,
	                             NULL, sizeof (*args), free_find_objects);
	args = _gck_call_get_arguments (call);
	args->attrs = gck_attributes_ref (match);
	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_find_handles_finish:
 * @self: the session
 * @result: the asynchronous result
 * @n_handles: location to store number of handles returned
 * @error: a location to return an error on failure
 *
 * Get the result of a find handles operation.
 *
 * Returns: (transfer full) (array length=n_handles) (nullable): an array of
 *          handles that matched, which may be empty, or %NULL on failure
 **/
gulong *
gck_session_find_handles_finish (GckSession *self,
                                 GAsyncResult *result,
                                 gulong *n_handles,
                                 GError **error)
{
	gulong *results = NULL;
	FindObjects *args;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);
	g_return_val_if_fail (n_handles != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	args = _gck_call_async_result_arguments (result, FindObjects);

	if (!_gck_call_basic_finish (result, error))
		return NULL;
	*n_handles = args->n_objects;
	results = args->objects;
	args->objects = NULL;
	return results;
}

/**
 * gck_session_find_objects:
 * @self: The session to find objects on.
 * @match: the attributes to match
 * @cancellable: (nullable): Optional cancellation object or %NULL.
 * @error: A location to return an error or %NULL.
 *
 * Find the objects matching the passed attributes. This call may
 * block for an indefinite period.
 *
 * If @match is a floating reference, it is consumed.
 *
 * Returns: (transfer full) (element-type Gck.Object): a list of the matching
 *          objects, which may be empty
 **/
GList *
gck_session_find_objects (GckSession *self,
                          GckAttributes *match,
                          GCancellable *cancellable,
                          GError **error)
{
	GList *results = NULL;
	gulong *handles;
	gulong n_handles;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);
	g_return_val_if_fail (match != NULL, NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	handles = gck_session_find_handles (self, match, cancellable, &n_handles, error);
	if (handles == NULL)
		return NULL;

	results = gck_objects_from_handle_array (self, handles, n_handles);
	g_free (handles);
	return results;
}

/**
 * gck_session_find_objects_async:
 * @self: The session to find objects on.
 * @match: The attributes to match.
 * @cancellable: (nullable): Optional cancellation object or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Find the objects matching the passed attributes. This call will
 * return immediately and complete asynchronously.
 *
 * If the @match #GckAttributes is floating, it is consumed.
 **/
void
gck_session_find_objects_async (GckSession *self,
                                GckAttributes *match,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (match != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	gck_session_find_handles_async (self, match, cancellable, callback, user_data);
}

/**
 * gck_session_find_objects_finish:
 * @self: The session to find objects on.
 * @result: The attributes to match.
 * @error: A location to return an error.
 *
 * Get the result of a find operation.
 *
 * Returns: (transfer full) (element-type Gck.Object): a list of the matching
 *          objects, which may be empty
 **/
GList *
gck_session_find_objects_finish (GckSession *self,
                                 GAsyncResult *result,
                                 GError **error)
{
	GList *results = NULL;
	gulong *handles;
	gulong n_handles;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	handles = gck_session_find_handles_finish (self, result, &n_handles, error);
	if (handles == NULL)
		return NULL;

	results = gck_objects_from_handle_array (self, handles, n_handles);
	g_free (handles);
	return results;

}

/**
 * gck_session_enumerate_objects:
 * @self: session to enumerate objects on
 * @match: attributes that the objects must match, or empty for all objects
 *
 * Setup an enumerator for listing matching objects available via this session.
 *
 * This call will not block but will return an enumerator immediately.
 *
 * Returns: (transfer full): a new enumerator
 **/
GckEnumerator *
gck_session_enumerate_objects (GckSession *session,
                               GckAttributes *match)
{
	GckUriData *uri_data;

	g_return_val_if_fail (match != NULL, NULL);

	uri_data = gck_uri_data_new ();
	uri_data->attributes = gck_attributes_ref (match);

	return _gck_enumerator_new_for_session (session, uri_data);
}

/* -----------------------------------------------------------------------------
 * KEY PAIR GENERATION
 */

typedef struct _GenerateKeyPair {
	GckArguments base;
	GckMechanism mechanism;
	GckAttributes *public_attrs;
	GckAttributes *private_attrs;
	CK_OBJECT_HANDLE public_key;
	CK_OBJECT_HANDLE private_key;
} GenerateKeyPair;

static void
free_generate_key_pair (GenerateKeyPair *args)
{
	g_clear_pointer (&args->public_attrs, gck_attributes_unref);
	g_clear_pointer (&args->private_attrs, gck_attributes_unref);
	g_free (args);
}

static CK_RV
perform_generate_key_pair (GenerateKeyPair *args)
{
	CK_ATTRIBUTE_PTR pub_attrs, priv_attrs;
	CK_ULONG n_pub_attrs, n_priv_attrs;

	g_assert (sizeof (CK_MECHANISM) == sizeof (GckMechanism));

	pub_attrs = _gck_attributes_commit_out (args->public_attrs, &n_pub_attrs);
	priv_attrs = _gck_attributes_commit_out (args->private_attrs, &n_priv_attrs);

	return (args->base.pkcs11->C_GenerateKeyPair) (args->base.handle,
	                                               (CK_MECHANISM_PTR)&(args->mechanism),
	                                               pub_attrs, n_pub_attrs,
	                                               priv_attrs, n_priv_attrs,
	                                               &args->public_key,
	                                               &args->private_key);
}

/**
 * gck_session_generate_key_pair:
 * @self: The session to use.
 * @mech_type: The mechanism type to use for key generation.
 * @public_attrs: Additional attributes for the generated public key.
 * @private_attrs: Additional attributes for the generated private key.
 * @public_key: (optional) (out): location to return the resulting public key
 * @private_key: (optional) (out): location to return the resulting private key.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Generate a new key pair of public and private keys. This call may block for
 * an indefinite period.
 *
 * If @public_attrs and/or @private_attrs is a floating reference, it is
 * consumed.
 *
 * Return value: %TRUE if the operation succeeded.
 **/
gboolean
gck_session_generate_key_pair (GckSession *self, gulong mech_type,
                               GckAttributes *public_attrs, GckAttributes *private_attrs,
                               GckObject **public_key, GckObject **private_key,
                               GCancellable *cancellable, GError **error)
{
	GckMechanism mech = { mech_type, NULL, 0 };
	return gck_session_generate_key_pair_full (self, &mech, public_attrs, private_attrs, public_key, private_key, cancellable, error);
}

/**
 * gck_session_generate_key_pair_full:
 * @self: The session to use.
 * @mechanism: The mechanism to use for key generation.
 * @public_attrs: Additional attributes for the generated public key.
 * @private_attrs: Additional attributes for the generated private key.
 * @public_key: (optional) (out): a location to return the resulting public key
 * @private_key: (optional) (out): a location to return the resulting private key
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Generate a new key pair of public and private keys. This call may block for an
 * indefinite period.
 *
 * Return value: %TRUE if the operation succeeded.
 **/
gboolean
gck_session_generate_key_pair_full (GckSession *self,
                                    GckMechanism *mechanism,
                                    GckAttributes *public_attrs,
                                    GckAttributes *private_attrs,
                                    GckObject **public_key,
                                    GckObject **private_key,
                                    GCancellable *cancellable,
                                    GError **error)
{
	GenerateKeyPair args = { GCK_ARGUMENTS_INIT, GCK_MECHANISM_EMPTY, public_attrs, private_attrs, 0, 0 };
	gboolean ret;

	g_return_val_if_fail (GCK_IS_SESSION (self), FALSE);
	g_return_val_if_fail (mechanism, FALSE);
	g_return_val_if_fail (public_attrs, FALSE);
	g_return_val_if_fail (private_attrs, FALSE);

	/* Shallow copy of the mechanism structure */
	memcpy (&args.mechanism, mechanism, sizeof (args.mechanism));

	ret = _gck_call_sync (self, perform_generate_key_pair, NULL, &args, cancellable, error);

	if (!ret)
		return FALSE;

	if (public_key)
		*public_key = gck_object_from_handle (self, args.public_key);
	if (private_key)
		*private_key = gck_object_from_handle (self, args.private_key);
	return TRUE;
}

/**
 * gck_session_generate_key_pair_async:
 * @self: The session to use.
 * @mechanism: The mechanism to use for key generation.
 * @public_attrs: Additional attributes for the generated public key.
 * @private_attrs: Additional attributes for the generated private key.
 * @cancellable: (nullable): Optional cancellation object or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Generate a new key pair of public and private keys. This call will
 * return immediately and complete asynchronously.
 *
 * If @public_attrs and/or @private_attrs is a floating reference, it is
 * consumed.
 **/
void
gck_session_generate_key_pair_async (GckSession *self, GckMechanism *mechanism,
                                      GckAttributes *public_attrs, GckAttributes *private_attrs,
                                      GCancellable *cancellable, GAsyncReadyCallback callback,
                                      gpointer user_data)
{
	GckCall *call;
	GenerateKeyPair *args;

	call  = _gck_call_async_prep (self, perform_generate_key_pair,
	                              NULL, sizeof (*args), free_generate_key_pair);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (mechanism);
	g_return_if_fail (public_attrs);
	g_return_if_fail (private_attrs);

	/* Shallow copy of the mechanism structure */
	memcpy (&args->mechanism, mechanism, sizeof (args->mechanism));

	args->public_attrs = gck_attributes_ref (public_attrs);
	args->private_attrs = gck_attributes_ref (private_attrs);

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_generate_key_pair_finish:
 * @self: The session to use.
 * @result: The async result passed to the callback.
 * @public_key: (optional) (out): a location to return the resulting public key
 * @private_key: (optional) (out): a location to return the resulting private key
 * @error: A location to return an error.
 *
 * Get the result of a generate key pair operation.
 *
 * Return value: %TRUE if the operation succeeded.
 **/
gboolean
gck_session_generate_key_pair_finish (GckSession *self,
                                      GAsyncResult *result,
                                      GckObject **public_key,
                                      GckObject **private_key,
                                      GError **error)
{
	GenerateKeyPair *args;

	g_return_val_if_fail (GCK_IS_SESSION (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	args = _gck_call_async_result_arguments (result, GenerateKeyPair);

	if (!_gck_call_basic_finish (result, error))
		return FALSE;

	if (public_key)
		*public_key = gck_object_from_handle (self, args->public_key);
	if (private_key)
		*private_key = gck_object_from_handle (self, args->private_key);
	return TRUE;
}

/* -----------------------------------------------------------------------------
 * KEY WRAPPING
 */

typedef struct _WrapKey {
	GckArguments base;
	GckMechanism mechanism;
	CK_OBJECT_HANDLE wrapper;
	CK_OBJECT_HANDLE wrapped;
	gpointer result;
	gulong n_result;
} WrapKey;

static void
free_wrap_key (WrapKey *args)
{
	g_clear_pointer (&args->result, g_free);
	g_free (args);
}

static CK_RV
perform_wrap_key (WrapKey *args)
{
	CK_RV rv;

	g_assert (sizeof (CK_MECHANISM) == sizeof (GckMechanism));

	/* Get the length of the result */
	rv = (args->base.pkcs11->C_WrapKey) (args->base.handle,
	                                     (CK_MECHANISM_PTR)&(args->mechanism),
	                                     args->wrapper, args->wrapped,
	                                     NULL, &args->n_result);
	if (rv != CKR_OK)
		return rv;

	/* And try again with a real buffer */
	args->result = g_malloc0 (args->n_result);
	return (args->base.pkcs11->C_WrapKey) (args->base.handle,
	                                       (CK_MECHANISM_PTR)&(args->mechanism),
	                                       args->wrapper, args->wrapped,
	                                       args->result, &args->n_result);
}

/**
 * gck_session_wrap_key:
 * @self: The session to use.
 * @wrapper: The key to use for wrapping.
 * @mech_type: The mechanism type to use for wrapping.
 * @wrapped: The key to wrap.
 * @n_result: A location in which to return the length of the wrapped data.
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @error: A location to return an error, or %NULL.
 *
 * Wrap a key into a byte stream. This call may block for an
 * indefinite period.
 *
 * Returns: (transfer full) (array length=n_result): the wrapped data or %NULL
 *          if the operation failed
 **/
guchar *
gck_session_wrap_key (GckSession *self, GckObject *key, gulong mech_type,
                      GckObject *wrapped, gsize *n_result, GCancellable *cancellable, GError **error)
{
	GckMechanism mech = { mech_type, NULL, 0 };
	return gck_session_wrap_key_full (self, key, &mech, wrapped, n_result, cancellable, error);
}

/**
 * gck_session_wrap_key_full:
 * @self: The session to use.
 * @wrapper: The key to use for wrapping.
 * @mechanism: The mechanism to use for wrapping.
 * @wrapped: The key to wrap.
 * @n_result: A location in which to return the length of the wrapped data.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Wrap a key into a byte stream. This call may block for an
 * indefinite period.
 *
 * Returns: (transfer full) (array length=n_result): the wrapped data or %NULL
 *          if the operation failed
 **/
guchar *
gck_session_wrap_key_full (GckSession *self, GckObject *wrapper, GckMechanism *mechanism,
                            GckObject *wrapped, gsize *n_result, GCancellable *cancellable,
                            GError **error)
{
	WrapKey args = { GCK_ARGUMENTS_INIT, GCK_MECHANISM_EMPTY, 0, 0, NULL, 0 };
	gboolean ret;

	g_return_val_if_fail (GCK_IS_SESSION (self), FALSE);
	g_return_val_if_fail (mechanism, FALSE);
	g_return_val_if_fail (GCK_IS_OBJECT (wrapped), FALSE);
	g_return_val_if_fail (GCK_IS_OBJECT (wrapper), FALSE);
	g_return_val_if_fail (n_result, FALSE);

	/* Shallow copy of the mechanism structure */
	memcpy (&args.mechanism, mechanism, sizeof (args.mechanism));

	g_object_get (wrapper, "handle", &args.wrapper, NULL);
	g_return_val_if_fail (args.wrapper != 0, NULL);
	g_object_get (wrapped, "handle", &args.wrapped, NULL);
	g_return_val_if_fail (args.wrapped != 0, NULL);

	ret = _gck_call_sync (self, perform_wrap_key, NULL, &args, cancellable, error);

	if (!ret)
		return FALSE;

	*n_result = args.n_result;
	return args.result;
}

/**
 * gck_session_wrap_key_async:
 * @self: The session to use.
 * @wrapper: The key to use for wrapping.
 * @mechanism: The mechanism to use for wrapping.
 * @wrapped: The key to wrap.
 * @cancellable: (nullable): Optional cancellation object or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Wrap a key into a byte stream. This call will
 * return immediately and complete asynchronously.
 **/
void
gck_session_wrap_key_async (GckSession *self, GckObject *key, GckMechanism *mechanism,
                             GckObject *wrapped, GCancellable *cancellable,
                             GAsyncReadyCallback callback, gpointer user_data)
{
	GckCall *call;
	WrapKey *args;

	call = _gck_call_async_prep (self, perform_wrap_key,
	                              NULL, sizeof (*args), free_wrap_key);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (mechanism);
	g_return_if_fail (GCK_IS_OBJECT (wrapped));
	g_return_if_fail (GCK_IS_OBJECT (key));

	/* Shallow copy of the mechanism structure */
	memcpy (&args->mechanism, mechanism, sizeof (args->mechanism));

	g_object_get (key, "handle", &args->wrapper, NULL);
	g_return_if_fail (args->wrapper != 0);
	g_object_get (wrapped, "handle", &args->wrapped, NULL);
	g_return_if_fail (args->wrapped != 0);

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_wrap_key_finish:
 * @self: The session to use.
 * @result: The async result passed to the callback.
 * @n_result: A location in which to return the length of the wrapped data.
 * @error: A location to return an error.
 *
 * Get the result of a wrap key operation.
 *
 * Returns: (transfer full) (array length=n_result): the wrapped data or %NULL
 *          if the operation failed
 **/
guchar *
gck_session_wrap_key_finish (GckSession *self, GAsyncResult *result,
                              gsize *n_result, GError **error)
{
	WrapKey *args;
	gpointer ret;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);
	g_return_val_if_fail (n_result, NULL);

	args = _gck_call_async_result_arguments (result, WrapKey);

	if (!_gck_call_basic_finish (result, error))
		return NULL;

	*n_result = args->n_result;
	args->n_result = 0;
	ret = args->result;
	args->result = NULL;

	return ret;
}

/* -----------------------------------------------------------------------------
 * KEY UNWRAPPING
 */

typedef struct _UnwrapKey {
	GckArguments base;
	GckMechanism mechanism;
	GckAttributes *attrs;
	CK_OBJECT_HANDLE wrapper;
	gconstpointer input;
	gulong n_input;
	CK_OBJECT_HANDLE unwrapped;
} UnwrapKey;

static void
free_unwrap_key (UnwrapKey *args)
{
	g_clear_pointer (&args->attrs, gck_attributes_unref);
	g_free (args);
}

static CK_RV
perform_unwrap_key (UnwrapKey *args)
{
	CK_ATTRIBUTE_PTR attrs;
	CK_ULONG n_attrs;

	g_assert (sizeof (CK_MECHANISM) == sizeof (GckMechanism));

	attrs = _gck_attributes_commit_out (args->attrs, &n_attrs);

	return (args->base.pkcs11->C_UnwrapKey) (args->base.handle,
	                                         (CK_MECHANISM_PTR)&(args->mechanism),
	                                         args->wrapper, (CK_BYTE_PTR)args->input,
	                                         args->n_input, attrs, n_attrs,
	                                         &args->unwrapped);
}

/**
 * gck_session_unwrap_key:
 * @self: The session to use.
 * @wrapper: The key to use for unwrapping.
 * @mech_type: The mechanism to use for unwrapping.
 * @input: (array length=n_input): the wrapped data as a byte stream
 * @n_input: The length of the wrapped data.
 * @attrs: Additional attributes for the unwrapped key.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Unwrap a key from a byte stream. This call may block for an
 * indefinite period.
 *
 * Returns: (transfer full): the new unwrapped key or %NULL if the
 *          operation failed
 **/
GckObject *
gck_session_unwrap_key (GckSession *self,
                        GckObject *wrapper,
                        gulong mech_type,
                        const guchar *input,
                        gsize n_input,
                        GckAttributes *attrs,
                        GCancellable *cancellable,
                        GError **error)
{
	GckMechanism mech = { mech_type, NULL, 0 };
	return gck_session_unwrap_key_full (self, wrapper, &mech, input, n_input, attrs, cancellable, error);
}

/**
 * gck_session_unwrap_key_full:
 * @self: The session to use.
 * @wrapper: The key to use for unwrapping.
 * @mechanism: The mechanism to use for unwrapping.
 * @input: (array length=n_input): the wrapped data as a byte stream
 * @n_input: The length of the wrapped data.
 * @attrs: Additional attributes for the unwrapped key.
 * @cancellable: (nullable): Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Unwrap a key from a byte stream. This call may block for an
 * indefinite period.
 *
 * Returns: (transfer full): the new unwrapped key or %NULL if the operation
 *          failed
 **/
GckObject *
gck_session_unwrap_key_full (GckSession *self,
                             GckObject *wrapper,
                             GckMechanism *mechanism,
                             const guchar *input,
                             gsize n_input,
                             GckAttributes *attrs,
                             GCancellable *cancellable,
                             GError **error)
{
	UnwrapKey args = { GCK_ARGUMENTS_INIT, GCK_MECHANISM_EMPTY, attrs, 0, input, n_input, 0 };
	gboolean ret;

	g_return_val_if_fail (GCK_IS_SESSION (self), FALSE);
	g_return_val_if_fail (GCK_IS_OBJECT (wrapper), FALSE);
	g_return_val_if_fail (mechanism, FALSE);
	g_return_val_if_fail (attrs, FALSE);

	/* Shallow copy of the mechanism structure */
	memcpy (&args.mechanism, mechanism, sizeof (args.mechanism));

	g_object_get (wrapper, "handle", &args.wrapper, NULL);
	g_return_val_if_fail (args.wrapper != 0, NULL);

	ret = _gck_call_sync (self, perform_unwrap_key, NULL, &args, cancellable, error);

	if (!ret)
		return NULL;

	return gck_object_from_handle (self, args.unwrapped);
}

/**
 * gck_session_unwrap_key_async:
 * @self: The session to use.
 * @wrapper: The key to use for unwrapping.
 * @mechanism: The mechanism to use for unwrapping.
 * @input: (array length=n_input): the wrapped data as a byte stream
 * @n_input: The length of the wrapped data.
 * @attrs: Additional attributes for the unwrapped key.
 * @cancellable: (nullable): Optional cancellation object or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Unwrap a key from a byte stream. This call will
 * return immediately and complete asynchronously.
 **/
void
gck_session_unwrap_key_async (GckSession *self,
                              GckObject *wrapper,
                              GckMechanism *mechanism,
                              const guchar *input,
                              gsize n_input,
                              GckAttributes *attrs,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GckCall *call;
	UnwrapKey *args;

	call = _gck_call_async_prep (self, perform_unwrap_key,
	                             NULL, sizeof (*args), free_unwrap_key);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (GCK_IS_OBJECT (wrapper));
	g_return_if_fail (attrs);

	g_object_get (wrapper, "handle", &args->wrapper, NULL);
	g_return_if_fail (args->wrapper != 0);

	/* Shallow copy of the mechanism structure */
	memcpy (&args->mechanism, mechanism, sizeof (args->mechanism));

	args->attrs = gck_attributes_ref (attrs);
	args->input = input;
	args->n_input = n_input;

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_unwrap_key_finish:
 * @self: The session to use.
 * @result: The async result passed to the callback.
 * @error: A location to return an error.
 *
 * Get the result of a unwrap key operation.
 *
 * Returns: (transfer full): the new unwrapped key or %NULL if the operation
 *          failed.
 **/
GckObject *
gck_session_unwrap_key_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	UnwrapKey *args;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);

	args = _gck_call_async_result_arguments (result, UnwrapKey);

	if (!_gck_call_basic_finish (result, error))
		return NULL;
	return gck_object_from_handle (self, args->unwrapped);
}

/* -----------------------------------------------------------------------------
 * KEY DERIVATION
 */

typedef struct _DeriveKey {
	GckArguments base;
	GckMechanism mechanism;
	GckAttributes *attrs;
	CK_OBJECT_HANDLE key;
	CK_OBJECT_HANDLE derived;
} DeriveKey;

static void
free_derive_key (DeriveKey *args)
{
	g_clear_pointer (&args->attrs, gck_attributes_unref);
	g_free (args);
}

static CK_RV
perform_derive_key (DeriveKey *args)
{
	CK_ATTRIBUTE_PTR attrs;
	CK_ULONG n_attrs;

	g_assert (sizeof (CK_MECHANISM) == sizeof (GckMechanism));

	attrs = _gck_attributes_commit_out (args->attrs, &n_attrs);

	return (args->base.pkcs11->C_DeriveKey) (args->base.handle,
	                                         (CK_MECHANISM_PTR)&(args->mechanism),
	                                         args->key, attrs, n_attrs,
	                                         &args->derived);
}

/**
 * gck_session_derive_key:
 * @self: The session to use.
 * @base: The key to derive from.
 * @mech_type: The mechanism to use for derivation.
 * @attrs: Additional attributes for the derived key.
 * @cancellable: Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Derive a key from another key. This call may block for an
 * indefinite period.
 *
 * If the @attrs #GckAttributes is floating, it is consumed.
 *
 * Returns: (transfer full): the new derived key or %NULL if the operation
 *          failed
 **/
GckObject *
gck_session_derive_key (GckSession *self, GckObject *base, gulong mech_type,
                        GckAttributes *attrs, GCancellable *cancellable, GError **error)
{
	GckMechanism mech = { mech_type, NULL, 0 };
	return gck_session_derive_key_full (self, base, &mech, attrs, cancellable, error);
}

/**
 * gck_session_derive_key_full:
 * @self: The session to use.
 * @base: The key to derive from.
 * @mechanism: The mechanism to use for derivation.
 * @attrs: Additional attributes for the derived key.
 * @cancellable: Optional cancellation object, or %NULL.
 * @error: A location to return an error, or %NULL.
 *
 * Derive a key from another key. This call may block for an
 * indefinite period.
 *
 * Returns: (transfer full): the new derived key or %NULL if the operation
 *          failed
 **/
GckObject*
gck_session_derive_key_full (GckSession *self, GckObject *base, GckMechanism *mechanism,
                             GckAttributes *attrs, GCancellable *cancellable, GError **error)
{
	DeriveKey args = { GCK_ARGUMENTS_INIT, GCK_MECHANISM_EMPTY, attrs, 0, 0 };
	gboolean ret;

	g_return_val_if_fail (GCK_IS_SESSION (self), FALSE);
	g_return_val_if_fail (GCK_IS_OBJECT (base), FALSE);
	g_return_val_if_fail (mechanism, FALSE);
	g_return_val_if_fail (attrs, FALSE);

	/* Shallow copy of the mechanism structure */
	memcpy (&args.mechanism, mechanism, sizeof (args.mechanism));

	g_object_get (base, "handle", &args.key, NULL);
	g_return_val_if_fail (args.key != 0, NULL);

	ret = _gck_call_sync (self, perform_derive_key, NULL, &args, cancellable, error);

	if (!ret)
		return NULL;

	return gck_object_from_handle (self, args.derived);
}

/**
 * gck_session_derive_key_async:
 * @self: The session to use.
 * @base: The key to derive from.
 * @mechanism: The mechanism to use for derivation.
 * @attrs: Additional attributes for the derived key.
 * @cancellable: Optional cancellation object or %NULL.
 * @callback: Called when the operation completes.
 * @user_data: Data to pass to the callback.
 *
 * Derive a key from another key. This call will
 * return immediately and complete asynchronously.
 **/
void
gck_session_derive_key_async (GckSession *self, GckObject *base, GckMechanism *mechanism,
                               GckAttributes *attrs, GCancellable *cancellable,
                               GAsyncReadyCallback callback, gpointer user_data)
{
	GckCall *call;
	DeriveKey *args;

	call = _gck_call_async_prep (self, perform_derive_key,
	                             NULL, sizeof (*args), free_derive_key);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (GCK_IS_SESSION (self));
	g_return_if_fail (GCK_IS_OBJECT (base));
	g_return_if_fail (attrs);

	g_object_get (base, "handle", &args->key, NULL);
	g_return_if_fail (args->key != 0);

	/* Shallow copy of the mechanism structure */
	memcpy (&args->mechanism, mechanism, sizeof (args->mechanism));

	args->attrs = gck_attributes_ref (attrs);

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_derive_key_finish:
 * @self: The session to use.
 * @result: The async result passed to the callback.
 * @error: A location to return an error.
 *
 * Get the result of a derive key operation.
 *
 * Returns: (transfer full): the new derived key or %NULL if the operation
 *          failed
 **/
GckObject *
gck_session_derive_key_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	DeriveKey *args;

	g_return_val_if_fail (GCK_IS_SESSION (self), NULL);

	args = _gck_call_async_result_arguments (result, DeriveKey);

	if (!_gck_call_basic_finish (result, error))
		return NULL;

	return gck_object_from_handle (self, args->derived);
}

/* --------------------------------------------------------------------------------------------------
 * COMMON CRYPTO ROUTINES
 */

typedef struct _Crypt {
	GckArguments base;

	/* Functions to call */
	CK_C_EncryptInit init_func;
	CK_C_Encrypt complete_func;

	/* Interaction */
	GckObject *key_object;
	GTlsInteraction *interaction;

	/* Input */
	CK_OBJECT_HANDLE key;
	GckMechanism mechanism;
	guchar *input;
	CK_ULONG n_input;

	/* Output */
	guchar *result;
	CK_ULONG n_result;

} Crypt;

static CK_RV
perform_crypt (Crypt *args)
{
	CK_RV rv;

	g_assert (args);
	g_assert (args->init_func);
	g_assert (args->complete_func);
	g_assert (!args->result);
	g_assert (!args->n_result);

	/* Initialize the crypt operation */
	rv = (args->init_func) (args->base.handle, (CK_MECHANISM_PTR)&(args->mechanism), args->key);
	if (rv != CKR_OK)
		return rv;

	rv = _gck_session_authenticate_key (args->base.pkcs11, args->base.handle,
	                                    args->key_object, args->interaction, NULL);

	if (rv != CKR_OK)
		return rv;

	/* Get the length of the result */
	rv = (args->complete_func) (args->base.handle, args->input, args->n_input, NULL, &args->n_result);
	if (rv != CKR_OK)
		return rv;

	/* And try again with a real buffer */
	args->result = g_malloc0 (args->n_result);
	return (args->complete_func) (args->base.handle, args->input, args->n_input, args->result, &args->n_result);
}

static void
free_crypt (Crypt *args)
{
	g_clear_object (&args->interaction);
	g_clear_object (&args->key_object);

	g_clear_pointer (&args->input, g_free);
	g_clear_pointer (&args->result, g_free);
	g_free (args);
}

static guchar*
crypt_sync (GckSession *self, GckObject *key, GckMechanism *mechanism, const guchar *input,
            gsize n_input, gsize *n_result, GCancellable *cancellable, GError **error,
            CK_C_EncryptInit init_func, CK_C_Encrypt complete_func)
{
	Crypt args;

	g_return_val_if_fail (GCK_IS_OBJECT (key), NULL);
	g_return_val_if_fail (mechanism, NULL);
	g_return_val_if_fail (init_func, NULL);
	g_return_val_if_fail (complete_func, NULL);

	memset (&args, 0, sizeof (args));
	g_object_get (key, "handle", &args.key, NULL);
	g_return_val_if_fail (args.key != 0, NULL);

	/* Shallow copy of the mechanism structure */
	memcpy (&args.mechanism, mechanism, sizeof (args.mechanism));

	/* No need to copy in this case */
	args.input = (guchar*)input;
	args.n_input = n_input;

	args.init_func = init_func;
	args.complete_func = complete_func;

	args.key_object = key;
	args.interaction = gck_session_get_interaction (self);

	if (!_gck_call_sync (self, perform_crypt, NULL, &args, cancellable, error)) {
		g_free (args.result);
		args.result = NULL;
	} else {
		*n_result = args.n_result;
	}

	g_clear_object (&args.interaction);
	return args.result;
}

static void
crypt_async (GckSession *self, GckObject *key, GckMechanism *mechanism, const guchar *input,
             gsize n_input, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data,
             CK_C_EncryptInit init_func, CK_C_Encrypt complete_func)
{
	GckCall *call;
	Crypt *args;

	call = _gck_call_async_prep (self, perform_crypt, NULL, sizeof (*args), free_crypt);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (GCK_IS_OBJECT (key));
	g_return_if_fail (mechanism);
	g_return_if_fail (init_func);
	g_return_if_fail (complete_func);

	g_object_get (key, "handle", &args->key, NULL);
	g_return_if_fail (args->key != 0);

	/* Shallow copy of the mechanism structure */
	memcpy (&args->mechanism, mechanism, sizeof (args->mechanism));

	args->input = input && n_input ? g_memdup2 (input, n_input) : NULL;
	args->n_input = n_input;

	args->init_func = init_func;
	args->complete_func = complete_func;

	args->key_object = g_object_ref (key);
	args->interaction = gck_session_get_interaction (self);

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

static guchar*
crypt_finish (GckSession *self, GAsyncResult *result, gsize *n_result, GError **error)
{
	Crypt *args;
	guchar *res;

	if (!_gck_call_basic_finish (result, error))
		return NULL;
	args = _gck_call_async_result_arguments (result, Crypt);

	/* Steal the values from the results */
	res = args->result;
	args->result = NULL;
	*n_result = args->n_result;
	args->n_result = 0;

	return res;
}

/* --------------------------------------------------------------------------------------------------
 * ENCRYPT
 */

/**
 * gck_session_encrypt:
 * @self: The session.
 * @key: The key to encrypt with.
 * @mech_type: The mechanism type to use for encryption.
 * @input: (array length=n_input): the data to encrypt
 * @n_input: the length of the data to encrypt
 * @n_result: location to store the length of the result data
 * @cancellable: Optional cancellation object, or %NULL
 * @error: A location to place error information.
 *
 * Encrypt data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_result): the data that was encrypted,
 *          or %NULL if an error occured.
 */
guchar *
gck_session_encrypt (GckSession *self, GckObject *key, gulong mech_type, const guchar *input,
                      gsize n_input, gsize *n_result, GCancellable *cancellable, GError **error)
{
	GckMechanism mechanism = { mech_type, NULL, 0 };
	return gck_session_encrypt_full (self, key, &mechanism, input, n_input, n_result, cancellable, error);
}

/**
 * gck_session_encrypt_full:
 * @self: The session.
 * @key: The key to encrypt with.
 * @mechanism: The mechanism type and parameters to use for encryption.
 * @input: (array length=n_input): the data to encrypt
 * @n_input: the length of the data to encrypt
 * @n_result: location to store the length of the result data
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @error: A location to place error information.
 *
 * Encrypt data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_result): the data that was encrypted,
 *          or %NULL if an error occured
 */
guchar *
gck_session_encrypt_full (GckSession *self, GckObject *key, GckMechanism *mechanism,
                           const guchar *input, gsize n_input, gsize *n_result,
                           GCancellable *cancellable, GError **error)
{
	GckModule *module = NULL;
	CK_FUNCTION_LIST_PTR funcs;
	guchar *ret;

	g_object_get (self, "module", &module, NULL);
	g_return_val_if_fail (module != NULL, NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (module != NULL, NULL);

	ret = crypt_sync (self, key, mechanism, input, n_input, n_result, cancellable, error,
	                  funcs->C_EncryptInit, funcs->C_Encrypt);

	g_object_unref (module);
	return ret;
}

/**
 * gck_session_encrypt_async:
 * @self: The session.
 * @key: The key to encrypt with.
 * @mechanism: The mechanism type and parameters to use for encryption.
 * @input: (array length=n_input): the data to encrypt
 * @n_input: length of the data to encrypt
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @callback: Called when the operation completes.
 * @user_data: A pointer to pass to the callback.
 *
 * Encrypt data in a mechanism specific manner. This call will
 * return immediately and complete asynchronously.
 **/
void
gck_session_encrypt_async (GckSession *self, GckObject *key, GckMechanism *mechanism,
                            const guchar *input, gsize n_input, GCancellable *cancellable,
                            GAsyncReadyCallback callback, gpointer user_data)
{
	GckModule *module = NULL;
	CK_FUNCTION_LIST_PTR funcs;

	g_object_get (self, "module", &module, NULL);
	g_return_if_fail (module != NULL);

	funcs = gck_module_get_functions (module);
	g_return_if_fail (module != NULL);

	crypt_async (self, key, mechanism, input, n_input, cancellable, callback, user_data,
	             funcs->C_EncryptInit, funcs->C_Encrypt);

	g_object_unref (module);
}

/**
 * gck_session_encrypt_finish:
 * @self: The session.
 * @result: The result object passed to the callback.
 * @n_result: A location to store the length of the result data.
 * @error: A location to place error information.
 *
 * Get the result of an encryption operation.
 *
 * Returns: (transfer full) (array length=n_result): the data that was encrypted,
 *          or %NULL if an error occurred.
 */
guchar*
gck_session_encrypt_finish (GckSession *self, GAsyncResult *result, gsize *n_result,
                             GError **error)
{
	return crypt_finish (self, result, n_result, error);
}

/* --------------------------------------------------------------------------------------------------
 * DECRYPT
 */

/**
 * gck_session_decrypt:
 * @self: The session.
 * @key: The key to decrypt with.
 * @mech_type: The mechanism type to use for decryption.
 * @input: (array length=n_input): data to decrypt
 * @n_input: length of the data to decrypt
 * @n_result: location to store the length of the result data
 * @cancellable: Optional cancellation object, or %NULL
 * @error: A location to place an error.
 *
 * Decrypt data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_result): the data that was decrypted,
 *          or %NULL if an error occured
 */
guchar *
gck_session_decrypt (GckSession *self, GckObject *key, gulong mech_type, const guchar *input,
                      gsize n_input, gsize *n_result, GCancellable *cancellable, GError **error)
{
	GckMechanism mechanism = { mech_type, NULL, 0 };
	return gck_session_decrypt_full (self, key, &mechanism, input, n_input, n_result, cancellable, error);
}

/**
 * gck_session_decrypt_full:
 * @self: The session.
 * @key: The key to decrypt with.
 * @mechanism: The mechanism type and parameters to use for decryption.
 * @input: (array length=n_input): data to decrypt
 * @n_input: length of the data to decrypt
 * @n_result: location to store the length of the result data
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @error: A location to place error information.
 *
 * Decrypt data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_result): the data that was decrypted,
 *          or %NULL if an error occured
 */
guchar *
gck_session_decrypt_full (GckSession *self, GckObject *key, GckMechanism *mechanism,
                           const guchar *input, gsize n_input, gsize *n_result,
                           GCancellable *cancellable, GError **error)
{
	GckModule *module = NULL;
	CK_FUNCTION_LIST_PTR funcs;
	guchar *ret;

	g_object_get (self, "module", &module, NULL);
	g_return_val_if_fail (module != NULL, NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (module != NULL, NULL);

	ret = crypt_sync (self, key, mechanism, input, n_input, n_result, cancellable, error,
	                  funcs->C_DecryptInit, funcs->C_Decrypt);
	g_object_unref (module);
	return ret;
}

/**
 * gck_session_decrypt_async:
 * @self: The session.
 * @key: The key to decrypt with.
 * @mechanism: The mechanism type and parameters to use for decryption.
 * @input: (array length=n_input): data to decrypt
 * @n_input: length of the data to decrypt
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @callback: Called when the operation completes.
 * @user_data: A pointer to pass to the callback.
 *
 * Decrypt data in a mechanism specific manner. This call will
 * return immediately and complete asynchronously.
 */
void
gck_session_decrypt_async (GckSession *self, GckObject *key, GckMechanism *mechanism,
                            const guchar *input, gsize n_input, GCancellable *cancellable,
                            GAsyncReadyCallback callback, gpointer user_data)
{
	GckModule *module = NULL;
	CK_FUNCTION_LIST_PTR funcs;

	g_object_get (self, "module", &module, NULL);
	g_return_if_fail (module != NULL);

	funcs = gck_module_get_functions (module);
	g_return_if_fail (module != NULL);

	crypt_async (self, key, mechanism, input, n_input, cancellable, callback, user_data,
	             funcs->C_DecryptInit, funcs->C_Decrypt);
	g_object_unref (module);
}

/**
 * gck_session_decrypt_finish:
 * @self: The session.
 * @result: The result object passed to the callback.
 * @n_result: A location to store the length of the result data.
 * @error: A location to place error information.
 *
 * Get the result of an decryption operation.
 *
 * Returns: (transfer full) (array length=n_result): the data that was decrypted,
 *          or %NULL if an error occurred
 */
guchar*
gck_session_decrypt_finish (GckSession *self, GAsyncResult *result,
                             gsize *n_result, GError **error)
{
	return crypt_finish (self, result, n_result, error);
}

/* --------------------------------------------------------------------------------------------------
 * SIGN
 */

/**
 * gck_session_sign:
 * @self: The session.
 * @key: The key to sign with.
 * @mech_type: The mechanism type to use for signing.
 * @input: (array length=n_input): data to sign
 * @n_input: length of the data to sign
 * @n_result: location to store the length of the result data
 * @cancellable: Optional cancellation object, or %NULL
 * @error: A location to place an error.
 *
 * Sign data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: (transfer full) (array length=n_result): the data that was signed,
 *          or %NULL if an error occured
 */
guchar *
gck_session_sign (GckSession *self, GckObject *key, gulong mech_type, const guchar *input,
                   gsize n_input, gsize *n_result, GCancellable *cancellable, GError **error)
{
	GckMechanism mechanism = { mech_type, NULL, 0 };
	return gck_session_sign_full (self, key, &mechanism, input, n_input, n_result, NULL, error);
}

/**
 * gck_session_sign_full:
 * @self: The session.
 * @key: The key to sign with.
 * @mechanism: The mechanism type and parameters to use for signing.
 * @input: (array length=n_input): data to sign
 * @n_input: length of the data to sign
 * @n_result: location to store the length of the result data
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @error: A location to place error information.
 *
 * Sign data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: The data that was signed, or %NULL if an error occured.
 */
guchar*
gck_session_sign_full (GckSession *self, GckObject *key, GckMechanism *mechanism,
                        const guchar *input, gsize n_input, gsize *n_result,
                        GCancellable *cancellable, GError **error)
{
	GckModule *module = NULL;
	CK_FUNCTION_LIST_PTR funcs;
	guchar *ret;

	g_object_get (self, "module", &module, NULL);
	g_return_val_if_fail (module != NULL, NULL);

	funcs = gck_module_get_functions (module);
	g_return_val_if_fail (module != NULL, NULL);

	ret = crypt_sync (self, key, mechanism, input, n_input, n_result, cancellable, error,
	                  funcs->C_SignInit, funcs->C_Sign);
	g_object_unref (module);
	return ret;
}

/**
 * gck_session_sign_async:
 * @self: The session.
 * @key: The key to sign with.
 * @mechanism: The mechanism type and parameters to use for signing.
 * @input: (array length=n_input): data to sign
 * @n_input: length of the data to sign
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @callback: Called when the operation completes.
 * @user_data: A pointer to pass to the callback.
 *
 * Sign data in a mechanism specific manner. This call will
 * return immediately and complete asynchronously.
 */
void
gck_session_sign_async (GckSession *self, GckObject *key, GckMechanism *mechanism,
                         const guchar *input, gsize n_input, GCancellable *cancellable,
                         GAsyncReadyCallback callback, gpointer user_data)
{
	GckModule *module = NULL;
	CK_FUNCTION_LIST_PTR funcs;

	g_object_get (self, "module", &module, NULL);
	g_return_if_fail (module != NULL);

	funcs = gck_module_get_functions (module);
	g_return_if_fail (module != NULL);

	crypt_async (self, key, mechanism, input, n_input, cancellable, callback, user_data,
	             funcs->C_SignInit, funcs->C_Sign);
	g_object_unref (module);
}

/**
 * gck_session_sign_finish:
 * @self: The session.
 * @result: The result object passed to the callback.
 * @n_result: A location to store the length of the result data.
 * @error: A location to place error information.
 *
 * Get the result of an signing operation.
 *
 * Returns: (transfer full) (array length=n_result): the data that was signed,
 *          or %NULL if an error occurred
 */
guchar *
gck_session_sign_finish (GckSession *self, GAsyncResult *result,
                          gsize *n_result, GError **error)
{
	return crypt_finish (self, result, n_result, error);
}

/* --------------------------------------------------------------------------------------------------
 * VERIFY
 */

typedef struct _Verify {
	GckArguments base;

	/* Interaction */
	GckObject *key_object;
	GTlsInteraction *interaction;

	/* Input */
	CK_OBJECT_HANDLE key;
	GckMechanism mechanism;
	guchar *input;
	CK_ULONG n_input;
	guchar *signature;
	CK_ULONG n_signature;

} Verify;

static CK_RV
perform_verify (Verify *args)
{
	CK_RV rv;

	/* Initialize the crypt operation */
	rv = (args->base.pkcs11->C_VerifyInit) (args->base.handle, (CK_MECHANISM_PTR)&(args->mechanism), args->key);
	if (rv != CKR_OK)
		return rv;

	rv = _gck_session_authenticate_key (args->base.pkcs11, args->base.handle,
	                                    args->key_object, args->interaction, NULL);

	if (rv != CKR_OK)
		return rv;

	/* Do the actual verify */
	return (args->base.pkcs11->C_Verify) (args->base.handle, args->input, args->n_input,
	                                      args->signature, args->n_signature);
}

static void
free_verify (Verify *args)
{
	g_clear_object (&args->interaction);
	g_clear_object (&args->key_object);

	g_clear_pointer (&args->input, g_free);
	g_clear_pointer (&args->signature, g_free);
	g_free (args);
}

/**
 * gck_session_verify:
 * @self: The session.
 * @key: The key to verify with.
 * @mech_type: The mechanism type to use for verifying.
 * @input: (array length=n_input): data to verify
 * @n_input: length of the data to verify
 * @signature: (array length=n_signature): the signature
 * @n_signature: length of the signature
 * @cancellable: Optional cancellation object, or %NULL
 * @error: A location to place an error.
 *
 * Verify data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: %TRUE if the data verified correctly, otherwise a failure or error occurred.
 */
gboolean
gck_session_verify (GckSession *self, GckObject *key, gulong mech_type, const guchar *input,
                     gsize n_input, const guchar *signature, gsize n_signature, GCancellable *cancellable, GError **error)
{
	GckMechanism mechanism = { mech_type, NULL, 0 };
	return gck_session_verify_full (self, key, &mechanism, input, n_input,
	                                 signature, n_signature, NULL, error);
}

/**
 * gck_session_verify_full:
 * @self: The session.
 * @key: The key to verify with.
 * @mechanism: The mechanism type and parameters to use for signing.
 * @input: (array length=n_input): data to verify
 * @n_input: the length of the data to verify
 * @signature: (array length=n_signature): the signature
 * @n_signature: length of the signature
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @error: A location to place an error.
 *
 * Verify data in a mechanism specific manner. This call may
 * block for an indefinite period.
 *
 * Returns: %TRUE if the data verified correctly, otherwise a failure or error occurred.
 */
gboolean
gck_session_verify_full (GckSession *self, GckObject *key, GckMechanism *mechanism,
                          const guchar *input, gsize n_input, const guchar *signature,
                          gsize n_signature, GCancellable *cancellable, GError **error)
{
	Verify args;
	gboolean ret;

	g_return_val_if_fail (GCK_IS_OBJECT (key), FALSE);
	g_return_val_if_fail (mechanism, FALSE);

	memset (&args, 0, sizeof (args));
	g_object_get (key, "handle", &args.key, NULL);
	g_return_val_if_fail (args.key != 0, FALSE);

	/* Shallow copy of the mechanism structure */
	memcpy (&args.mechanism, mechanism, sizeof (args.mechanism));

	/* No need to copy in this case */
	args.input = (guchar*)input;
	args.n_input = n_input;
	args.signature = (guchar*)signature;
	args.n_signature = n_signature;

	args.key_object = key;
	args.interaction = gck_session_get_interaction (self);

	ret = _gck_call_sync (self, perform_verify, NULL, &args, cancellable, error);

	g_clear_object (&args.interaction);

	return ret;
}

/**
 * gck_session_verify_async:
 * @self: The session.
 * @key: The key to verify with.
 * @mechanism: The mechanism type and parameters to use for signing.
 * @input: (array length=n_input): data to verify
 * @n_input: the length of the data to verify
 * @signature: (array length=n_signature): the signature
 * @n_signature: the length of the signature
 * @cancellable: A GCancellable which can be used to cancel the operation.
 * @callback: Called when the operation completes.
 * @user_data: A pointer to pass to the callback.
 *
 * Verify data in a mechanism specific manner. This call returns
 * immediately and completes asynchronously.
 */
void
gck_session_verify_async (GckSession *self, GckObject *key, GckMechanism *mechanism,
                           const guchar *input, gsize n_input, const guchar *signature,
                           gsize n_signature, GCancellable *cancellable,
                           GAsyncReadyCallback callback, gpointer user_data)
{
	GckCall *call;
	Verify *args;

	call = _gck_call_async_prep (self, perform_verify, NULL, sizeof (*args), free_verify);
	args = _gck_call_get_arguments (call);

	g_return_if_fail (GCK_IS_OBJECT (key));
	g_return_if_fail (mechanism);

	g_object_get (key, "handle", &args->key, NULL);
	g_return_if_fail (args->key != 0);

	/* Shallow copy of the mechanism structure */
	memcpy (&args->mechanism, mechanism, sizeof (args->mechanism));

	args->input = input && n_input ? g_memdup2 (input, n_input) : NULL;
	args->n_input = n_input;
	args->signature = signature && n_signature ? g_memdup2 (signature, n_signature) : NULL;
	args->n_signature = n_signature;

	args->key_object = g_object_ref (key);
	args->interaction = gck_session_get_interaction (self);

	_gck_call_async_ready_go (call, self, cancellable, callback, user_data);
}

/**
 * gck_session_verify_finish:
 * @self: The session.
 * @result: The result object passed to the callback.
 * @error: A location to place error information.
 *
 * Get the result of an verify operation.
 *
 * Returns: %TRUE if the data verified correctly, otherwise a failure or error occurred.
 */
gboolean
gck_session_verify_finish (GckSession *self, GAsyncResult *result, GError **error)
{
	return _gck_call_basic_finish (result, error);
}

static void
update_password_for_token (GTlsPassword *password,
                           CK_TOKEN_INFO *token_info,
                           gboolean request_retry)
{
	GTlsPasswordFlags flags;
	gchar *label;

	label = gck_string_from_chars (token_info->label, sizeof (token_info->label));
	g_tls_password_set_description (password, label);
	g_free (label);

	flags = 0;
	if (request_retry)
		flags |= G_TLS_PASSWORD_RETRY;
	if (token_info && token_info->flags & CKF_USER_PIN_COUNT_LOW)
		flags |= G_TLS_PASSWORD_MANY_TRIES;
	if (token_info && token_info->flags & CKF_USER_PIN_FINAL_TRY)
		flags |= G_TLS_PASSWORD_FINAL_TRY;
	g_tls_password_set_flags (password, flags);
}

CK_RV
_gck_session_authenticate_token (CK_FUNCTION_LIST_PTR funcs,
                                 CK_SESSION_HANDLE session,
                                 GckSlot *token,
                                 GTlsInteraction *interaction,
                                 GCancellable *cancellable)
{
	CK_SESSION_INFO session_info;
	GTlsPassword *password = NULL;
	CK_TOKEN_INFO token_info;
	GTlsInteractionResult res;
	gboolean request_retry;
	CK_SLOT_ID slot_id;
	CK_BYTE_PTR pin;
	gsize n_pin;
	CK_RV rv = CKR_OK;
	GError *error = NULL;

	g_assert (funcs != NULL);
	g_assert (GCK_IS_SLOT (token));

	slot_id = gck_slot_get_handle (token);
	request_retry = FALSE;

	do {
		if (g_cancellable_is_cancelled (cancellable)) {
			rv = CKR_FUNCTION_CANCELED;
			break;
		}

		rv = (funcs->C_GetTokenInfo) (slot_id, &token_info);
		if (rv != CKR_OK) {
			g_warning ("couldn't get token info when logging in: %s",
			           gck_message_from_rv (rv));
			break;
		}

		/* No login necessary? */
		if ((token_info.flags & CKF_LOGIN_REQUIRED) == 0) {
			g_debug ("no login required for token, skipping login");
			rv = CKR_OK;
			break;
		}

		/* Next check if session is logged in? */
		rv = (funcs->C_GetSessionInfo) (session, &session_info);
		if (rv != CKR_OK) {
			g_warning ("couldn't get session info when logging in: %s",
			           gck_message_from_rv (rv));
			break;
		}

		/* Already logged in? */
		if (session_info.state == CKS_RW_USER_FUNCTIONS ||
		    session_info.state == CKS_RO_USER_FUNCTIONS ||
		    session_info.state == CKS_RW_SO_FUNCTIONS) {
			g_debug ("already logged in, skipping login");
			rv = CKR_OK;
			break;
		}

		if (token_info.flags & CKF_PROTECTED_AUTHENTICATION_PATH) {
			g_debug ("trying to log into session: protected authentication path, no password");

			/* No password passed for PAP */
			pin = NULL;
			n_pin = 0;


		/* Not protected auth path */
		} else {
			g_debug ("trying to log into session: want password %s",
			            request_retry ? "login was incorrect" : "");

			if (password == NULL)
				password = g_object_new (GCK_TYPE_PASSWORD, "token", token, NULL);

			update_password_for_token (password, &token_info, request_retry);

			if (interaction == NULL)
				res = G_TLS_INTERACTION_UNHANDLED;

			else
				res = g_tls_interaction_invoke_ask_password (interaction,
				                                             G_TLS_PASSWORD (password),
				                                             NULL, &error);

			if (res == G_TLS_INTERACTION_FAILED) {
				g_message ("interaction couldn't ask password: %s", error->message);
				rv = _gck_rv_from_error (error, CKR_USER_NOT_LOGGED_IN);
				g_clear_error (&error);
				break;

			} else if (res == G_TLS_INTERACTION_UNHANDLED) {
				g_message ("couldn't authenticate: no interaction handler");
				rv = CKR_USER_NOT_LOGGED_IN;
				break;
			}

			pin = (CK_BYTE_PTR)g_tls_password_get_value (password, &n_pin);
		}

		/* Try to log in */
		rv = (funcs->C_Login) (session, CKU_USER, (CK_BYTE_PTR)pin, (CK_ULONG)n_pin);

		/* Only one C_Login call if protected auth path */
		if (token_info.flags & CKF_PROTECTED_AUTHENTICATION_PATH)
			break;

		request_retry = TRUE;
	} while (rv == CKR_PIN_INCORRECT);

	g_clear_object (&password);

	return rv;
}

static void
update_password_for_key (GTlsPassword *password,
                         CK_TOKEN_INFO *token_info,
                         gboolean request_retry)
{
	GTlsPasswordFlags flags;

	flags = 0;
	if (request_retry)
		flags |= G_TLS_PASSWORD_RETRY;
	if (token_info && token_info->flags & CKF_USER_PIN_COUNT_LOW)
		flags |= G_TLS_PASSWORD_MANY_TRIES;
	if (token_info && token_info->flags & CKF_USER_PIN_FINAL_TRY)
		flags |= G_TLS_PASSWORD_FINAL_TRY;
	g_tls_password_set_flags (password, flags);
}

CK_RV
_gck_session_authenticate_key (CK_FUNCTION_LIST_PTR funcs,
                               CK_SESSION_HANDLE session,
                               GckObject *key,
                               GTlsInteraction *interaction,
                               GCancellable *cancellable)
{
	CK_ATTRIBUTE attrs[2];
	CK_SESSION_INFO session_info;
	CK_TOKEN_INFO token_info;
	GTlsPassword *password = NULL;
	CK_OBJECT_HANDLE handle;
	GTlsInteractionResult res;
	gboolean request_retry;
	GError *error = NULL;
	CK_BYTE_PTR pin;
	gsize pin_len;
	CK_BBOOL bvalue;
	gboolean got_label;
	CK_RV rv;

	g_assert (funcs != NULL);

	handle = gck_object_get_handle (key);

	attrs[0].type = CKA_LABEL;
	attrs[0].pValue = NULL;
	attrs[0].ulValueLen = 0;
	attrs[1].type = CKA_ALWAYS_AUTHENTICATE;
	attrs[1].pValue = &bvalue;
	attrs[1].ulValueLen = sizeof (bvalue);

	rv = (funcs->C_GetAttributeValue) (session, handle, attrs, 2);
	if (rv == CKR_ATTRIBUTE_TYPE_INVALID) {
		bvalue = CK_FALSE;

	} else if (rv != CKR_OK) {
		g_message ("couldn't check whether key requires authentication, assuming it doesn't: %s",
		           gck_message_from_rv (rv));
		return CKR_OK;
	}

	/* No authentication needed, on this object */
	if (bvalue != CK_TRUE) {
		g_debug ("key does not require authentication");
		return CKR_OK;
	}

	got_label = FALSE;
	request_retry = FALSE;

	do {
		if (g_cancellable_is_cancelled (cancellable)) {
			rv = CKR_FUNCTION_CANCELED;
			break;
		}

		rv = (funcs->C_GetSessionInfo) (session, &session_info);
		if (rv != CKR_OK) {
			g_warning ("couldn't get session info when authenticating key: %s",
			           gck_message_from_rv (rv));
			return rv;
		}

		rv = (funcs->C_GetTokenInfo) (session_info.slotID, &token_info);
		if (rv != CKR_OK) {
			g_warning ("couldn't get token info when authenticating key: %s",
			           gck_message_from_rv (rv));
			return rv;
		}

		/* Protected authentication path, just use NULL passwords */
		if (token_info.flags & CKF_PROTECTED_AUTHENTICATION_PATH) {

			password = NULL;
			pin = NULL;
			pin_len = 0;

		/* Need to prompt for a password */
		} else {
			g_debug ("trying to log into session: want password %s",
			         request_retry ? "login was incorrect" : "");

			if (password == NULL)
				password = g_object_new (GCK_TYPE_PASSWORD, "key", key, NULL);

			/* Set the password */
			update_password_for_key (password, &token_info, request_retry);

			/* Set the label properly */
			if (!got_label) {
				if (attrs[0].ulValueLen && attrs[0].ulValueLen != GCK_INVALID) {
					attrs[0].pValue = g_malloc0 (attrs[0].ulValueLen + 1);
					rv = (funcs->C_GetAttributeValue) (session, handle, attrs, 1);
					if (rv == CKR_OK) {
						((gchar *)attrs[0].pValue)[attrs[0].ulValueLen] = 0;
						g_tls_password_set_description (password, attrs[0].pValue);
					}
					g_free (attrs[0].pValue);
					attrs[0].pValue = NULL;
				}

				got_label = TRUE;
			}

			if (interaction == NULL)
				res = G_TLS_INTERACTION_UNHANDLED;

			else
				res = g_tls_interaction_invoke_ask_password (interaction,
				                                             G_TLS_PASSWORD (password),
				                                             NULL, &error);

			if (res == G_TLS_INTERACTION_FAILED) {
				g_message ("interaction couldn't ask password: %s", error->message);
				rv = _gck_rv_from_error (error, CKR_USER_NOT_LOGGED_IN);
				g_clear_error (&error);
				break;

			} else if (res == G_TLS_INTERACTION_UNHANDLED) {
				g_message ("couldn't authenticate: no interaction handler");
				rv = CKR_USER_NOT_LOGGED_IN;
				break;
			}

			pin = (CK_BYTE_PTR)g_tls_password_get_value (G_TLS_PASSWORD (password), &pin_len);
		}

		/* Try to log in */
		rv = (funcs->C_Login) (session, CKU_CONTEXT_SPECIFIC, pin, pin_len);

		/* Only one C_Login call if protected auth path */
		if (token_info.flags & CKF_PROTECTED_AUTHENTICATION_PATH)
			break;

		request_retry = TRUE;
	} while (rv == CKR_PIN_INCORRECT);

	g_clear_object (&password);

	return rv;
}
