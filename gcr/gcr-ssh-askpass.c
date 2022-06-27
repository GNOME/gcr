/*
 * Copyright (C) 2014 Stefan Walter
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
 * Auther: Stef Walter <stefw@gnome.org>
 */

#include "config.h"

#include "gcr-ssh-askpass.h"

#include <glib-unix.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <unistd.h>

#define GCR_SSH_ASKPASS_BIN "gcr4-ssh-askpass"

/* Used from tests to override location */
const char *gcr_ssh_askpass_executable = LIBEXECDIR "/" GCR_SSH_ASKPASS_BIN;

/**
 * GcrSshAskpass:
 *
 * When used as the setup function while spawning an ssh command like ssh-add
 * or ssh, this allows callbacks for passwords on the provided interaction.
 */

enum {
	PROP_0,
	PROP_INTERACTION
};

struct _GcrSshAskpass {
	GObject parent;
	GTlsInteraction *interaction;
	gchar *directory;
	gchar *socket;
	guint source;
	gint fd;
	GCancellable *cancellable;
	GMainContext *context;
};

G_DEFINE_TYPE (GcrSshAskpass, gcr_ssh_askpass, G_TYPE_OBJECT);

static void
gcr_ssh_askpass_init (GcrSshAskpass *self)
{
	self->cancellable = g_cancellable_new ();
	self->context = g_main_context_ref_thread_default ();
}

