/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Stefan Walter
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
 * Author: Stef Walter <stef@thewalter.net>
 */

#ifndef __GCR_DBUS_CONSTANTS_H__
#define __GCR_DBUS_CONSTANTS_H__

#include <glib.h>

G_BEGIN_DECLS

#define GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME            "org.gnome.keyring.SystemPrompter"
#define GCR_DBUS_PROMPTER_PRIVATE_BUS_NAME           "org.gnome.keyring.PrivatePrompter"
#define GCR_DBUS_PROMPTER_MOCK_BUS_NAME              "org.gnome.keyring.MockPrompter"

#define GCR_DBUS_PROMPTER_OBJECT_PATH                "/org/gnome/keyring/Prompter"
#define GCR_DBUS_PROMPT_OBJECT_PREFIX                "/org/gnome/keyring/Prompt"

#define GCR_DBUS_PROMPTER_INTERFACE                  "org.gnome.keyring.internal.Prompter"

#define GCR_DBUS_PROMPTER_METHOD_BEGIN               "BeginPrompting"
#define GCR_DBUS_PROMPTER_METHOD_STOP                "StopPrompting"
#define GCR_DBUS_PROMPTER_METHOD_PERFORM             "PerformPrompt"

#define GCR_DBUS_CALLBACK_INTERFACE                  "org.gnome.keyring.internal.Prompter.Callback"

#define GCR_DBUS_PROMPT_ERROR_IN_PROGRESS            "org.gnome.keyring.Prompter.InProgress"
#define GCR_DBUS_PROMPT_ERROR_FAILED                 "org.gnome.keyring.Prompter.Failed"

#define GCR_DBUS_CALLBACK_METHOD_READY               "PromptReady"
#define GCR_DBUS_CALLBACK_METHOD_DONE                "PromptDone"

#define GCR_DBUS_PROMPT_TYPE_PASSWORD                "password"
#define GCR_DBUS_PROMPT_TYPE_CONFIRM                 "confirm"

#define GCR_DBUS_PROMPT_REPLY_NONE                   ""
#define GCR_DBUS_PROMPT_REPLY_YES                    "yes"
#define GCR_DBUS_PROMPT_REPLY_NO                     "no"

G_END_DECLS

#endif /* __GCR_DBUS_CONSTANTS_H__ */
