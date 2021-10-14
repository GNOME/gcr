/*
 * Copyright 2021 Collabora Ltd.
 * Copyright Corentin Noël <corentin.noel@collabora.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gcr-gtk4/gcr-renderer.h>

G_DEFINE_INTERFACE (GcrRenderer, gcr_renderer, G_TYPE_OBJECT)

static void
gcr_renderer_default_init (GcrRendererInterface *iface)
{
  /* add properties and signals to the interface here */
}

