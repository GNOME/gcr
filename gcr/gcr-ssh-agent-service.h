/*
 * gnome-keyring
 *
 * Copyright (C) 2007 Stefan Walter
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

#ifndef GCR_SSH_AGENT_SERVICE_H
#define GCR_SSH_AGENT_SERVICE_H

#include <gio/gio.h>
#include "gcr-ssh-agent-preload.h"
#include "gcr-ssh-agent-process.h"
#include "egg/egg-buffer.h"

#define GCR_TYPE_SSH_AGENT_SERVICE gcr_ssh_agent_service_get_type ()
G_DECLARE_FINAL_TYPE (GcrSshAgentService, gcr_ssh_agent_service, GCR, SSH_AGENT_SERVICE, GObject);

GcrSshAgentService *gcr_ssh_agent_service_new  (const gchar        *path,
                                                GStrv              ssh_agent_args,
                                                GcrSshAgentPreload *preload);

gboolean            gcr_ssh_agent_service_start
                                               (GcrSshAgentService *self);

void                gcr_ssh_agent_service_stop (GcrSshAgentService *self);

GcrSshAgentPreload *gcr_ssh_agent_service_get_preload
                                               (GcrSshAgentService *self);

GcrSshAgentProcess *gcr_ssh_agent_service_get_process
                                               (GcrSshAgentService *self);

gboolean            gcr_ssh_agent_service_lookup_key
                                               (GcrSshAgentService *self,
                                                GBytes             *key);

#endif /* GCR_SSH_AGENT_SERVICE_H */
