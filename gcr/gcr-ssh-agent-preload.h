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

#ifndef GCR_SSH_AGENT_PRELOAD_H
#define GCR_SSH_AGENT_PRELOAD_H

#include <glib-object.h>

typedef struct {
	gchar *filename;
	GBytes *public_key;
	gchar *comment;
} GcrSshAgentKeyInfo;

void                gcr_ssh_agent_key_info_free    (gpointer boxed);
gpointer            gcr_ssh_agent_key_info_copy    (gpointer boxed);

#define GCR_TYPE_SSH_AGENT_PRELOAD gcr_ssh_agent_preload_get_type ()
G_DECLARE_FINAL_TYPE (GcrSshAgentPreload, gcr_ssh_agent_preload, GCR, SSH_AGENT_PRELOAD, GObject)

GcrSshAgentPreload *gcr_ssh_agent_preload_new      (const gchar *path);

GList              *gcr_ssh_agent_preload_get_keys (GcrSshAgentPreload *self);

GcrSshAgentKeyInfo *gcr_ssh_agent_preload_lookup_by_public_key
                                                   (GcrSshAgentPreload *self,
                                                    GBytes *public_key);

#endif /* GCR_SSH_AGENT_PRELOAD_H */

