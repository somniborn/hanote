// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SCALED_TYPE_PAINTABLE (scaled_paintable_get_type())
G_DECLARE_FINAL_TYPE(ScaledPaintable, scaled_paintable, SCALED, PAINTABLE, GObject)

ScaledPaintable *scaled_paintable_new(GdkTexture *texture,
                                      const char *filename,
                                      int width, int height);
void             scaled_paintable_set_size(ScaledPaintable *self,
                                           int width, int height);
const char      *scaled_paintable_get_filename(ScaledPaintable *self);
int              scaled_paintable_get_width(ScaledPaintable *self);
int              scaled_paintable_get_height(ScaledPaintable *self);
GdkTexture      *scaled_paintable_get_texture(ScaledPaintable *self);

/* Markdown image variant (display-only, not saved) */
ScaledPaintable *scaled_paintable_new_md(GdkTexture *texture,
                                         const char *filename,
                                         int width, int height);
gboolean         scaled_paintable_is_md_image(ScaledPaintable *self);

G_END_DECLS
