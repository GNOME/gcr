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

#ifndef GCR_SSH_AGENT_PROCESS_H
#define GCR_SSH_AGENT_PROCESS_H

#include <gio/gio.h>

#include "egg/egg-buffer.h"

#define GCR_TYPE_SSH_AGENT_PROCESS gcr_ssh_agent_process_get_type ()
G_DECLARE_FINAL_TYPE(GcrSshAgentProcess, gcr_ssh_agent_process, GCR, SSH_AGENT_PROCESS, GObject)

GcrSshAgentProcess *gcr_ssh_agent_process_new         (const gchar *path,
                                                       GStrv ssh_agent_args);
GSocketConnection  *gcr_ssh_agent_process_connect     (GcrSshAgentProcess *self,
                                                       GCancellable *cancellable,
                                                       GError **error);
GPid                gcr_ssh_agent_process_get_pid     (GcrSshAgentProcess *self);

#endif /* GCR_SSH_AGENT_PROCESS_H */
