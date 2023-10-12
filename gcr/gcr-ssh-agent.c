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

static gchar *base_dir;
static gchar **ssh_agent_args = NULL;

/* gcr-ssh-agent --base-dir <base_dir> -- [ssh-agent options]
 * gcr-ssh-agent --base-dir /home/user/test/ -- -c -d */

static GOptionEntry opt_entries[] = {
        { "base-dir", 'd', 0, G_OPTION_ARG_FILENAME, &base_dir, "Base directory", NULL },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &ssh_agent_args,
          "ssh-agent options", NULL },
        { NULL }
};

void
parse_arguments (int *argc, char **argv[])
{
        GError *error = NULL;
        GOptionContext *opt_context;

        opt_context = g_option_context_new ("-- ssh-agent options");
        g_option_context_add_main_entries (opt_context, opt_entries, NULL);

        if (!g_option_context_parse (opt_context, argc, argv, &error)) {
                g_printerr ("option parsing failed: %s\n", error->message);
                g_clear_error (&error);
        }

        g_option_context_free (opt_context);
}

int
main (int argc,
      char **argv)
{
	GcrSshAgentPreload *preload;
	GcrSshAgentService *service;
	GMainLoop *loop;

        parse_arguments(&argc, &argv);

	if (!base_dir) {
		g_printerr ("Usage: gcr-ssh-agent --base-dir <base-dir> --\n");
		return EXIT_FAILURE;
	}

	if (!g_path_is_absolute (base_dir)) {
		g_printerr ("gcr-ssh-agent: base-dir must be specified as an absolute path\n");
		return EXIT_FAILURE;
	}

	preload = gcr_ssh_agent_preload_new ("~/.ssh");
	service = gcr_ssh_agent_service_new (base_dir, ssh_agent_args, preload);
        g_free (ssh_agent_args);
	g_object_unref (preload);

	if (!gcr_ssh_agent_service_start (service))
		return EXIT_FAILURE;

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	return EXIT_SUCCESS;
}
