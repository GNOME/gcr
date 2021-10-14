/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GCR_RENDERER_H__
#define __GCR_RENDERER_H__

#include <glib-object.h>
#include <gck/gck.h>

G_BEGIN_DECLS

#define GCR_TYPE_RENDERER gcr_renderer_get_type ()
G_DECLARE_INTERFACE (GcrRenderer, gcr_renderer, GCK, RENDERER, GObject)

struct _GcrRendererInterface
{
  GTypeInterface g_iface;
};

G_END_DECLS

#endif /* __GCR_RENDERER_H__ */
