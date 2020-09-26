/*
 * Copyright (C) 2021 Red Hat, Inc.
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
 * Auther: Daiki Ueno
 */

#include "gcr-ssh-agent-preload.h"
#include "gcr-ssh-agent-service.h"

int
main (int argc,
      char **argv)
{
	const gchar *base_dir;
	GcrSshAgentPreload *preload;
	GcrSshAgentService *service;
	GMainLoop *loop;

	if (argc != 2) {
		g_printerr ("Usage: gcr-ssh-agent <base-dir>");
		return EXIT_FAILURE;
	}

	base_dir = argv[1];
	if (!g_path_is_absolute (base_dir)) {
		g_printerr ("gcr-ssh-agent: base-dir must be specified as an absolute path");
		return EXIT_FAILURE;
	}

	preload = gcr_ssh_agent_preload_new ("~/.ssh");
	service = gcr_ssh_agent_service_new (base_dir, preload);
	g_object_unref (preload);

	if (!gcr_ssh_agent_service_start (service))
		return EXIT_FAILURE;

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	return EXIT_SUCCESS;
}
