// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "format.h"
#include "store.h"
#include "scaled_paintable.h"
#include <string.h>

static GRegex *re_url       = NULL;
static GRegex *re_bold_ital = NULL;
static GRegex *re_bold      = NULL;
static GRegex *re_italic    = NULL;
static GRegex *re_strike    = NULL;
static GRegex *re_code      = NULL;
static GRegex *re_heading   = NULL;
static GRegex *re_list      = NULL;
static GRegex *re_md_img    = NULL;

static void ensure_regexes(void) {
    if (re_url) return;
    re_url       = g_regex_new("(https?://(?:[^\\s/<>\"']*\\.)+[a-zA-Z]{2,}(?::[0-9]+)?(?:/[a-zA-Z0-9._~:/?#@!$&()*+,;=%\\[\\]\\-]*)?|file:///[^\\s<>\"']+)", 0, 0, NULL);
    re_bold_ital = g_regex_new("\\*\\*\\*(.+?)\\*\\*\\*", 0, 0, NULL);
    re_bold      = g_regex_new("(?<!\\*)\\*\\*(?!\\*)(.+?)(?<!\\*)\\*\\*(?!\\*)", 0, 0, NULL);
    re_italic    = g_regex_new("(?<!\\*)\\*(?!\\*|\\s)(.+?)(?<!\\*|\\s)\\*(?!\\*)", 0, 0, NULL);
    re_strike    = g_regex_new("~~(.+?)~~", 0, 0, NULL);
    re_code      = g_regex_new("`([^`]+)`", 0, 0, NULL);
    re_heading   = g_regex_new("^(#{1,3})\\s+(.+)$", G_REGEX_MULTILINE, 0, NULL);
    re_list      = g_regex_new("^(\\s*(?:[-+]|\\d+\\.))(\\s+.+)$", G_REGEX_MULTILINE, 0, NULL);
    re_md_img    = g_regex_new("!\\[([^\\]]*)\\]\\(([^)]+)\\)", 0, 0, NULL);
}

void format_update_url_tags(NoteWindow *nw) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);

    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "url");
    if (!tag) {
        tag = gtk_text_buffer_create_tag(buf, "url",
            "underline", PANGO_UNDERLINE_SINGLE,
            "foreground", "#1a73e8",
            NULL);
    }
    gtk_text_buffer_remove_tag(buf, tag, &start, &end);

    char *text = gtk_text_buffer_get_text(buf, &start, &end, TRUE);
    if (!strstr(text, "://")) { g_free(text); return; }
    ensure_regexes();
    GMatchInfo *match_info;

    g_regex_match(re_url, text, 0, &match_info);
    while (g_match_info_matches(match_info)) {
        int s, e;
        g_match_info_fetch_pos(match_info, 0, &s, &e);

        int char_start = g_utf8_pointer_to_offset(text, text + s);
        int char_end = g_utf8_pointer_to_offset(text, text + e);

        GtkTextIter url_start, url_end;
        gtk_text_buffer_get_iter_at_offset(buf, &url_start, char_start);
        gtk_text_buffer_get_iter_at_offset(buf, &url_end, char_end);
        gtk_text_buffer_apply_tag(buf, tag, &url_start, &url_end);

        g_match_info_next(match_info, NULL);
    }

    g_match_info_free(match_info);
    g_free(text);
}

char *format_get_url_at_iter(GtkTextBuffer *buf, GtkTextIter *iter) {
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "url");
    if (!tag || !gtk_text_iter_has_tag(iter, tag))
        return NULL;

    GtkTextIter start = *iter, end = *iter;
    if (!gtk_text_iter_starts_tag(&start, tag))
        gtk_text_iter_backward_to_tag_toggle(&start, tag);
    if (!gtk_text_iter_ends_tag(&end, tag))
        gtk_text_iter_forward_to_tag_toggle(&end, tag);

    return gtk_text_buffer_get_text(buf, &start, &end, TRUE);
}

void format_url_motion_cb(GtkEventControllerMotion *ctrl,
                          double x, double y, gpointer user_data) {
    (void)ctrl;
    NoteWindow *nw = user_data;
    GtkTextView *tv = nw->text_view;

    int bx, by;
    gtk_text_view_window_to_buffer_coords(tv, GTK_TEXT_WINDOW_WIDGET,
                                          (int)x, (int)y, &bx, &by);
    GtkTextIter iter;
    gtk_text_view_get_iter_at_location(tv, &iter, bx, by);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(tv);
    char *url = format_get_url_at_iter(buf, &iter);

    if (url) {
        gtk_widget_set_cursor_from_name(GTK_WIDGET(tv), "pointer");
        g_free(url);
    } else {
        gtk_widget_set_cursor_from_name(GTK_WIDGET(tv), "text");
    }
}

