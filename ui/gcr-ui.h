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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef GCR_API_SUBJECT_TO_CHANGE
#error "This API has not yet reached stability."
#endif

#ifndef __GCR_UI_H__
#define __GCR_UI_H__

#include <glib.h>

#include <gcr/gcr-base.h>

#define __GCR_INSIDE_HEADER__

#include <ui/gcr-certificate-renderer.h>
#include <ui/gcr-certificate-widget.h>
#include <ui/gcr-collection-model.h>
#include <ui/gcr-combo-selector.h>
#include <ui/gcr-deprecated.h>
#include <ui/gcr-enum-types.h>
#include <ui/gcr-key-renderer.h>
#include <ui/gcr-key-widget.h>
#include <ui/gcr-failure-renderer.h>
#include <ui/gcr-key-renderer.h>
#include <ui/gcr-key-widget.h>
#include <ui/gcr-import-button.h>
#include <ui/gcr-list-selector.h>
#include <ui/gcr-prompt-dialog.h>
#include <ui/gcr-renderer.h>
#include <ui/gcr-secure-entry-buffer.h>
#include <ui/gcr-tree-selector.h>
#include <ui/gcr-unlock-options-widget.h>
#include <ui/gcr-viewer.h>
#include <ui/gcr-viewer-widget.h>

#undef __GCR_INSIDE_HEADER__

#endif /* __GCR_UI_H__ */
