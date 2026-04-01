// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <gtk/gtk.h>

typedef struct {
    const char *name;
    const char *hex;
} NoteColor;

typedef struct {
    char *filename;
    int   offset;
    int   width;
    int   height;
} NoteImage;

typedef struct {
    char     *id;
    char     *text;
    char     *color;
    char     *font;
    char     *monitor_id;
    double    rel_x, rel_y;
    int       width, height;
    GList    *images;
    gboolean  md_enabled;
} NoteData;

typedef struct {
    GtkWindow      *window;
    GtkTextView    *text_view;
    GtkWidget      *header;
    NoteData       *data;
    GtkApplication *app;
    GtkCssProvider *css_provider;
    char           *css_class;
    guint           format_timeout;
    double          pointer_x;
    double          pointer_y;
} NoteWindow;

NoteWindow *note_window_new(GtkApplication *app, NoteData *data);

NoteData   *note_data_new(const char *color);
void        note_data_free(NoteData *data);

NoteImage  *note_image_new(const char *filename, int offset, int w, int h);
void        note_image_free(NoteImage *img);

#define MAX_IMAGE_WIDTH 1200
#define MIN_IMAGE_SIZE  32
