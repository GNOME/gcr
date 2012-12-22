/*
 * gnome-keyring
 *
 * Copyright (C) 2010 Collabora Ltd.
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef GCR_API_SUBJECT_TO_CHANGE
#error "This API has not yet reached stability."
#endif

#ifndef __GCR_UI_H__
#define __GCR_UI_H__

#include <glib.h>

#include "gcr-base.h"

#define __GCR_INSIDE_HEADER__

#include <gcr/gcr-certificate-renderer.h>
#include <gcr/gcr-certificate-widget.h>
#include <gcr/gcr-collection-model.h>
#include <gcr/gcr-column.h>
#include <gcr/gcr-combo-selector.h>
#include <gcr/gcr-deprecated.h>
#include <gcr/gcr-key-renderer.h>
#include <gcr/gcr-key-widget.h>
#include <gcr/gcr-enum-types.h>
#include <gcr/gcr-failure-renderer.h>
#include <gcr/gcr-key-renderer.h>
#include <gcr/gcr-key-widget.h>
#include <gcr/gcr-import-button.h>
#include <gcr/gcr-list-selector.h>
#include <gcr/gcr-prompt-dialog.h>
#include <gcr/gcr-renderer.h>
#include <gcr/gcr-secure-entry-buffer.h>
#include <gcr/gcr-tree-selector.h>
#include <gcr/gcr-union-collection.h>
#include <gcr/gcr-unlock-options-widget.h>
#include <gcr/gcr-viewer.h>
#include <gcr/gcr-viewer-widget.h>

#undef __GCR_INSIDE_HEADER__

#endif /* __GCR_UI_H__ */