void format_url_click_cb(GtkGestureClick *gesture, int n_press,
                         double x, double y, gpointer user_data) {
    (void)n_press;
    GdkModifierType state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(gesture));
    if (!(state & GDK_CONTROL_MASK)) return;

    NoteWindow *nw = user_data;
    GtkTextView *tv = nw->text_view;

    int bx, by;
    gtk_text_view_window_to_buffer_coords(tv, GTK_TEXT_WINDOW_WIDGET,
                                          (int)x, (int)y, &bx, &by);
    GtkTextIter iter;
    gtk_text_view_get_iter_at_location(tv, &iter, bx, by);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(tv);
    char *url = format_get_url_at_iter(buf, &iter);

    if (url) {
        g_app_info_launch_default_for_uri(url, NULL, NULL);
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        g_free(url);
    }
}

static const char *md_tag_names[] = {
    "md-bold", "md-italic", "md-bold-italic",
    "md-strike", "md-code", "md-h1", "md-h2",
    "md-h3", "md-list", "md-syntax"
};
#define NUM_MD_TAGS 10

void format_ensure_md_tags(GtkTextBuffer *buf) {
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
    if (gtk_text_tag_table_lookup(table, "md-bold")) return;

    gtk_text_buffer_create_tag(buf, "md-bold",
        "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(buf, "md-italic",
        "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_create_tag(buf, "md-bold-italic",
        "weight", PANGO_WEIGHT_BOLD,
        "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_create_tag(buf, "md-strike",
        "strikethrough", TRUE, NULL);
    gtk_text_buffer_create_tag(buf, "md-code",
        "family", "Monospace", NULL);
    gtk_text_buffer_create_tag(buf, "md-h1",
        "weight", PANGO_WEIGHT_BOLD,
        "scale", 1.6, NULL);
    gtk_text_buffer_create_tag(buf, "md-h2",
        "weight", PANGO_WEIGHT_BOLD,
        "scale", 1.4, NULL);
    gtk_text_buffer_create_tag(buf, "md-h3",
        "weight", PANGO_WEIGHT_BOLD,
        "scale", 1.2, NULL);
    gtk_text_buffer_create_tag(buf, "md-list",
        "left-margin", 24, NULL);
    gtk_text_buffer_create_tag(buf, "md-syntax",
        "foreground", "#999999", NULL);
}

void format_remove_all_md_tags(GtkTextBuffer *buf) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    format_ensure_md_tags(buf);
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
    for (int i = 0; i < NUM_MD_TAGS; i++) {
        GtkTextTag *t = gtk_text_tag_table_lookup(table, md_tag_names[i]);
        if (t) gtk_text_buffer_remove_tag(buf, t, &start, &end);
    }
}

static void apply_md_regex(GtkTextBuffer *buf, const char *text,
                           GRegex *regex, const char *tag_name,
                           int prefix_len, int suffix_len) {
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
    GtkTextTag *content_tag = gtk_text_tag_table_lookup(table, tag_name);
    GtkTextTag *syntax_tag = gtk_text_tag_table_lookup(table, "md-syntax");
    GMatchInfo *match_info;

    g_regex_match(regex, text, 0, &match_info);
    while (g_match_info_matches(match_info)) {
        int s, e;
        g_match_info_fetch_pos(match_info, 0, &s, &e);

        int cs = g_utf8_pointer_to_offset(text, text + s);
        int ce = g_utf8_pointer_to_offset(text, text + e);

        GtkTextIter i1, i2;

        /* Syntax prefix */
        gtk_text_buffer_get_iter_at_offset(buf, &i1, cs);
        gtk_text_buffer_get_iter_at_offset(buf, &i2, cs + prefix_len);
        gtk_text_buffer_apply_tag(buf, syntax_tag, &i1, &i2);

        /* Content */
        gtk_text_buffer_get_iter_at_offset(buf, &i1, cs + prefix_len);
        gtk_text_buffer_get_iter_at_offset(buf, &i2, ce - suffix_len);
        gtk_text_buffer_apply_tag(buf, content_tag, &i1, &i2);

        /* Syntax suffix */
        gtk_text_buffer_get_iter_at_offset(buf, &i1, ce - suffix_len);
        gtk_text_buffer_get_iter_at_offset(buf, &i2, ce);
        gtk_text_buffer_apply_tag(buf, syntax_tag, &i1, &i2);

        g_match_info_next(match_info, NULL);
    }
    g_match_info_free(match_info);
}

