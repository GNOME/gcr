/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gkd-ssh-agent-private.h - Private SSH agent declarations

   Copyright (C) 2007 Stefan Walter

   Gnome keyring is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   Gnome keyring is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Author: Stef Walter <stef@memberwebs.com>
*/

#ifndef GCRSSHPRIVATE_H_
#define GCRSSHPRIVATE_H_

/* -----------------------------------------------------------------------------
 * SSH OPERATIONS and CONSTANTS
 */

/* Requests from client to daemon */
#define GCR_SSH_OP_REQUEST_RSA_IDENTITIES               1
#define GCR_SSH_OP_RSA_CHALLENGE                        3
#define GCR_SSH_OP_ADD_RSA_IDENTITY                     7
#define GCR_SSH_OP_REMOVE_RSA_IDENTITY                  8
#define GCR_SSH_OP_REMOVE_ALL_RSA_IDENTITIES            9
#define GCR_SSH_OP_REQUEST_IDENTITIES                   11
#define GCR_SSH_OP_SIGN_REQUEST                         13
#define GCR_SSH_OP_ADD_IDENTITY                         17
#define GCR_SSH_OP_REMOVE_IDENTITY                      18
#define GCR_SSH_OP_REMOVE_ALL_IDENTITIES                19
#define GCR_SSH_OP_ADD_SMARTCARD_KEY                    20
#define GCR_SSH_OP_REMOVE_SMARTCARD_KEY                 21
#define GCR_SSH_OP_LOCK                                 22
#define GCR_SSH_OP_UNLOCK                               23
#define GCR_SSH_OP_ADD_RSA_ID_CONSTRAINED               24
#define GCR_SSH_OP_ADD_ID_CONSTRAINED                   25
#define GCR_SSH_OP_ADD_SMARTCARD_KEY_CONSTRAINED        26

#define GCR_SSH_OP_MAX                                  27

/* Responses from daemon to client */
#define GCR_SSH_RES_RSA_IDENTITIES_ANSWER               2
#define GCR_SSH_RES_RSA_RESPONSE                        4
#define GCR_SSH_RES_FAILURE                             5
#define GCR_SSH_RES_SUCCESS                             6
#define GCR_SSH_RES_IDENTITIES_ANSWER                   12
#define GCR_SSH_RES_SIGN_RESPONSE                       14
#define GCR_SSH_RES_EXTENDED_FAILURE                    30
#define GCR_SSH_RES_SSHCOM_FAILURE                      102


#define	GCR_SSH_FLAG_CONSTRAIN_LIFETIME                 1
#define	GCR_SSH_FLAG_CONSTRAIN_CONFIRM                  2

#define GCR_SSH_DSA_SIGNATURE_PADDING                   20
#define	GCR_SSH_FLAG_OLD_SIGNATURE                      0x01
#define	GCR_SSH_FLAG_RSA_SHA2_256                       0x02
#define	GCR_SSH_FLAG_RSA_SHA2_512                       0x04

#endif /*GCRSSHPRIVATE_H_*/
