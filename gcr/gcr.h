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

#ifndef __GCR_H__
#define __GCR_H__

#include <glib.h>

#define __GCR_INSIDE_HEADER__

#include <gcr/gcr-types.h>

#include <gcr/gcr-certificate.h>
#include <gcr/gcr-certificate-chain.h>
#include <gcr/gcr-certificate-field.h>
#include <gcr/gcr-certificate-request.h>
#include <gcr/gcr-certificate-section.h>
#include <gcr/gcr-enum-types.h>
#include <gcr/gcr-fingerprint.h>
#include <gcr/gcr-importer.h>
#include <gcr/gcr-library.h>
#include <gcr/gcr-mock-prompter.h>
#include <gcr/gcr-parser.h>
#include <gcr/gcr-pkcs11-certificate.h>
#include <gcr/gcr-prompt.h>
#include <gcr/gcr-secret-exchange.h>
#include <gcr/gcr-secure-memory.h>
#include <gcr/gcr-simple-certificate.h>
#include <gcr/gcr-ssh-askpass.h>
#include <gcr/gcr-system-prompt.h>
#include <gcr/gcr-system-prompter.h>
#include <gcr/gcr-trust.h>
#include <gcr/gcr-unlock-options.h>
#include <gcr/gcr-version.h>

#undef __GCR_INSIDE_HEADER__

#endif /* __GCR_H__ */