void format_update_md_tags(NoteWindow *nw) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);

    format_ensure_md_tags(buf);
    format_remove_all_md_tags(buf);

    char *text = gtk_text_buffer_get_text(buf, &start, &end, TRUE);

    gboolean has_md = strstr(text, "*") || strstr(text, "~") ||
                      strstr(text, "`") || strstr(text, "#") ||
                      strstr(text, "- ") || strstr(text, "+ ") ||
                      strstr(text, "![");
    if (!has_md) { g_free(text); return; }

    ensure_regexes();
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);

    /* Bold+Italic: ***text*** */
    apply_md_regex(buf, text, re_bold_ital, "md-bold-italic", 3, 3);

    /* Bold: **text** */
    apply_md_regex(buf, text, re_bold, "md-bold", 2, 2);

    /* Italic: *text* */
    apply_md_regex(buf, text, re_italic, "md-italic", 1, 1);

    /* Strikethrough: ~~text~~ */
    apply_md_regex(buf, text, re_strike, "md-strike", 2, 2);

    /* Inline code: `text` */
    apply_md_regex(buf, text, re_code, "md-code", 1, 1);

    /* Headings: # text, ## text, ### text */
    {
        GMatchInfo *mi;
        GtkTextTag *syntax_tag = gtk_text_tag_table_lookup(table, "md-syntax");

        g_regex_match(re_heading, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            int hs, he, cs, ce;
            g_match_info_fetch_pos(mi, 1, &hs, &he);
            g_match_info_fetch_pos(mi, 2, &cs, &ce);

            int hash_len = he - hs;
            const char *tn = hash_len == 1 ? "md-h1" :
                             hash_len == 2 ? "md-h2" : "md-h3";
            GtkTextTag *htag = gtk_text_tag_table_lookup(table, tn);

            int char_hs = g_utf8_pointer_to_offset(text, text + hs);
            int char_cs = g_utf8_pointer_to_offset(text, text + cs);
            int char_ce = g_utf8_pointer_to_offset(text, text + ce);

            GtkTextIter i1, i2;
            gtk_text_buffer_get_iter_at_offset(buf, &i1, char_hs);
            gtk_text_buffer_get_iter_at_offset(buf, &i2, char_cs);
            gtk_text_buffer_apply_tag(buf, htag, &i1, &i2);
            gtk_text_buffer_apply_tag(buf, syntax_tag, &i1, &i2);

            gtk_text_buffer_get_iter_at_offset(buf, &i1, char_cs);
            gtk_text_buffer_get_iter_at_offset(buf, &i2, char_ce);
            gtk_text_buffer_apply_tag(buf, htag, &i1, &i2);

            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }

    /* Lists: - item, + item, 1. item */
    {
        GMatchInfo *mi;
        GtkTextTag *list_tag = gtk_text_tag_table_lookup(table, "md-list");
        GtkTextTag *syntax_tag = gtk_text_tag_table_lookup(table, "md-syntax");

        g_regex_match(re_list, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            int s, e, bs, be;
            g_match_info_fetch_pos(mi, 0, &s, &e);
            g_match_info_fetch_pos(mi, 1, &bs, &be);

            int cs = g_utf8_pointer_to_offset(text, text + s);
            int ce = g_utf8_pointer_to_offset(text, text + e);
            int cbs = g_utf8_pointer_to_offset(text, text + bs);
            int cbe = g_utf8_pointer_to_offset(text, text + be);

            GtkTextIter i1, i2;
            gtk_text_buffer_get_iter_at_offset(buf, &i1, cs);
            gtk_text_buffer_get_iter_at_offset(buf, &i2, ce);
            gtk_text_buffer_apply_tag(buf, list_tag, &i1, &i2);

            gtk_text_buffer_get_iter_at_offset(buf, &i1, cbs);
            gtk_text_buffer_get_iter_at_offset(buf, &i2, cbe);
            gtk_text_buffer_apply_tag(buf, syntax_tag, &i1, &i2);

            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }

    /* Markdown images: dim ![alt](path) */
    {
        GMatchInfo *mi;
        GtkTextTag *syntax_tag = gtk_text_tag_table_lookup(table, "md-syntax");

        g_regex_match(re_md_img, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            int s, e;
            g_match_info_fetch_pos(mi, 0, &s, &e);
            int cs = g_utf8_pointer_to_offset(text, text + s);
            int ce = g_utf8_pointer_to_offset(text, text + e);

            GtkTextIter i1, i2;
            gtk_text_buffer_get_iter_at_offset(buf, &i1, cs);
            gtk_text_buffer_get_iter_at_offset(buf, &i2, ce);
            gtk_text_buffer_apply_tag(buf, syntax_tag, &i1, &i2);

            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }

    g_free(text);
}

void format_remove_md_images(NoteWindow *nw) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buf, &iter);

    /* Collect offsets of md image paintables (prepend = reverse order) */
    GList *offsets = NULL;
    while (!gtk_text_iter_is_end(&iter)) {
        GdkPaintable *p = gtk_text_iter_get_paintable(&iter);
        if (p && SCALED_IS_PAINTABLE(p) &&
            scaled_paintable_is_md_image(SCALED_PAINTABLE(p))) {
            offsets = g_list_prepend(offsets,
                GINT_TO_POINTER(gtk_text_iter_get_offset(&iter)));
        }
        gtk_text_iter_forward_char(&iter);
    }

    /* Delete in reverse order to keep offsets stable */
    for (GList *l = offsets; l; l = l->next) {
        int offset = GPOINTER_TO_INT(l->data);
        GtkTextIter s, e;
        gtk_text_buffer_get_iter_at_offset(buf, &s, offset);
        e = s;
        gtk_text_iter_forward_char(&e);
        gtk_text_buffer_delete(buf, &s, &e);
    }
    g_list_free(offsets);
}

