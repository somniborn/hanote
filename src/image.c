// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "image.h"
#include "store.h"
#include "scaled_paintable.h"
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

static char *save_texture_to_file(GdkTexture *texture) {
    char *img_dir = store_get_images_dir();
    char *uuid = g_uuid_string_random();
    char *filename = g_strdup_printf("%s.png", uuid);
    char *full_path = g_build_filename(img_dir, filename, NULL);

    gdk_texture_save_to_png(texture, full_path);

    g_free(full_path);
    g_free(uuid);
    g_free(img_dir);
    return filename;
}

static void insert_image_at_cursor(NoteWindow *nw, GdkTexture *texture,
                                   const char *filename) {
    int orig_w = gdk_texture_get_width(texture);
    int orig_h = gdk_texture_get_height(texture);

    int disp_w = orig_w;
    int disp_h = orig_h;
    int note_w = gtk_widget_get_width(GTK_WIDGET(nw->text_view)) - 24;
    int max_w = note_w > 0 ? (note_w < MAX_IMAGE_WIDTH ? note_w : MAX_IMAGE_WIDTH)
                           : MAX_IMAGE_WIDTH;
    if (disp_w > max_w) {
        disp_h = disp_h * max_w / disp_w;
        disp_w = max_w;
    }

    ScaledPaintable *sp = scaled_paintable_new(texture, filename, disp_w, disp_h);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buf, &iter,
        gtk_text_buffer_get_insert(buf));
    gtk_text_buffer_insert_paintable(buf, &iter, GDK_PAINTABLE(sp));
    g_object_unref(sp);
}

static void drop_single_file(NoteWindow *nw, GFile *file);

static void on_paste_texture_ready(GObject *source, GAsyncResult *res,
                                   gpointer user_data) {
    NoteWindow *nw = user_data;
    GdkClipboard *clip = GDK_CLIPBOARD(source);
    GdkTexture *texture = gdk_clipboard_read_texture_finish(clip, res, NULL);

    if (texture) {
        char *filename = save_texture_to_file(texture);
        insert_image_at_cursor(nw, texture, filename);
        g_free(filename);
        g_object_unref(texture);
    }
}

static void on_paste_file_value_ready(GObject *source, GAsyncResult *res,
                                      gpointer user_data) {
    NoteWindow *nw = user_data;
    GdkClipboard *clip = GDK_CLIPBOARD(source);
    const GValue *val = gdk_clipboard_read_value_finish(clip, res, NULL);
    if (!val) return;

    if (G_VALUE_HOLDS(val, GDK_TYPE_FILE_LIST)) {
        GSList *files = g_value_get_boxed(val);
        for (GSList *l = files; l; l = l->next)
            drop_single_file(nw, G_FILE(l->data));
    }
}

gboolean image_paste_key_cb(GtkEventControllerKey *ctrl, guint keyval,
                            guint keycode, GdkModifierType state,
                            gpointer user_data) {
    (void)ctrl; (void)keycode;
    if (keyval != GDK_KEY_v || !(state & GDK_CONTROL_MASK))
        return FALSE;

    NoteWindow *nw = user_data;
    GdkClipboard *clip = gtk_widget_get_clipboard(GTK_WIDGET(nw->text_view));
    GdkContentFormats *formats = gdk_clipboard_get_formats(clip);

    /* Direct image (e.g. screenshot) */
    if (gdk_content_formats_contain_gtype(formats, GDK_TYPE_TEXTURE)) {
        gdk_clipboard_read_texture_async(clip, NULL,
            on_paste_texture_ready, nw);
        return TRUE;
    }
    /* File list (e.g. Ctrl+C on image file in file manager) */
    if (gdk_content_formats_contain_gtype(formats, GDK_TYPE_FILE_LIST)) {
        gdk_clipboard_read_value_async(clip, GDK_TYPE_FILE_LIST, 0, NULL,
            on_paste_file_value_ready, nw);
        return TRUE;
    }
    return FALSE;  /* text — let default paste handle it */
}

static void activate_after_drop(NoteWindow *nw) {
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(nw->window));
    if (surface && GDK_IS_TOPLEVEL(surface))
        gdk_toplevel_focus(GDK_TOPLEVEL(surface), GDK_CURRENT_TIME);
}

