// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "scaled_paintable.h"

struct _ScaledPaintable {
    GObject       parent_instance;
    GdkTexture   *texture;
    char         *filename;
    int           width;
    int           height;
    gboolean      is_md_image;
};

static void scaled_paintable_iface_init(GdkPaintableInterface *iface);

G_DEFINE_TYPE_WITH_CODE(ScaledPaintable, scaled_paintable, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(GDK_TYPE_PAINTABLE, scaled_paintable_iface_init))

static void scaled_paintable_snapshot(GdkPaintable *paintable,
                                      GdkSnapshot  *snapshot,
                                      double        width,
                                      double        height) {
    ScaledPaintable *self = SCALED_PAINTABLE(paintable);
    if (!self->texture) return;
    gdk_paintable_snapshot(GDK_PAINTABLE(self->texture), snapshot, width, height);
}

static int scaled_paintable_get_intrinsic_width(GdkPaintable *paintable) {
    return SCALED_PAINTABLE(paintable)->width;
}

static int scaled_paintable_get_intrinsic_height(GdkPaintable *paintable) {
    return SCALED_PAINTABLE(paintable)->height;
}

static double scaled_paintable_get_intrinsic_aspect_ratio(GdkPaintable *paintable) {
    ScaledPaintable *self = SCALED_PAINTABLE(paintable);
    if (self->height == 0) return 0.0;
    return (double)self->width / (double)self->height;
}

static void scaled_paintable_iface_init(GdkPaintableInterface *iface) {
    iface->snapshot                 = scaled_paintable_snapshot;
    iface->get_intrinsic_width      = scaled_paintable_get_intrinsic_width;
    iface->get_intrinsic_height     = scaled_paintable_get_intrinsic_height;
    iface->get_intrinsic_aspect_ratio = scaled_paintable_get_intrinsic_aspect_ratio;
}

static void scaled_paintable_dispose(GObject *object) {
    ScaledPaintable *self = SCALED_PAINTABLE(object);
    g_clear_object(&self->texture);
    g_clear_pointer(&self->filename, g_free);
    G_OBJECT_CLASS(scaled_paintable_parent_class)->dispose(object);
}

static void scaled_paintable_class_init(ScaledPaintableClass *klass) {
    G_OBJECT_CLASS(klass)->dispose = scaled_paintable_dispose;
}

static void scaled_paintable_init(ScaledPaintable *self) {
    (void)self;
}

ScaledPaintable *scaled_paintable_new(GdkTexture *texture,
                                      const char *filename,
                                      int width, int height) {
    ScaledPaintable *self = g_object_new(SCALED_TYPE_PAINTABLE, NULL);
    self->texture  = g_object_ref(texture);
    self->filename = g_strdup(filename);
    self->width    = width;
    self->height   = height;
    return self;
}

void scaled_paintable_set_size(ScaledPaintable *self, int width, int height) {
    self->width  = width;
    self->height = height;
    gdk_paintable_invalidate_size(GDK_PAINTABLE(self));
}

const char *scaled_paintable_get_filename(ScaledPaintable *self) {
    return self->filename;
}

int scaled_paintable_get_width(ScaledPaintable *self) {
    return self->width;
}

int scaled_paintable_get_height(ScaledPaintable *self) {
    return self->height;
}

GdkTexture *scaled_paintable_get_texture(ScaledPaintable *self) {
    return self->texture;
}

ScaledPaintable *scaled_paintable_new_md(GdkTexture *texture,
                                         const char *filename,
                                         int width, int height) {
    ScaledPaintable *self = scaled_paintable_new(texture, filename, width, height);
    self->is_md_image = TRUE;
    return self;
}

gboolean scaled_paintable_is_md_image(ScaledPaintable *self) {
    return self->is_md_image;
}