static char *resolve_image_path(const char *path) {
    if (g_path_is_absolute(path))
        return g_strdup(path);
    if (path[0] == '~' && path[1] == '/') {
        return g_build_filename(g_get_home_dir(), path + 2, NULL);
    }
    char *img_dir = store_get_images_dir();
    char *full = g_build_filename(img_dir, path, NULL);
    g_free(img_dir);
    return full;
}

typedef struct { int char_end; char *path; } MdImgMatch;

void format_insert_md_images(NoteWindow *nw) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    char *text = gtk_text_buffer_get_text(buf, &start, &end, TRUE);

    if (!strstr(text, "![")) { g_free(text); return; }
    ensure_regexes();
    GMatchInfo *mi;

    /* Collect matches (prepend = reverse order for stable insertion) */
    GList *matches = NULL;
    g_regex_match(re_md_img, text, 0, &mi);
    while (g_match_info_matches(mi)) {
        int e, ps, pe;
        g_match_info_fetch_pos(mi, 0, NULL, &e);
        g_match_info_fetch_pos(mi, 2, &ps, &pe);

        MdImgMatch *m = g_new0(MdImgMatch, 1);
        m->char_end = g_utf8_pointer_to_offset(text, text + e);
        m->path = g_strndup(text + ps, pe - ps);
        matches = g_list_prepend(matches, m);

        g_match_info_next(mi, NULL);
    }
    g_match_info_free(mi);

    /* Insert images in reverse order */
    for (GList *l = matches; l; l = l->next) {
        MdImgMatch *m = l->data;
        char *full_path = resolve_image_path(m->path);

        if (full_path && g_file_test(full_path, G_FILE_TEST_EXISTS)) {
            GdkTexture *texture = gdk_texture_new_from_filename(full_path, NULL);
            if (texture) {
                int orig_w = gdk_texture_get_width(texture);
                int orig_h = gdk_texture_get_height(texture);
                if (orig_w <= 0 || orig_h <= 0) {
                    g_object_unref(texture);
                    goto next_match;
                }
                int disp_w = orig_w, disp_h = orig_h;
                int max_w = MAX_IMAGE_WIDTH;
                int note_w = gtk_widget_get_width(GTK_WIDGET(nw->text_view)) - 24;
                if (note_w > 0 && note_w < max_w) max_w = note_w;
                if (disp_w > max_w) {
                    disp_h = disp_h * max_w / disp_w;
                    disp_w = max_w;
                }

                ScaledPaintable *sp = scaled_paintable_new_md(
                    texture, m->path, disp_w, disp_h);

                GtkTextIter iter;
                gtk_text_buffer_get_iter_at_offset(buf, &iter, m->char_end);
                gtk_text_buffer_insert(buf, &iter, "\n", 1);
                gtk_text_buffer_get_iter_at_offset(buf, &iter, m->char_end + 1);
                gtk_text_buffer_insert_paintable(buf, &iter, GDK_PAINTABLE(sp));

                g_object_unref(sp);
                g_object_unref(texture);
            }
        }
    next_match:
        g_free(full_path);
        g_free(m->path);
        g_free(m);
    }
    g_list_free(matches);
    g_free(text);
}
