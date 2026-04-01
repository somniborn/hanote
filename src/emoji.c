// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "emoji.h"
#include <string.h>

typedef struct {
    char *emoji;
    char *name;
} EmojiEntry;

static GArray *emoji_db = NULL;

/* 0 = faces/people (first), 1 = other emoji, 2 = symbols/non-emoji (last) */
static int emoji_priority(const char *s) {
    gunichar ch = g_utf8_get_char(s);
    /* Skin tone modifiers → back */
    if (ch >= 0x1F3FB && ch <= 0x1F3FF) return 2;
    /* Emoticons / faces: U+1F600..1F64F */
    if (ch >= 0x1F600 && ch <= 0x1F64F) return 0;
    /* Supplemental faces: U+1F900..1F9FF (🤣🥰🤗 etc.) */
    if (ch >= 0x1F900 && ch <= 0x1F9FF) return 0;
    /* Extended faces: U+1FA70..1FAFF (🥲🫠🫡 etc.) */
    if (ch >= 0x1FA70 && ch <= 0x1FAFF) return 0;
    /* People / body: U+1F466..1F487 */
    if (ch >= 0x1F466 && ch <= 0x1F487) return 0;
    /* Hand signs: U+1F44x..1F45x, 👋👌✊ etc. */
    if (ch >= 0x1F440 && ch <= 0x1F45F) return 0;
    /* Hearts: U+2764, U+1F49x */
    if (ch >= 0x1F490 && ch <= 0x1F49F) return 0;
    if (ch == 0x2764 || ch == 0x2763) return 0;
    /* Other emoji ≥ U+1F000 */
    if (ch >= 0x1F000) return 1;
    /* BMP emoji-like symbols */
    if (ch == 0x00A9 || ch == 0x00AE) return 1;
    if (ch >= 0x2600 && ch <= 0x27BF) return 1;
    if (ch >= 0x2300 && ch <= 0x23FF) return 1;
    if (ch >= 0x2B50 && ch <= 0x2B55) return 1;
    if (ch == 0x200D || ch == 0xFE0F) return 1;
    if (ch >= 0x3030 && ch <= 0x303D) return 1;
    if (ch == 0x2049 || ch == 0x203C) return 1;
    /* Everything else → back */
    return 2;
}

void emoji_load_db(void) {
    if (emoji_db) return;
    GArray *groups[3];
    for (int g = 0; g < 3; g++)
        groups[g] = g_array_new(FALSE, FALSE, sizeof(EmojiEntry));

    /* System file first (gets updated), then installed, then source tree */
    char *source_path = g_build_filename(SOURCE_DATA_DIR, "emojis.dic", NULL);
    char *install_path = g_build_filename(INSTALL_DATA_DIR, "emojis.dic", NULL);
    const char *candidates[] = {
        "/usr/share/speech-dispatcher/locale/en/emojis.dic",
        "/usr/local/share/speech-dispatcher/locale/en/emojis.dic",
        install_path,
        source_path,
    };

    char *contents = NULL;
    for (int i = 0; i < (int)G_N_ELEMENTS(candidates); i++) {
        if (g_file_get_contents(candidates[i], &contents, NULL, NULL))
            break;
    }
    g_free(source_path);
    g_free(install_path);
    if (!contents) return;

    char **lines = g_strsplit(contents, "\n", -1);
    g_free(contents);

    for (int i = 0; lines[i]; i++) {
        if (lines[i][0] == '#' || lines[i][0] == '\0') continue;
        if (g_str_has_prefix(lines[i], "symbols:")) continue;

        char **parts = g_strsplit(lines[i], "\t", 3);
        if (parts[0] && parts[1]) {
            EmojiEntry e;
            e.emoji = g_strdup(parts[0]);
            e.name = g_strdup(parts[1]);
            int p = emoji_priority(parts[0]);
            g_array_append_val(groups[p], e);
        }
        g_strfreev(parts);
    }
    g_strfreev(lines);

    /* Merge: faces → other emoji → symbols */
    emoji_db = groups[0];
    for (int g = 1; g < 3; g++) {
        for (guint i = 0; i < groups[g]->len; i++) {
            EmojiEntry e = g_array_index(groups[g], EmojiEntry, i);
            g_array_append_val(emoji_db, e);
        }
        g_array_free(groups[g], TRUE);
    }
}

#define EMOJI_PAGE_SIZE 200

typedef struct {
    GtkFlowBox *flow;
    NoteWindow *nw;
    guint       loaded;
    char       *search;
    GArray     *matched;
} EmojiFilterData;

static void emoji_filter_data_free(EmojiFilterData *fd) {
    if (fd->matched) g_array_free(fd->matched, TRUE);
    g_free(fd->search);
    g_free(fd);
}

static void build_match_list(EmojiFilterData *fd) {
    if (fd->matched) g_array_free(fd->matched, TRUE);
    fd->matched = g_array_new(FALSE, FALSE, sizeof(guint));
    if (!emoji_db) return;

    for (guint i = 0; i < emoji_db->len; i++) {
        EmojiEntry *e = &g_array_index(emoji_db, EmojiEntry, i);
        if (fd->search && fd->search[0]) {
            char *name_lower = g_utf8_strdown(e->name, -1);
            gboolean match = strstr(name_lower, fd->search) != NULL;
            g_free(name_lower);
            if (!match) continue;
        }
        g_array_append_val(fd->matched, i);
    }
}

