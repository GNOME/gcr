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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stef@thewalter.net>
 */

#ifndef __GCR_DBUS_CONSTANTS_H__
#define __GCR_DBUS_CONSTANTS_H__

#include <glib.h>

G_BEGIN_DECLS

#define GCR_DBUS_PROMPTER_BUS_NAME                   "org.gnome.keyring.Prompter"
#define GCR_DBUS_PROMPTER_MOCK_BUS_NAME              "org.gnome.keyring.MockPrompter"

#define GCR_DBUS_PROMPTER_OBJECT_PATH                "/org/gnome/keyring/Prompter"

#define GCR_DBUS_PROMPTER_INTERFACE                  "org.gnome.keyring.Prompter"

#define GCR_DBUS_PROMPTER_METHOD_BEGIN               "BeginPrompting"
#define GCR_DBUS_PROMPTER_METHOD_FINISH              "FinishPrompting"

#define GCR_DBUS_PROMPT_INTERFACE                    "org.gnome.keyring.Prompter.Prompt"

#define GCR_DBUS_PROMPT_ERROR_IN_PROGRESS            "org.gnome.keyring.Prompter.InProgress"
#define GCR_DBUS_PROMPT_ERROR_NOT_HAPPENING          "org.gnome.keyring.Prompter.NotHappening"
#define GCR_DBUS_PROMPT_ERROR_FAILED                 "org.gnome.keyring.Prompter.Failed"

#define GCR_DBUS_PROMPT_PROPERTY_TITLE               "Title"
#define GCR_DBUS_PROMPT_PROPERTY_MESSAGE             "Message"
#define GCR_DBUS_PROMPT_PROPERTY_DESCRIPTION         "Description"
#define GCR_DBUS_PROMPT_PROPERTY_WARNING             "Warning"
#define GCR_DBUS_PROMPT_PROPERTY_CHOICE_LABEL        "ChoiceLabel"
#define GCR_DBUS_PROMPT_PROPERTY_CHOICE_CHOSEN       "ChoiceChosen"
#define GCR_DBUS_PROMPT_PROPERTY_PASSWORD_NEW        "PasswordNew"
#define GCR_DBUS_PROMPT_PROPERTY_PASSWORD_STRENGTH   "PasswordStrength"
#define GCR_DBUS_PROMPT_PROPERTY_CALLER_WINDOW       "CallerWindow"

#define GCR_DBUS_PROMPT_METHOD_PASSWORD              "RequestPassword"
#define GCR_DBUS_PROMPT_METHOD_CONFIRM               "RequestConfirm"

G_END_DECLS

#endif /* __GCR_DBUS_CONSTANTS_H__ */
