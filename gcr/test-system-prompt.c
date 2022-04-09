/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
   Copyright (C) 2011 Collabora Ltd

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

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr/gcr.h"
#include "gcr/gcr-mock-prompter.h"

#include "egg/egg-testing.h"

#include <glib.h>

#define g_assert_cmpstr_free(a, op, b) G_STMT_START { \
        char *lhs_str = a;                            \
        char *rhs_str = b;                            \
        g_assert_cmpstr (lhs_str, op, rhs_str);       \
        g_free (lhs_str);                             \
        g_free (rhs_str);                             \
} G_STMT_END

typedef struct {
	const gchar *prompter_name;
} Test;

static void
setup (Test *test,
       gconstpointer unused)
{
	test->prompter_name = gcr_mock_prompter_start ();
}

static void
teardown (Test *test,
          gconstpointer unused)
{
	gcr_mock_prompter_stop ();
}

static void
test_open_prompt (Test *test,
                  gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;
	gchar *bus_name;

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	g_object_get (prompt, "bus-name", &bus_name, NULL);
	g_assert_cmpstr (bus_name, ==, test->prompter_name);

	ret = gcr_system_prompt_close (GCR_SYSTEM_PROMPT (prompt), NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_free (bus_name);
	g_object_unref (prompt);
}

static void
test_open_failure (Test *test,
                   gconstpointer unused)
{
	GcrPrompt *prompt;
	GDBusConnection *connection;
	GError *error = NULL;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error (error);

	/* Try to open a prompt where no prompter is running */

	prompt = gcr_system_prompt_open_for_prompter (g_dbus_connection_get_unique_name (connection),
	                                              0, NULL, &error);
	g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
	g_assert (prompt == NULL);

	g_error_free (error);
	g_object_unref (connection);
}

static void
test_prompt_password (Test *test,
                      gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password;

	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	password = gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (password, ==, "booo");

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_password_in_exchange (Test *test,
                           gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	GcrSecretExchange *exchange;

	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);

	g_object_get (prompt, "secret-exchange", &exchange, NULL);
	g_assert (GCR_IS_SECRET_EXCHANGE (exchange));
	g_assert_cmpstr (gcr_secret_exchange_get_secret (exchange, NULL), ==, "booo");

	g_object_unref (exchange);
	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_password_custom_exchange (Test *test,
                               gconstpointer unused)
{
	GcrSystemPrompt *prompt;
	GError *error = NULL;
	GcrSecretExchange *exchange;
	const gchar *password;

	exchange = gcr_secret_exchange_new (NULL);
	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = g_initable_new (GCR_TYPE_SYSTEM_PROMPT, NULL, &error,
	                         "timeout-seconds", 0,
	                         "bus-name", test->prompter_name,
	                         "secret-exchange", exchange,
	                         NULL);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	password = gcr_prompt_password_run (GCR_PROMPT (prompt), NULL, &error);
	g_assert_cmpstr (password, ==, "booo");
	g_assert_no_error (error);

	password = gcr_secret_exchange_get_secret (exchange, NULL);
	g_assert_cmpstr (password, ==, "booo");

	g_object_unref (exchange);
	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
on_async_result (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GAsyncResult **ret = user_data;
	*ret = g_object_ref (result);
	egg_test_wait_stop ();
}

static void
test_async_password (Test *test,
                     gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password;

	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	gcr_system_prompt_open_for_prompter_async (test->prompter_name, 0, NULL,
	                                           on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	g_assert (result != NULL);
	prompt = gcr_system_prompt_open_finish (result, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_clear_object (&result);
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	gcr_prompt_password_async (prompt, NULL,
	                           on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	password = gcr_prompt_password_finish (prompt, result, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (password, ==, "booo");
	g_clear_object (&result);

	g_object_unref (prompt);
}

static void
test_prompt_confirm (Test *test,
                     gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_ok (NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_async_confirm (Test *test,
                    gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean confirm;

	gcr_mock_prompter_expect_confirm_ok (NULL);

	gcr_system_prompt_open_for_prompter_async (test->prompter_name, 0, NULL,
	                                           on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	g_assert (result != NULL);
	prompt = gcr_system_prompt_open_finish (result, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_clear_object (&result);
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	gcr_prompt_confirm_async (prompt, NULL, on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	confirm = gcr_prompt_confirm_finish (prompt, result, &error);
	g_assert_no_error (error);
	g_assert (confirm == TRUE);
	g_clear_object (&result);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_cancel_password (Test *test,
                      gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password;

	gcr_mock_prompter_expect_password_cancel ();

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	password = gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (password, ==, NULL);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_cancel_confirm (Test *test,
                     gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_cancel ();

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == FALSE);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_prompt_properties (Test *test,
                        gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_ok ("title", "My Title",
	                                     "description", "My Description",
	                                     "warning", "My Warning",
	                                     "message", "My Message",
	                                     "caller-window", "01010",
	                                     "choice-label", "My Choice",
	                                     "choice-chosen", TRUE,
	                                     "password-new", TRUE,
	                                     "password-strength", 0,
	                                     "continue-label", "My Continue",
	                                     "cancel-label", "My Cancel",
	                                     NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	g_object_set (prompt,
	              "title", "Other Title",
	              "choice-label", "Other Choice",
	              "description", "Other Description",
	              "message", "Other Message",
	              "caller-window", "01012",
	              "warning", "Other Warning",
	              "password-new", FALSE,
	              "choice-chosen", TRUE,
	              "continue-label", "Other Continue",
	              "cancel-label", "Other Cancel",
	              NULL);

	g_assert_cmpstr_free (gcr_prompt_get_title (prompt), ==, g_strdup ("Other Title"));
	g_assert_cmpstr_free (gcr_prompt_get_choice_label (prompt), ==, g_strdup ("Other Choice"));
	g_assert_cmpstr_free (gcr_prompt_get_description (prompt), ==, g_strdup ("Other Description"));
	g_assert_cmpstr_free (gcr_prompt_get_message (prompt), ==, g_strdup ("Other Message"));
	g_assert_cmpstr_free (gcr_prompt_get_caller_window (prompt), ==, g_strdup ("01012"));
	g_assert_cmpstr_free (gcr_prompt_get_warning (prompt), ==, g_strdup ("Other Warning"));
	g_assert_cmpstr_free (gcr_prompt_get_continue_label (prompt), ==, g_strdup ("Other Continue"));
	g_assert_cmpstr_free (gcr_prompt_get_cancel_label (prompt), ==, g_strdup ("Other Cancel"));
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == TRUE);

	gcr_prompt_set_title (prompt, "My Title");
	gcr_prompt_set_choice_label (prompt, "My Choice");
	gcr_prompt_set_description (prompt, "My Description");
	gcr_prompt_set_message (prompt, "My Message");
	gcr_prompt_set_caller_window (prompt, "01010");
	gcr_prompt_set_warning (prompt, "My Warning");
	gcr_prompt_set_continue_label (prompt, "My Continue");
	gcr_prompt_set_cancel_label (prompt, "My Cancel");
	gcr_prompt_set_password_new (prompt, TRUE);
	gcr_prompt_set_choice_chosen (prompt, TRUE);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_assert (gcr_prompt_get_choice_chosen (prompt) == TRUE);
	g_assert_cmpint (gcr_prompt_get_password_strength (prompt), ==, 0);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_prompt_properties_unset (Test *test,
                              gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	g_assert_cmpstr_free (gcr_prompt_get_title (prompt), ==, g_strdup (""));
	g_assert_cmpstr_free (gcr_prompt_get_choice_label (prompt), ==, NULL);
	g_assert_cmpstr_free(gcr_prompt_get_description (prompt), ==, g_strdup (""));
	g_assert_cmpstr_free (gcr_prompt_get_message (prompt), ==, g_strdup (""));
	g_assert_cmpstr_free (gcr_prompt_get_caller_window (prompt), ==, NULL);
	g_assert_cmpstr_free (gcr_prompt_get_warning (prompt), ==, NULL);
	g_assert_cmpstr_free (gcr_prompt_get_continue_label (prompt), ==, g_strdup ("Continue"));
	g_assert_cmpstr_free (gcr_prompt_get_cancel_label (prompt), ==, g_strdup ("Cancel"));
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == FALSE);
	g_assert_cmpint (gcr_prompt_get_password_strength (prompt), ==, 0);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}


static void
test_prompt_properties_reset (Test *test,
                              gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	g_object_set (prompt,
	              "title", "Other Title",
	              "choice-label", "Other Choice",
	              "description", "Other Description",
	              "message", "Other Message",
	              "caller-window", "01012",
	              "warning", "Other Warning",
	              "password-new", FALSE,
	              "choice-chosen", TRUE,
	              "continue-label", "Other Continue",
	              "cancel-label", "Other Cancel",
	              NULL);

	g_assert_cmpstr_free (gcr_prompt_get_title (prompt), ==, g_strdup ("Other Title"));
	g_assert_cmpstr_free (gcr_prompt_get_choice_label (prompt), ==, g_strdup ("Other Choice"));
	g_assert_cmpstr_free (gcr_prompt_get_description (prompt), ==, g_strdup ("Other Description"));
	g_assert_cmpstr_free (gcr_prompt_get_message (prompt), ==, g_strdup ("Other Message"));
	g_assert_cmpstr_free (gcr_prompt_get_caller_window (prompt), ==, g_strdup ("01012"));
	g_assert_cmpstr_free (gcr_prompt_get_warning (prompt), ==, g_strdup ("Other Warning"));
	g_assert_cmpstr_free (gcr_prompt_get_continue_label (prompt), ==, g_strdup ("Other Continue"));
	g_assert_cmpstr_free (gcr_prompt_get_cancel_label (prompt), ==, g_strdup ("Other Cancel"));
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == TRUE);

	gcr_prompt_reset (prompt);

	g_assert_cmpstr_free (gcr_prompt_get_title (prompt), ==, g_strdup (""));
	g_assert_cmpstr_free (gcr_prompt_get_choice_label (prompt), ==, NULL);
	g_assert_cmpstr_free (gcr_prompt_get_description (prompt), ==, g_strdup (""));
	g_assert_cmpstr_free (gcr_prompt_get_message (prompt), ==, g_strdup (""));
	g_assert_cmpstr_free (gcr_prompt_get_caller_window (prompt), ==, NULL);
	g_assert_cmpstr_free (gcr_prompt_get_warning (prompt), ==, NULL);
	g_assert_cmpstr_free (gcr_prompt_get_continue_label (prompt), ==, g_strdup ("Continue"));
	g_assert_cmpstr_free (gcr_prompt_get_cancel_label (prompt), ==, g_strdup ("Cancel"));
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == FALSE);
	g_assert_cmpint (gcr_prompt_get_password_strength (prompt), ==, 0);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_prompt_close (Test *test,
                   gconstpointer unused)
{
	GcrPrompt *prompt;
	GcrPrompt *prompt2;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_ok (NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	prompt2 = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_error (error, GCR_SYSTEM_PROMPT_ERROR, GCR_SYSTEM_PROMPT_IN_PROGRESS);
	g_clear_error (&error);
	g_assert (prompt2 == NULL);
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	prompt2 = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_error (error, GCR_SYSTEM_PROMPT_ERROR, GCR_SYSTEM_PROMPT_IN_PROGRESS);
	g_clear_error (&error);
	g_assert (prompt2 == NULL);

	gcr_system_prompt_close (GCR_SYSTEM_PROMPT (prompt), NULL, &error);
	g_assert_no_error (error);

	prompt2 = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt2));

	g_object_unref (prompt);

	g_object_unref (prompt2);
	g_assert (prompt == NULL);
}

static void
on_prompt_close (GcrPrompt *prompt,
                 gpointer user_data)
{
	gboolean *prompt_closed = (gboolean *)user_data;
	g_assert (*prompt_closed == FALSE);
	*prompt_closed = TRUE;
}

static void
test_close_cancels (Test *test,
                    gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password = NULL;
	GAsyncResult *result = NULL;
	gboolean prompt_closed;

	gcr_mock_prompter_set_delay_msec (3000);
	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	prompt_closed = FALSE;
	g_signal_connect_after (prompt, "prompt-close", G_CALLBACK (on_prompt_close), &prompt_closed);

	gcr_prompt_password_async (prompt, NULL, on_async_result, &result);

	gcr_system_prompt_close (GCR_SYSTEM_PROMPT (prompt), NULL, &error);
	g_assert_no_error (error);

	g_assert (prompt_closed == TRUE);
	egg_test_wait ();

	password = gcr_prompt_password_finish (prompt, result, &error);
	g_assert_no_error (error);
	g_assert (password == NULL);
	g_clear_object (&result);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_close_from_prompter (Test *test,
                          gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;
	const gchar *password;
	gboolean prompt_closed;

	gcr_mock_prompter_expect_close ();

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	prompt_closed = FALSE;
	g_signal_connect_after (prompt, "prompt-close", G_CALLBACK (on_prompt_close), &prompt_closed);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == GCR_PROMPT_REPLY_CANCEL);

	/* The prompt should be closed now, these shouldn't reach the mock prompter */

	while (!prompt_closed)
		g_main_context_iteration (NULL, TRUE);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == GCR_PROMPT_REPLY_CANCEL);

	password = gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (password == NULL);

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

static void
test_after_close_dismisses (Test *test,
                            gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;
	const gchar *password;
	gboolean prompt_closed;

	gcr_mock_prompter_expect_confirm_ok (NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_object_add_weak_pointer (G_OBJECT (prompt), (gpointer *)&prompt);

	prompt_closed = FALSE;
	g_signal_connect_after (prompt, "prompt-close", G_CALLBACK (on_prompt_close), &prompt_closed);


	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == GCR_PROMPT_REPLY_CONTINUE);

	gcr_prompt_close (prompt);
	g_assert (prompt_closed);

	/* These should never even reach the mock prompter */

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == GCR_PROMPT_REPLY_CANCEL);

	password = gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (password == NULL);

	while (g_main_context_iteration (NULL, FALSE));

	g_object_unref (prompt);
	g_assert (prompt == NULL);
}

typedef struct {
	GAsyncResult *result1;
	GAsyncResult *result2;
} ResultPair;

static void
on_result_pair_one (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	ResultPair *pair = user_data;
	g_assert (pair->result1 == NULL);
	pair->result1 = g_object_ref (result);
	if (pair->result1 && pair->result2)
		egg_test_wait_stop ();
}

static void
on_result_pair_two (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	ResultPair *pair = user_data;
	g_assert (pair->result2 == NULL);
	pair->result2 = g_object_ref (result);
	if (pair->result1 && pair->result2)
		egg_test_wait_stop ();
}

static void
test_watch_cancels (Test *test,
                    gconstpointer unused)
{
	GcrPrompt *prompt;
	GcrPrompt *prompt2;
	GError *error = NULL;
	const gchar *password;
	ResultPair pair = { NULL, NULL };

	gcr_mock_prompter_set_delay_msec (3000);
	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	/* This should happen immediately */
	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	/* Show a password prompt */
	gcr_prompt_password_async (prompt, NULL, on_result_pair_one, &pair);

	/* This prompt should wait, block */
	gcr_system_prompt_open_for_prompter_async (test->prompter_name, 0, NULL,
	                                           on_result_pair_two, &pair);

	/* Wait a bit before stopping, so outgoing request is done */
	egg_test_wait_until (1000);

	/* Kill the mock prompter */
	gcr_mock_prompter_disconnect ();

	/* Both the above operations should cancel */
	egg_test_wait ();

	prompt2 = gcr_system_prompt_open_finish (pair.result2, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_clear_error (&error);
	g_assert (prompt2 == NULL);

	password = gcr_prompt_password_finish (prompt, pair.result1, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_clear_error (&error);
	g_assert (password == NULL);

	g_object_unref (prompt);
	g_object_unref (pair.result1);
	g_object_unref (pair.result2);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-system-prompt");

	g_test_add ("/gcr/system-prompt/open", Test, NULL, setup, test_open_prompt, teardown);
	g_test_add ("/gcr/system-prompt/open-failure", Test, NULL, setup, test_open_failure, teardown);
	g_test_add ("/gcr/system-prompt/password", Test, NULL, setup, test_prompt_password, teardown);
	g_test_add ("/gcr/system-prompt/password-async", Test, NULL, setup, test_async_password, teardown);
	g_test_add ("/gcr/system-prompt/password-cancel", Test, NULL, setup, test_cancel_password, teardown);
	g_test_add ("/gcr/system-prompt/password-in-exchange", Test, NULL, setup, test_password_in_exchange, teardown);
	g_test_add ("/gcr/system-prompt/password-custom-exchange", Test, NULL, setup, test_password_custom_exchange, teardown);
	g_test_add ("/gcr/system-prompt/confirm", Test, NULL, setup, test_prompt_confirm, teardown);
	g_test_add ("/gcr/system-prompt/confirm-async", Test, NULL, setup, test_async_confirm, teardown);
	g_test_add ("/gcr/system-prompt/confirm-cancel", Test, NULL, setup, test_cancel_confirm, teardown);
	g_test_add ("/gcr/system-prompt/properties", Test, NULL, setup, test_prompt_properties, teardown);
	g_test_add ("/gcr/system-prompt/properties-unset", Test, NULL, setup, test_prompt_properties_unset, teardown);
	g_test_add ("/gcr/system-prompt/properties-reset", Test, NULL, setup, test_prompt_properties_reset, teardown);
	g_test_add ("/gcr/system-prompt/close", Test, NULL, setup, test_prompt_close, teardown);
	g_test_add ("/gcr/system-prompt/close-cancels", Test, NULL, setup, test_close_cancels, teardown);
	g_test_add ("/gcr/system-prompt/after-close-dismisses", Test, NULL, setup, test_after_close_dismisses, teardown);
	g_test_add ("/gcr/system-prompt/close-from-prompter", Test, NULL, setup, test_close_from_prompter, teardown);
	g_test_add ("/gcr/system-prompt/watch-cancels", Test, NULL, setup, test_watch_cancels, teardown);

	return egg_tests_run_with_loop ();
}