static void on_emoji_grid_btn(GtkButton *btn, gpointer user_data) {
    NoteWindow *nw = user_data;
    const char *emoji = gtk_button_get_label(btn);
    if (emoji && emoji[0]) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
        gtk_text_buffer_insert_at_cursor(buf, emoji, -1);
    }
}

static void append_emoji_page(EmojiFilterData *fd) {
    if (!fd->matched) return;
    guint end = fd->loaded + EMOJI_PAGE_SIZE;
    if (end > fd->matched->len) end = fd->matched->len;

    for (guint i = fd->loaded; i < end; i++) {
        guint idx = g_array_index(fd->matched, guint, i);
        EmojiEntry *e = &g_array_index(emoji_db, EmojiEntry, idx);
        GtkWidget *btn = gtk_button_new_with_label(e->emoji);
        gtk_widget_set_tooltip_text(btn, e->name);
        gtk_widget_set_size_request(btn, 36, 36);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_emoji_grid_btn), fd->nw);
        gtk_flow_box_insert(fd->flow, btn, -1);
    }
    fd->loaded = end;
}

static void rebuild_emoji_grid(EmojiFilterData *fd) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(fd->flow))) != NULL)
        gtk_flow_box_remove(fd->flow, child);
    fd->loaded = 0;
    build_match_list(fd);
    append_emoji_page(fd);
}

static void on_emoji_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    EmojiFilterData *fd = user_data;
    g_free(fd->search);
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    fd->search = (text && text[0]) ? g_utf8_strdown(text, -1) : NULL;
    rebuild_emoji_grid(fd);
}

static void on_emoji_scroll_edge(GtkScrolledWindow *sw, GtkPositionType pos,
                                 gpointer user_data) {
    (void)sw;
    if (pos != GTK_POS_BOTTOM) return;
    EmojiFilterData *fd = user_data;
    if (fd->loaded < fd->matched->len)
        append_emoji_page(fd);
}

static void on_dialog_destroy_remove_css(GtkWidget *win, GtkCssProvider *prov) {
    (void)win;
    gtk_style_context_remove_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(prov));
}

void emoji_show_picker(GtkButton *btn, NoteWindow *nw) {
    (void)btn;
    emoji_load_db();

    GtkWidget *win = gtk_application_window_new(nw->app);
    gtk_window_set_title(GTK_WINDOW(win), "Emoji");
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 360);
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(win), nw->window);

    GtkCssProvider *ep = gtk_css_provider_new();
    gtk_css_provider_load_from_string(ep,
        "window.emoji-dialog { background-color: #d0d0d0; color: #000000; }"
        "window.emoji-dialog headerbar { background-color: #c0c0c0; color: #000000; }"
        "window.emoji-dialog entry,"
        "window.emoji-dialog entry > text {"
        "  background-color: #e8e8e8; color: #000000; caret-color: #000000;"
        "}"
        "window.emoji-dialog scrolledwindow,"
        "window.emoji-dialog flowbox,"
        "window.emoji-dialog flowboxchild {"
        "  background-color: #d0d0d0;"
        "}"
        "window.emoji-dialog flowboxchild > button {"
        "  min-width: 36px; min-height: 36px; font-size: 20px;"
        "  padding: 2px; background: none; border: none;"
        "  box-shadow: none; outline: none;"
        "}"
        "window.emoji-dialog flowboxchild > button:hover {"
        "  background-color: #b8b8b8;"
        "}"
        "window.emoji-dialog entry image {"
        "  color: #000000;"
        "  -gtk-icon-style: symbolic;"
        "}"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(ep),
        GTK_STYLE_PROVIDER_PRIORITY_USER + 2);
    gtk_widget_add_css_class(win, "emoji-dialog");
    g_object_set_data_full(G_OBJECT(win), "css-provider",
        ep, (GDestroyNotify)g_object_unref);
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy_remove_css), ep);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(win), vbox);

    GtkWidget *search = gtk_search_entry_new();
    gtk_widget_set_margin_start(search, 8);
    gtk_widget_set_margin_end(search, 8);
    gtk_widget_set_margin_top(search, 8);
    gtk_widget_set_margin_bottom(search, 4);
    gtk_box_append(GTK_BOX(vbox), search);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 9);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_widget_set_margin_start(flow, 4);
    gtk_widget_set_margin_end(flow, 4);
    gtk_widget_set_margin_top(flow, 4);
    gtk_widget_set_margin_bottom(flow, 4);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), flow);
    gtk_box_append(GTK_BOX(vbox), scroll);

    /* Filter data — freed on window destroy */
    EmojiFilterData *fd = g_new0(EmojiFilterData, 1);
    fd->flow = GTK_FLOW_BOX(flow);
    fd->nw = nw;
    fd->search = NULL;
    fd->matched = NULL;
    fd->loaded = 0;

    rebuild_emoji_grid(fd);

    g_signal_connect(search, "search-changed",
                     G_CALLBACK(on_emoji_search_changed), fd);
    g_signal_connect(scroll, "edge-reached",
                     G_CALLBACK(on_emoji_scroll_edge), fd);
    g_signal_connect_swapped(win, "destroy", G_CALLBACK(emoji_filter_data_free), fd);

    gtk_window_present(GTK_WINDOW(win));
    gtk_widget_grab_focus(search);
}
