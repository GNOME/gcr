/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gck.h - the GObject PKCS#11 wrapper library

   Copyright (C) 2008, Stefan Walter

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

   Author: Stef Walter <nielsen@memberwebs.com>
*/

#ifndef GCK_H
#define GCK_H

#define __GCK_INSIDE_HEADER__

/*
 * To use this API, you need to be prepared for changes to the API,
 * and add the C flag: -DGCK_API_SUBJECT_TO_CHANGE
 */

#ifndef __GI_SCANNER__
#ifndef GCK_API_SUBJECT_TO_CHANGE
#error "This API has not yet reached stability."
#endif
#endif

#include <gck/gck-enum-types.h>
#include <gck/gck-version.h>
#include <gck/gck-attributes.h>
#include <gck/gck-module.h>
#include <gck/gck-enumerator.h>
#include <gck/gck-slot.h>
#include <gck/gck-session.h>
#include <gck/gck-object.h>
#include <gck/gck-object-cache.h>
#include <gck/gck-password.h>
#include <gck/gck-uri.h>

#undef __GCK_INSIDE_HEADER__

#endif /* GCK_H */
