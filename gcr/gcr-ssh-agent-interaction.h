/*
 * gnome-keyring
 *
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
 * Author: Daiki Ueno
 */

#ifndef GCR_SSH_AGENT_INTERACTION_H
#define GCR_SSH_AGENT_INTERACTION_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define GCR_TYPE_SSH_AGENT_INTERACTION gcr_ssh_agent_interaction_get_type ()
G_DECLARE_FINAL_TYPE (GcrSshAgentInteraction, gcr_ssh_agent_interaction, GCR, SSH_AGENT_INTERACTION, GTlsInteraction)

GTlsInteraction *gcr_ssh_agent_interaction_new (const gchar *prompter_name,
						const gchar *label,
						GHashTable *fields);

G_END_DECLS

#endif /* GCR_SSH_AGENT_INTERACTION_H */