static void drop_single_file(NoteWindow *nw, GFile *file) {
    char *basename = g_file_get_basename(file);
    char *content_type = g_content_type_guess(basename, NULL, 0, NULL);
    gboolean is_image = g_content_type_is_a(content_type, "image/*");
    g_free(content_type);

    if (is_image) {
        char *img_dir = store_get_images_dir();
        char *uuid = g_uuid_string_random();
        const char *dot = strrchr(basename, '.');
        char *ext = (dot && dot != basename) ? g_strdup(dot) : g_strdup(".png");
        char *filename = g_strdup_printf("%s%s", uuid, ext);
        char *dest_path = g_build_filename(img_dir, filename, NULL);

        GFile *dest = g_file_new_for_path(dest_path);
        g_file_copy(file, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);

        GdkTexture *texture = gdk_texture_new_from_file(dest, NULL);
        g_object_unref(dest);

        if (texture) {
            insert_image_at_cursor(nw, texture, filename);
            g_object_unref(texture);
        }

        g_free(dest_path);
        g_free(filename);
        g_free(ext);
        g_free(uuid);
        g_free(img_dir);
    } else {
        char *uri = g_file_get_uri(file);
        GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(buf, &iter,
            gtk_text_buffer_get_insert(buf));
        if (!gtk_text_iter_is_start(&iter)) {
            GtkTextIter prev = iter;
            gtk_text_iter_backward_char(&prev);
            gunichar ch = gtk_text_iter_get_char(&prev);
            if (ch == '\n')
                ;
            else if (!g_unichar_isspace(ch))
                gtk_text_buffer_insert(buf, &iter, " ", 1);
        }
        /* Remember position, insert URI, then hide "file://" prefix */
        int offset = gtk_text_iter_get_offset(&iter);
        gtk_text_buffer_insert(buf, &iter, uri, -1);

        GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
        GtkTextTag *fs_tag = gtk_text_tag_table_lookup(table, "file-scheme");
        if (!fs_tag) {
            fs_tag = gtk_text_buffer_create_tag(buf, "file-scheme",
                "invisible", TRUE, NULL);
        }
        GtkTextIter scheme_start, scheme_end;
        gtk_text_buffer_get_iter_at_offset(buf, &scheme_start, offset);
        gtk_text_buffer_get_iter_at_offset(buf, &scheme_end, offset + 7);
        gtk_text_buffer_apply_tag(buf, fs_tag, &scheme_start, &scheme_end);

        g_free(uri);
    }
    g_free(basename);
}

gboolean image_drop_cb(GtkDropTarget *target, const GValue *value,
                       double x, double y, gpointer user_data) {
    (void)target; (void)x; (void)y;
    NoteWindow *nw = user_data;

    if (G_VALUE_HOLDS(value, GDK_TYPE_TEXTURE)) {
        GdkTexture *texture = g_value_get_object(value);
        char *filename = save_texture_to_file(texture);
        insert_image_at_cursor(nw, texture, filename);
        g_free(filename);
        activate_after_drop(nw);
        return TRUE;
    } else if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
        GSList *files = g_value_get_boxed(value);
        for (GSList *l = files; l; l = l->next)
            drop_single_file(nw, G_FILE(l->data));
        activate_after_drop(nw);
        return TRUE;
    } else if (G_VALUE_HOLDS(value, G_TYPE_FILE)) {
        drop_single_file(nw, g_value_get_object(value));
        activate_after_drop(nw);
        return TRUE;
    }
    return FALSE;
}

void image_reload(NoteWindow *nw) {
    if (!nw->data->images) return;

    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    char *img_dir = store_get_images_dir();

    /* Sort by offset descending to keep offsets stable during insertion */
    GList *sorted = g_list_copy(nw->data->images);
    for (GList *a = sorted; a; a = a->next) {
        for (GList *b = a->next; b; b = b->next) {
            NoteImage *ia = a->data, *ib = b->data;
            if (ib->offset > ia->offset) {
                a->data = ib;
                b->data = ia;
            }
        }
    }

    for (GList *l = sorted; l; l = l->next) {
        NoteImage *img = l->data;
        char *full_path = g_build_filename(img_dir, img->filename, NULL);

        if (g_file_test(full_path, G_FILE_TEST_EXISTS)) {
            GdkTexture *texture = gdk_texture_new_from_filename(full_path, NULL);
            if (texture) {
                ScaledPaintable *sp = scaled_paintable_new(
                    texture, img->filename, img->width, img->height);

                GtkTextIter iter;
                gtk_text_buffer_get_iter_at_offset(buf, &iter, img->offset);

                /* Delete the U+FFFC placeholder */
                GtkTextIter next = iter;
                gtk_text_iter_forward_char(&next);
                gtk_text_buffer_delete(buf, &iter, &next);

                /* Re-get iter at same offset after delete */
                gtk_text_buffer_get_iter_at_offset(buf, &iter, img->offset);
                gtk_text_buffer_insert_paintable(buf, &iter, GDK_PAINTABLE(sp));

                g_object_unref(sp);
                g_object_unref(texture);
            }
        }
        g_free(full_path);
    }

    g_list_free(sorted);
    g_free(img_dir);
}

void image_restore_file_scheme_tags(GtkTextBuffer *buf) {
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    char *text = gtk_text_buffer_get_text(buf, &s, &e, TRUE);
    const char *p = text;
    while ((p = strstr(p, "file://")) != NULL) {
        int char_off = g_utf8_pointer_to_offset(text, p);
        GtkTextTagTable *tbl = gtk_text_buffer_get_tag_table(buf);
        GtkTextTag *fs = gtk_text_tag_table_lookup(tbl, "file-scheme");
        if (!fs)
            fs = gtk_text_buffer_create_tag(buf, "file-scheme",
                "invisible", TRUE, NULL);
        GtkTextIter fs_start, fs_end;
        gtk_text_buffer_get_iter_at_offset(buf, &fs_start, char_off);
        gtk_text_buffer_get_iter_at_offset(buf, &fs_end, char_off + 7);
        gtk_text_buffer_apply_tag(buf, fs, &fs_start, &fs_end);
        p += 7;
    }
    g_free(text);
}
