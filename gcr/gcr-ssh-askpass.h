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
 * Author: Stef Walter <stefw@gnome.org>
 */

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_SSH_ASKPASS_H__
#define __GCR_SSH_ASKPASS_H__

#include <glib-object.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define GCR_TYPE_SSH_ASKPASS               (gcr_ssh_askpass_get_type ())
#define GCR_SSH_ASKPASS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_SSH_ASKPASS, GcrSshAskpass))
#define GCR_SSH_ASKPASS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_SSH_ASKPASS, GcrSshAskpassClass))
#define GCR_IS_SSH_ASKPASS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_SSH_ASKPASS))
#define GCR_IS_SSH_ASKPASS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_SSH_ASKPASS))
#define GCR_SSH_ASKPASS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_SSH_ASKPASS, GcrSshAskpassClass))

typedef struct _GcrSshAskpass GcrSshAskpass;
typedef struct _GcrSshAskpassClass GcrSshAskpassClass;

GType               gcr_ssh_askpass_get_type        (void);

GcrSshAskpass *     gcr_ssh_askpass_new             (GTlsInteraction *interaction);

GTlsInteraction *   gcr_ssh_askpass_get_interaction (GcrSshAskpass *self);

void                gcr_ssh_askpass_child_setup     (gpointer askpass);

G_END_DECLS

#endif /* __GCR_SSH_ASKPASS_H__ */