static void
gcr_ssh_askpass_set_property (GObject *obj,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	GcrSshAskpass *self = GCR_SSH_ASKPASS (obj);

	switch (prop_id) {
	case PROP_INTERACTION:
		self->interaction = g_value_dup_object (value);
		g_return_if_fail (self->interaction != NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_ssh_askpass_get_property (GObject *obj,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	GcrSshAskpass *self = GCR_SSH_ASKPASS (obj);

	switch (prop_id) {
	case PROP_INTERACTION:
		g_value_set_object (value, gcr_ssh_askpass_get_interaction (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static gboolean
write_all (gint fd,
           const guchar *buf,
           gsize len)
{
	guint all = len;
	int res;

	while (len > 0) {
		res = write (fd, buf, len);
		if (res <= 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			if (errno != EPIPE)
				g_warning ("couldn't write %u bytes to client: %s", all,
				           res < 0 ? g_strerror (errno) : "");
			return FALSE;
		} else  {
			len -= res;
			buf += res;
		}
	}
	return TRUE;
}

static GString *
read_all_into_string (gint fd)
{
	GString *input = g_string_new ("");
	gsize len;
	gssize ret;

	for (;;) {
		len = input->len;
		g_string_set_size (input, len + 256);
		ret = read (fd, input->str + len, 256);
		if (ret < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				g_critical ("couldn't read from " GCR_SSH_ASKPASS_BIN ": %s", g_strerror (errno));
				g_string_free (input, TRUE);
				return NULL;
			}
		} else if (ret == 0) {
			return input;
		} else {
			input->len = len + ret;
			input->str[input->len] = '\0';
		}
	}
}

typedef struct {
	gint fd;
	GTlsInteraction *interaction;
	GCancellable *cancellable;
} AskpassContext;

static gpointer
askpass_thread (gpointer data)
{
	AskpassContext *ctx = data;
	gboolean success = FALSE;
	GTlsPassword *password = NULL;
	GTlsInteractionResult res;
	GError *error = NULL;
	const guchar *value;
	GString *input;
	gsize length;

	input = read_all_into_string (ctx->fd);
	if (!input)
		goto out;

	if (input->len == 0)
		g_string_append (input, _("Enter your OpenSSH passphrase"));

	g_debug ("asking for ssh-askpass password: %s", input->str);

	password = g_tls_password_new (G_TLS_PASSWORD_NONE, input->str);
	res = g_tls_interaction_invoke_ask_password (ctx->interaction, password, ctx->cancellable, &error);

	g_debug ("ask password returned %d", res);

	success = FALSE;
	if (res == G_TLS_INTERACTION_HANDLED) {
		value = g_tls_password_get_value (password, &length);
		if (write_all (ctx->fd, (const guchar *)value, length))
			g_debug ("password written to " GCR_SSH_ASKPASS_BIN);
		else
			g_message ("failed to write password to " GCR_SSH_ASKPASS_BIN);
		success = TRUE;
	} else if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("couldn't prompt for password: %s", error->message);
	} else {
		g_debug ("unhandled or cancelled ask password");
	}

out:
	if (!success) {
		g_debug ("writing failure to " GCR_SSH_ASKPASS_BIN);
		write_all (ctx->fd, (const guchar *)"\xff", 1);
	}
	if (password)
		g_object_unref (password);
	if (input)
		g_string_free (input, TRUE);
	g_clear_error (&error);

	g_close (ctx->fd, NULL);
	g_object_unref (ctx->interaction);
	g_object_unref (ctx->cancellable);
	g_free (ctx);

	return NULL;
}

static gboolean
askpass_accept (gint fd,
                GIOCondition cond,
                gpointer user_data)
{
	GcrSshAskpass *self = user_data;
	AskpassContext *ctx;
	struct sockaddr_un addr;
	socklen_t addrlen;
	GThread *thread;
	gint new_fd;

	addrlen = sizeof (addr);
	new_fd = accept (fd, (struct sockaddr *) &addr, &addrlen);
	if (new_fd < 0) {
		if (errno != EAGAIN && errno != EINTR)
			g_warning ("couldn't accept new control request: %s", g_strerror (errno));
		return TRUE;
	}

	g_debug ("accepted new connection from " GCR_SSH_ASKPASS_BIN);

	ctx = g_new0 (AskpassContext, 1);
	ctx->fd = new_fd;
	ctx->interaction = g_object_ref (self->interaction);
	ctx->cancellable = g_object_ref (self->cancellable);

	thread = g_thread_new ("ssh-askpass", askpass_thread, ctx);
	g_thread_unref (thread);

	return TRUE;
}

static void
gcr_ssh_askpass_constructed (GObject *obj)
{
	GcrSshAskpass *self = GCR_SSH_ASKPASS (obj);
	struct sockaddr_un addr;

	G_OBJECT_CLASS (gcr_ssh_askpass_parent_class)->constructed (obj);

	self->directory = g_build_filename (g_get_user_runtime_dir (), "ssh-askpass.XXXXXX", NULL);
	if (!g_mkdtemp_full (self->directory, 0700)) {
		g_warning ("couldn't create temporary directory: %s: %s", self->directory, g_strerror (errno));
		return;
	}

	self->socket = g_build_filename (self->directory, "socket", NULL);

	self->fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (self->fd < 0) {
		g_warning ("couldn't open socket: %s", g_strerror (errno));
		return;
	}

	if (!g_unix_set_fd_nonblocking (self->fd, TRUE, NULL))
		g_return_if_reached ();

	memset (&addr, 0, sizeof (addr));
	addr.sun_family = AF_UNIX;
	g_strlcpy (addr.sun_path, self->socket, sizeof (addr.sun_path));
	if (bind (self->fd, (struct sockaddr*) &addr, sizeof (addr)) < 0) {
		g_warning ("couldn't bind to askpass socket: %s: %s", self->socket, g_strerror (errno));
		return;
	}

	if (listen (self->fd, 128) < 0) {
		g_warning ("couldn't listen on askpass socket: %s: %s", self->socket, g_strerror (errno));
		return;
	}

	g_debug ("listening for " GCR_SSH_ASKPASS_BIN " at: %s", self->socket);

	self->source = g_unix_fd_add (self->fd, G_IO_IN, askpass_accept, self);
}

static void
gcr_ssh_askpass_dispose (GObject *obj)
{
	GcrSshAskpass *self = GCR_SSH_ASKPASS (obj);

	g_cancellable_cancel (self->cancellable);

	if (self->source) {
		g_source_remove (self->source);
		self->source = 0;
	}

	if (self->fd >= 0) {
		g_close (self->fd, NULL);
		self->fd = -1;
	}

	if (self->socket) {
		g_unlink (self->socket);
		g_free (self->socket);
		self->socket = NULL;
	}

	if (self->directory) {
		g_rmdir (self->directory);
		g_free (self->directory);
		self->directory = NULL;
	}

	if (self->interaction) {
		g_object_unref (self->interaction);
		self->interaction = NULL;
	}

	G_OBJECT_CLASS (gcr_ssh_askpass_parent_class)->dispose (obj);
}

static void
gcr_ssh_askpass_finalize (GObject *obj)
{
	GcrSshAskpass *self = GCR_SSH_ASKPASS (obj);

	g_object_unref (self->cancellable);
	g_main_context_unref (self->context);

	G_OBJECT_CLASS (gcr_ssh_askpass_parent_class)->finalize (obj);
}

/**
 * gcr_ssh_askpass_new:
 * @interaction: the interaction to use for prompting paswords
 *
 * Create a new GcrSshAskpass object which can be used to spawn an
 * ssh command and prompt for any necessary passwords.
 *
 * Use the gcr_ssh_askpass_child_setup() function as a callback with
 * g_spawn_sync(), g_spawn_async() or g_spawn_async_with_pipes().
 *
 * Returns: (transfer full): A new #GcrSshAskpass object
 */
GcrSshAskpass *
gcr_ssh_askpass_new (GTlsInteraction *interaction)
{
	g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), NULL);
	return g_object_new (GCR_TYPE_SSH_ASKPASS,
	                     "interaction", interaction,
	                     NULL);
}

/**
 * gcr_ssh_askpass_get_interaction:
 * @self: a #GcrSshAskpass object
 *
 * Get the interaction associated with this object.
 *
 * Returns: (transfer none): the interaction
 */
GTlsInteraction *
gcr_ssh_askpass_get_interaction (GcrSshAskpass *self)
{
	g_return_val_if_fail (GCR_IS_SSH_ASKPASS (self), NULL);
	return self->interaction;
}

/**
 * gcr_ssh_askpass_child_setup:
 * @askpass: a #GcrSshAskpass object
 *
 * Use this function as a callback setup function passed to g_spawn_sync(),
 * g_spawn_async(), g_spawn_async_with_pipes().
 */
void
gcr_ssh_askpass_child_setup (gpointer askpass)
{
	GcrSshAskpass *self = askpass;

	g_setenv ("SSH_ASKPASS", gcr_ssh_askpass_executable, TRUE);

	/* ssh wants DISPLAY set in order to use SSH_ASKPASS */
	if (!g_getenv ("DISPLAY"))
		g_setenv ("DISPLAY", "x", TRUE);

	/* For communicating back with ourselves */
	if (self->socket)
		g_setenv ("GCR_SSH_ASKPASS_SOCKET", self->socket, TRUE);

	/* Close the control terminal */
	setsid ();
}

static void
gcr_ssh_askpass_class_init (GcrSshAskpassClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gcr_ssh_askpass_get_property;
	gobject_class->set_property = gcr_ssh_askpass_set_property;
	gobject_class->constructed = gcr_ssh_askpass_constructed;
	gobject_class->dispose = gcr_ssh_askpass_dispose;
	gobject_class->finalize = gcr_ssh_askpass_finalize;

	/**
	 * GcrSshAskpass:interaction:
	 *
	 * The interaction used to prompt for passwords.
	 */
	g_object_class_install_property (gobject_class, PROP_INTERACTION,
	           g_param_spec_object ("interaction", "Interaction", "Interaction",
	                                G_TYPE_TLS_INTERACTION,
	                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

#ifdef GCR_SSH_ASKPASS_TOOL

#include "egg/egg-secure-memory.h"

EGG_SECURE_DEFINE_GLIB_GLOBALS ();
EGG_SECURE_DECLARE ("ssh-askpass");

int
main (int argc,
      char *argv[])
{
	GString *message;
	struct sockaddr_un addr;
	const gchar *path;
	guchar *buf;
	gint count;
	gint i;
	int ret;
	int fd;

	path = g_getenv ("GCR_SSH_ASKPASS_SOCKET");
	if (path == NULL) {
		g_printerr (GCR_SSH_ASKPASS_BIN ": this program is not meant to be run directly");
		return 2;
	}

	fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		g_warning ("couldn't open socket: %s", g_strerror (errno));
		return -1;
	}

	memset (&addr, 0, sizeof (addr));
	addr.sun_family = AF_UNIX;
	g_strlcpy (addr.sun_path, path, sizeof (addr.sun_path));
	if (connect (fd, (struct sockaddr*) &addr, sizeof (addr)) < 0) {
		g_warning ("couldn't connect to askpass socket: %s: %s", path, g_strerror (errno));
		return -1;
	}

	message = g_string_new ("");
	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			if (i == 1)
				g_string_append_c (message, ' ');
			g_string_append (message, argv[i]);
		}
	}

	if (!write_all (fd, (const guchar *)message->str, message->len)) {
		g_string_free (message, TRUE);
		return -1;
	}
	g_string_free (message, TRUE);

	if (shutdown (fd, SHUT_WR) < 0) {
		g_warning ("couldn't shutdown socket: %s", g_strerror (errno));
		return -1;
	}

	count = 0;
	buf = egg_secure_alloc (128);

	for (;;) {
		ret = read (fd, buf, 128);
		if (ret < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				if (errno != ECONNRESET) {
					g_critical ("couldn't read from ssh-askpass socket: %s",
					            g_strerror (errno));
				}
				egg_secure_free (buf);
				return -1;
			}
			ret = 0;
		} else if (ret == 0) {
			break;
		} else if (!write_all (1, buf, ret)) {
			egg_secure_free (buf);
			return -1;
		}
		count += ret;
	}

	if (count == 1 && buf[0] == 0xff) {
		egg_secure_free (buf);
		return -1;
	}

	egg_secure_free (buf);
	return 0;
}

#endif /* GCR_SSH_ASKPASS_TOOL */
