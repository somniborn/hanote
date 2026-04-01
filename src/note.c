// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "note.h"
#include "config.h"
#include "store.h"
#include "scaled_paintable.h"
#include "emoji.h"
#include "format.h"
#include "image.h"
#include "monitor.h"
#include "compositor.h"
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

static const NoteColor NOTE_COLORS[] = {
    {"Yellow",  "#fff9b1"},
    {"Green",   "#c1f0c1"},
    {"Blue",    "#a8d8ea"},
    {"Pink",    "#f8c8dc"},
    {"Purple",  "#d5b8ff"},
    {"Orange",  "#ffd6a5"},
    {"White",   "#ffffff"},
};
#define NUM_NOTE_COLORS 7

/* Global list of all notes (managed by main.c) */
extern GList *all_notes;
extern void   app_save_notes(void);
extern void   app_create_note(GtkApplication *app, const char *color);
extern void   app_remove_window(NoteWindow *nw);
extern void   app_sync_positions(void);

static char *generate_id(void) {
    return g_strdup_printf("note-%ld-%d", (long)g_get_real_time(), g_random_int());
}

/* Parse "#rrggbb" and darken by factor (0.0–1.0) */
static char *darken_hex(const char *hex, double factor) {
    unsigned int r, g, b;
    sscanf(hex, "#%02x%02x%02x", &r, &g, &b);
    r = (unsigned int)(r * factor);
    g = (unsigned int)(g * factor);
    b = (unsigned int)(b * factor);
    return g_strdup_printf("#%02x%02x%02x", r, g, b);
}

NoteImage *note_image_new(const char *filename, int offset, int w, int h) {
    NoteImage *img = g_new0(NoteImage, 1);
    img->filename = g_strdup(filename);
    img->offset   = offset;
    img->width    = w;
    img->height   = h;
    return img;
}

void note_image_free(NoteImage *img) {
    if (!img) return;
    g_free(img->filename);
    g_free(img);
}

NoteData *note_data_new(const char *color) {
    NoteData *nd = g_new0(NoteData, 1);
    nd->id     = generate_id();
    nd->text   = g_strdup("");
    nd->color  = g_strdup(color);
    nd->font   = NULL;
    nd->monitor_id = NULL;
    nd->rel_x  = 0.3;
    nd->rel_y  = 0.3;
    nd->width  = config_get()->width;
    nd->height = config_get()->height;
    nd->images = NULL;
    nd->md_enabled = FALSE;
    return nd;
}

void note_data_free(NoteData *data) {
    if (!data) return;
    g_free(data->id);
    g_free(data->text);
    g_free(data->color);
    g_free(data->font);
    g_free(data->monitor_id);
    g_list_free_full(data->images, (GDestroyNotify)note_image_free);
    g_free(data);
}

static void apply_css(NoteWindow *nw) {
    char *header_color = darken_hex(nw->data->color, 0.90);
    const char *font = nw->data->font;

    char *font_css = g_strdup("");
    if (font && font[0]) {
        PangoFontDescription *pfd = pango_font_description_from_string(font);
        const char *family = pango_font_description_get_family(pfd);
        int size_pt = pango_font_description_get_size(pfd) / PANGO_SCALE;
        PangoStyle style = pango_font_description_get_style(pfd);
        PangoWeight weight = pango_font_description_get_weight(pfd);
        g_free(font_css);
        font_css = g_strdup_printf(
            "  font-family: \"%s\";"
            "  font-size: %dpt;"
            "  font-weight: %d;"
            "  font-style: %s;",
            family ? family : "Sans",
            size_pt > 0 ? size_pt : 16,
            (int)weight,
            style == PANGO_STYLE_ITALIC ? "italic" :
            style == PANGO_STYLE_OBLIQUE ? "oblique" : "normal"
        );
        pango_font_description_free(pfd);
    }

    char *css_str = g_strdup_printf(
        "window.%s.hanote {"
        "  background-color: %s;"
        "}"
        "window.%s.hanote .note-header {"
        "  background-color: %s;"
        "  padding: 4px 8px;"
        "  min-height: 32px;"
        "}"
        "window.%s.hanote .note-header button.icon-btn {"
        "  min-width: 28px;"
        "  min-height: 28px;"
        "  padding: 2px;"
        "  border: none;"
        "  background: none;"
        "  box-shadow: none;"
        "  border-radius: 4px;"
        "  color: #000000;"
        "}"
        "window.%s.hanote .note-header button.icon-btn image {"
        "  color: #000000;"
        "  -gtk-icon-size: 16px;"
        "  -gtk-icon-style: symbolic;"
        "}"
        "window.%s.hanote .note-header button.icon-btn:hover {"
        "  background-color: rgba(0,0,0,0.1);"
        "}"
        "window.%s.hanote .note-header button.icon-btn:checked {"
        "  background-color: rgba(0,0,0,0.2);"
        "  font-weight: bold;"
        "}"
        "window.%s.hanote textview {"
        "  background-color: %s;"
        "  color: #333333;"
        "  font-size: %dpx;"
        "  font-family: \"%s\";"
        "  %s"
        "}"
        "window.%s.hanote textview text {"
        "  background-color: %s;"
        "  color: #333333;"
        "}"
        "window.%s.hanote textview text selection {"
        "  background-color: #000000;"
        "  color: #ffffff;"
        "}"
        "window.%s.hanote popover.emoji-picker,"
        "window.%s.hanote popover.emoji-picker > contents {"
        "  background-color: #f0f0f0;"
        "  color: #000000;"
        "}"
        "window.%s.hanote popover.emoji-picker label,"
        "window.%s.hanote popover.emoji-picker button,"
        "window.%s.hanote popover.emoji-picker entry,"
        "window.%s.hanote popover.emoji-picker entry > text {"
        "  color: #000000;"
        "}",
        nw->css_class, nw->data->color,   /* window bg */
        nw->css_class, header_color,       /* header */
        nw->css_class,                     /* icon-btn */
        nw->css_class,                     /* icon-btn image */
        nw->css_class,                     /* icon-btn hover */
        nw->css_class,                     /* icon-btn checked */
        nw->css_class, nw->data->color, config_get()->font_size, config_get()->font, font_css, /* textview */
        nw->css_class, nw->data->color,   /* textview text */
        nw->css_class,                     /* selection */
        nw->css_class, nw->css_class,     /* emoji popover + contents */
        nw->css_class, nw->css_class, nw->css_class, nw->css_class /* emoji label,btn,entry,text */
    );

    if (nw->css_provider) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(nw->css_provider));
        g_object_unref(nw->css_provider);
    }

    nw->css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(nw->css_provider, css_str);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(nw->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );

    g_free(css_str);
    g_free(header_color);
    g_free(font_css);
}

static void on_text_changed(GtkTextBuffer *buf, NoteWindow *nw);

static void sync_text(NoteWindow *nw) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    g_free(nw->data->text);
    nw->data->text = gtk_text_buffer_get_text(buf, &start, &end, TRUE);

    /* Rebuild image list with current offsets and sizes */
    g_list_free_full(nw->data->images, (GDestroyNotify)note_image_free);
    nw->data->images = NULL;

    GtkTextIter iter = start;
    int offset = 0;
    while (!gtk_text_iter_is_end(&iter)) {
        GdkPaintable *p = gtk_text_iter_get_paintable(&iter);
        if (p && SCALED_IS_PAINTABLE(p)) {
            ScaledPaintable *sp = SCALED_PAINTABLE(p);
            NoteImage *img = note_image_new(
                scaled_paintable_get_filename(sp),
                offset,
                scaled_paintable_get_width(sp),
                scaled_paintable_get_height(sp)
            );
            nw->data->images = g_list_append(nw->data->images, img);
        }
        gtk_text_iter_forward_char(&iter);
        offset++;
    }
}

static gboolean do_format_update(gpointer user_data) {
    NoteWindow *nw = user_data;
    nw->format_timeout = 0;

    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    g_signal_handlers_block_by_func(buf, on_text_changed, nw);

    if (nw->data->md_enabled)
        format_remove_md_images(nw);
    sync_text(nw);
    format_update_url_tags(nw);
    if (nw->data->md_enabled) {
        format_update_md_tags(nw);
        format_insert_md_images(nw);
    }

    g_signal_handlers_unblock_by_func(buf, on_text_changed, nw);
    app_save_notes();

    return G_SOURCE_REMOVE;
}

static void on_text_changed(GtkTextBuffer *buf, NoteWindow *nw) {
    (void)buf;
    if (nw->format_timeout)
        g_source_remove(nw->format_timeout);
    nw->format_timeout = g_timeout_add(150, do_format_update, nw);
}

static void on_add_clicked(GtkButton *btn, NoteWindow *nw) {
    (void)btn;
    const char *new_hex = config_color_to_hex(config_get()->color);
    if (!new_hex) new_hex = "#fff9b1";
    app_create_note(nw->app, new_hex);
}

static void cleanup_note_window(NoteWindow *nw) {
    if (nw->format_timeout) {
        g_source_remove(nw->format_timeout);
        nw->format_timeout = 0;
    }

    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    g_signal_handlers_disconnect_by_data(buf, nw);
    g_signal_handlers_disconnect_by_data(nw->window, nw);

    if (nw->css_provider) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(nw->css_provider));
        g_object_unref(nw->css_provider);
        nw->css_provider = NULL;
    }

    app_remove_window(nw);
    g_free(nw->css_class);
    g_free(nw);
}

static void do_delete_note(NoteWindow *nw) {
    GtkApplication *app = nw->app;
    gboolean was_last = (g_list_length(all_notes) == 1);

    all_notes = g_list_remove(all_notes, nw->data);
    app_save_notes();

    char *img_dir = store_get_images_dir();
    for (GList *il = nw->data->images; il; il = il->next) {
        NoteImage *img = il->data;
        char *fpath = g_build_filename(img_dir, img->filename, NULL);
        g_unlink(fpath);
        g_free(fpath);
    }
    g_free(img_dir);

    note_data_free(nw->data);
    nw->data = NULL;

    GtkWindow *win = nw->window;
    cleanup_note_window(nw);
    gtk_window_destroy(win);

    if (was_last) {
        const char *hex = config_color_to_hex(config_get()->color);
        if (!hex) hex = "#fff9b1";
        app_create_note(app, hex);
    }
}

static void on_delete_confirm(GObject *source, GAsyncResult *res,
                              gpointer user_data) {
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG(source);
    int button = gtk_alert_dialog_choose_finish(dialog, res, NULL);
    if (button == 1) /* "Delete" */
        do_delete_note(user_data);
}

static void on_delete_clicked(GtkButton *btn, NoteWindow *nw) {
    (void)btn;
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Delete this note?");
    gtk_alert_dialog_set_detail(dialog, "This action cannot be undone.");
    const char *buttons[] = {"Cancel", "Delete", NULL};
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 0);
    gtk_alert_dialog_choose(dialog, nw->window, NULL,
                            on_delete_confirm, nw);
    g_object_unref(dialog);
}

static gboolean restore_position_idle(gpointer user_data) {
    NoteWindow *nw = user_data;

    MonitorInfo *mon = NULL;
    if (nw->data->monitor_id)
        mon = monitor_find_by_id(nw->data->monitor_id);
    if (!mon)
        mon = monitor_find_primary();
    if (!mon) return G_SOURCE_REMOVE;

    int abs_x, abs_y;
    monitor_rel_to_abs(mon, nw->data->rel_x, nw->data->rel_y, &abs_x, &abs_y);
    compositor_move_window(nw, abs_x, abs_y);

    return G_SOURCE_REMOVE;
}

static gboolean on_close_request(GtkWindow *window, NoteWindow *nw) {
    (void)window;
    app_sync_positions();
    app_save_notes();
    g_application_quit(G_APPLICATION(nw->app));
    return TRUE;
}

#define COLOR_DOT_SIZE 26

static void color_dot_draw(GtkDrawingArea *area, cairo_t *cr,
                           int width, int height, gpointer user_data) {
    const char *hex = user_data;
    gboolean hovered = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(area), "hovered"));
    unsigned int r, g, b;
    sscanf(hex, "#%02x%02x%02x", &r, &g, &b);

    double cx = width / 2.0;
    double cy = height / 2.0;
    double radius = (COLOR_DOT_SIZE / 2.0) - 1.5;

    cairo_arc(cr, cx, cy, radius, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, r / 255.0, g / 255.0, b / 255.0);
    cairo_fill_preserve(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, hovered ? 0.6 : 0.2);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
}

static void on_color_dot_enter(GtkEventControllerMotion *ctrl,
                               double x, double y, gpointer user_data) {
    (void)ctrl; (void)x; (void)y;
    GtkWidget *dot = user_data;
    g_object_set_data(G_OBJECT(dot), "hovered", GINT_TO_POINTER(1));
    gtk_widget_queue_draw(dot);
}

static void on_color_dot_leave(GtkEventControllerMotion *ctrl,
                               gpointer user_data) {
    (void)ctrl;
    GtkWidget *dot = user_data;
    g_object_set_data(G_OBJECT(dot), "hovered", GINT_TO_POINTER(0));
    gtk_widget_queue_draw(dot);
}

static void color_dot_clicked(GtkGestureClick *gesture, int n_press,
                              double x, double y, gpointer user_data) {
    (void)gesture; (void)n_press; (void)x; (void)y;
    NoteWindow *nw = user_data;
    GtkWidget *dot = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    const char *color = g_object_get_data(G_OBJECT(dot), "color-hex");
    g_free(nw->data->color);
    nw->data->color = g_strdup(color);
    apply_css(nw);
    app_save_notes();
}

static void on_font_dialog_finish(GObject *source, GAsyncResult *res,
                                  gpointer user_data) {
    NoteWindow *nw = user_data;
    GtkFontDialog *dialog = GTK_FONT_DIALOG(source);
    PangoFontDescription *desc = gtk_font_dialog_choose_font_finish(dialog, res, NULL);
    if (desc) {
        char *font_str = pango_font_description_to_string(desc);
        g_free(nw->data->font);
        nw->data->font = font_str;
        pango_font_description_free(desc);
        apply_css(nw);
        app_save_notes();
    }
}

static void on_font_clicked(GtkButton *btn, NoteWindow *nw) {
    (void)btn;
    GtkFontDialog *dialog = gtk_font_dialog_new();
    gtk_font_dialog_set_modal(dialog, TRUE);

    PangoFontDescription *initial = NULL;
    if (nw->data->font && nw->data->font[0]) {
        initial = pango_font_description_from_string(nw->data->font);
    } else {
        char *default_font = g_strdup_printf("%s %d", config_get()->font, config_get()->font_size);
        initial = pango_font_description_from_string(default_font);
        g_free(default_font);
    }

    gtk_font_dialog_choose_font(dialog, nw->window, initial, NULL,
                                on_font_dialog_finish, nw);
    pango_font_description_free(initial);
    g_object_unref(dialog);
}

static void on_md_toggled(GtkToggleButton *btn, NoteWindow *nw) {
    nw->data->md_enabled = gtk_toggle_button_get_active(btn);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    g_signal_handlers_block_by_func(buf, on_text_changed, nw);

    if (!nw->data->md_enabled) {
        format_remove_md_images(nw);
        format_remove_all_md_tags(buf);
    } else {
        format_remove_md_images(nw);
        sync_text(nw);
        format_update_md_tags(nw);
        format_insert_md_images(nw);
    }

    g_signal_handlers_unblock_by_func(buf, on_text_changed, nw);
    app_save_notes();
}

static void on_right_click_suppress(GtkGestureClick *gesture, int n_press,
                                    double x, double y, gpointer user_data) {
    (void)n_press; (void)x; (void)y; (void)user_data;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void on_size_changed(GObject *obj, GParamSpec *pspec, gpointer user_data) {
    (void)obj; (void)pspec;
    NoteWindow *nw = user_data;
    if (!nw->data) return;
    nw->data->width = gtk_widget_get_width(GTK_WIDGET(nw->window));
    nw->data->height = gtk_widget_get_height(GTK_WIDGET(nw->window));
    if (nw->data->width > 0 && nw->data->height > 0)
        app_save_notes();
}

static void on_pointer_motion(GtkEventControllerMotion *ctrl,
                              double x, double y, gpointer user_data) {
    (void)ctrl;
    NoteWindow *nw = user_data;
    nw->pointer_x = x;
    nw->pointer_y = y;
}

static gboolean on_scroll_resize(GtkEventControllerScroll *ctrl,
                                  double dx, double dy, gpointer user_data) {
    (void)dx;
    GdkModifierType state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(ctrl));
    if (!(state & GDK_CONTROL_MASK)) return FALSE;

    NoteWindow *nw = user_data;
    GtkTextView *tv = nw->text_view;

    int bx, by;
    gtk_text_view_window_to_buffer_coords(tv, GTK_TEXT_WINDOW_WIDGET,
                                          (int)nw->pointer_x, (int)nw->pointer_y,
                                          &bx, &by);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(tv);
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buf, &iter);
    ScaledPaintable *sp = NULL;
    while (!gtk_text_iter_is_end(&iter)) {
        GdkPaintable *p = gtk_text_iter_get_paintable(&iter);
        if (p && SCALED_IS_PAINTABLE(p)) {
            GdkRectangle rect;
            gtk_text_view_get_iter_location(tv, &iter, &rect);
            if (bx >= rect.x && bx < rect.x + rect.width &&
                by >= rect.y && by < rect.y + rect.height) {
                sp = SCALED_PAINTABLE(p);
                break;
            }
        }
        gtk_text_iter_forward_char(&iter);
    }
    if (!sp) return FALSE;

    int w = scaled_paintable_get_width(sp);
    int h = scaled_paintable_get_height(sp);
    if (w <= 0 || h <= 0) return FALSE;
    double ratio = (double)h / (double)w;

    int step = (dy < 0) ? 20 : -20;
    int new_w = w + step;
    if (new_w < MIN_IMAGE_SIZE) new_w = MIN_IMAGE_SIZE;
    if (new_w > MAX_IMAGE_WIDTH) new_w = MAX_IMAGE_WIDTH;
    int new_h = (int)(new_w * ratio);
    if (new_h < MIN_IMAGE_SIZE) { new_h = MIN_IMAGE_SIZE; new_w = (int)(new_h / ratio); }

    scaled_paintable_set_size(sp, new_w, new_h);

    sync_text(nw);
    app_save_notes();
    return TRUE;
}

static GtkWidget *build_header(NoteWindow *nw) {
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(header, "note-header");

    GtkWidget *add_btn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_add_css_class(add_btn, "icon-btn");
    gtk_widget_set_tooltip_text(add_btn, "New note");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_clicked), nw);
    gtk_box_append(GTK_BOX(header), add_btn);

    GtkWidget *font_btn = gtk_button_new_from_icon_name("font-select-symbolic");
    gtk_widget_add_css_class(font_btn, "icon-btn");
    gtk_widget_set_tooltip_text(font_btn, "Choose font");
    g_signal_connect(font_btn, "clicked", G_CALLBACK(on_font_clicked), nw);
    gtk_box_append(GTK_BOX(header), font_btn);

    GtkWidget *emoji_btn = gtk_button_new_from_icon_name("face-smile-symbolic");
    gtk_widget_add_css_class(emoji_btn, "icon-btn");
    gtk_widget_set_tooltip_text(emoji_btn, "Insert emoji");
    g_signal_connect(emoji_btn, "clicked", G_CALLBACK(emoji_show_picker), nw);
    gtk_box_append(GTK_BOX(header), emoji_btn);

    GtkWidget *md_btn = gtk_toggle_button_new_with_label("MD");
    gtk_widget_add_css_class(md_btn, "icon-btn");
    gtk_widget_set_tooltip_text(md_btn, "Markdown formatting");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(md_btn), nw->data->md_enabled);
    g_signal_connect(md_btn, "toggled", G_CALLBACK(on_md_toggled), nw);
    gtk_box_append(GTK_BOX(header), md_btn);

    GtkWidget *spacer_l = gtk_label_new("");
    gtk_widget_set_hexpand(spacer_l, TRUE);
    gtk_box_append(GTK_BOX(header), spacer_l);

    GtkWidget *color_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_valign(color_box, GTK_ALIGN_CENTER);
    for (int i = 0; i < NUM_NOTE_COLORS; i++) {
        GtkWidget *dot = gtk_drawing_area_new();
        gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(dot), COLOR_DOT_SIZE);
        gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(dot), COLOR_DOT_SIZE);
        gtk_widget_set_cursor_from_name(dot, "default");
        gtk_widget_set_tooltip_text(dot, NOTE_COLORS[i].name);
        g_object_set_data(G_OBJECT(dot), "color-hex", (gpointer)NOTE_COLORS[i].hex);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(dot),
            color_dot_draw, (gpointer)NOTE_COLORS[i].hex, NULL);

        GtkGesture *click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed", G_CALLBACK(color_dot_clicked), nw);
        gtk_widget_add_controller(dot, GTK_EVENT_CONTROLLER(click));

        GtkEventController *motion = gtk_event_controller_motion_new();
        g_signal_connect(motion, "enter", G_CALLBACK(on_color_dot_enter), dot);
        g_signal_connect(motion, "leave", G_CALLBACK(on_color_dot_leave), dot);
        gtk_widget_add_controller(dot, motion);

        gtk_box_append(GTK_BOX(color_box), dot);
    }
    gtk_box_append(GTK_BOX(header), color_box);

    GtkWidget *spacer_r = gtk_label_new("");
    gtk_widget_set_hexpand(spacer_r, TRUE);
    gtk_box_append(GTK_BOX(header), spacer_r);

    GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "icon-btn");
    gtk_widget_set_tooltip_text(del_btn, "Delete note");
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_clicked), nw);
    gtk_box_append(GTK_BOX(header), del_btn);

    return header;
}

static GtkCssProvider *global_dialog_css = NULL;

static void ensure_dialog_css(void) {
    if (global_dialog_css) return;
    global_dialog_css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(global_dialog_css,
        "window.dialog { background-color: #f0f0f0; color: #000000; }"
        "window.dialog headerbar { background-color: #e0e0e0; color: #000000; }"
        "window.dialog label,"
        "window.dialog button,"
        "window.dialog entry,"
        "window.dialog entry > text,"
        "window.dialog list,"
        "window.dialog list > row,"
        "window.dialog list > row > label,"
        "window.dialog searchbar,"
        "window.dialog searchbar > revealer > box,"
        "window.dialog stack,"
        "window.dialog scrolledwindow,"
        "window.dialog textview,"
        "window.dialog textview > text {"
        "  color: #000000;"
        "  background-color: #ffffff;"
        "}"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(global_dialog_css),
        GTK_STYLE_PROVIDER_PRIORITY_USER + 2);
}

NoteWindow *note_window_new(GtkApplication *app, NoteData *data) {
    emoji_load_db();
    ensure_dialog_css();

    NoteWindow *nw = g_new0(NoteWindow, 1);
    nw->data = data;
    nw->app = app;
    nw->css_provider = NULL;

    nw->window = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(nw->window, "");
    gtk_window_set_default_size(nw->window, data->width, data->height);

    g_signal_connect(nw->window, "notify::default-width",
                     G_CALLBACK(on_size_changed), nw);
    g_signal_connect(nw->window, "notify::default-height",
                     G_CALLBACK(on_size_changed), nw);

    static int note_counter = 0;
    nw->css_class = g_strdup_printf("hanote-w%d", note_counter++);
    gtk_widget_add_css_class(GTK_WIDGET(nw->window), nw->css_class);
    gtk_widget_add_css_class(GTK_WIDGET(nw->window), "hanote");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(nw->window, vbox);

    GtkWidget *header = build_header(nw);
    nw->header = header;
    GtkWidget *handle = gtk_window_handle_new();
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), header);
    gtk_window_set_titlebar(nw->window, handle);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);

    nw->text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_extra_menu(nw->text_view, NULL);
    gtk_text_view_set_wrap_mode(nw->text_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(nw->text_view, 12);
    gtk_text_view_set_right_margin(nw->text_view, 12);
    gtk_text_view_set_top_margin(nw->text_view, 8);
    gtk_text_view_set_bottom_margin(nw->text_view, 8);
    gtk_widget_add_css_class(GTK_WIDGET(nw->text_view), "note-textview");

    if (data->text && data->text[0]) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
        gtk_text_buffer_set_text(buf, data->text, -1);
    }

    GtkTextBuffer *buf = gtk_text_view_get_buffer(nw->text_view);
    g_signal_connect(buf, "changed", G_CALLBACK(on_text_changed), nw);

    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click), GDK_BUTTON_SECONDARY);
    g_signal_connect(right_click, "pressed",
        G_CALLBACK(on_right_click_suppress), NULL);
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(right_click), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(GTK_WIDGET(nw->text_view),
        GTK_EVENT_CONTROLLER(right_click));

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(format_url_motion_cb), nw);
    gtk_widget_add_controller(GTK_WIDGET(nw->text_view), motion);

    GtkGesture *url_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(url_click), GDK_BUTTON_PRIMARY);
    g_signal_connect(url_click, "released", G_CALLBACK(format_url_click_cb), nw);
    gtk_widget_add_controller(GTK_WIDGET(nw->text_view),
        GTK_EVENT_CONTROLLER(url_click));

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(image_paste_key_cb), nw);
    gtk_widget_add_controller(GTK_WIDGET(nw->text_view), key_ctrl);

    GtkDropTarget *drop = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY);
    GType drop_types[] = { GDK_TYPE_TEXTURE, GDK_TYPE_FILE_LIST, G_TYPE_FILE };
    gtk_drop_target_set_gtypes(drop, drop_types, 3);
    g_signal_connect(drop, "drop", G_CALLBACK(image_drop_cb), nw);
    gtk_widget_add_controller(GTK_WIDGET(nw->text_view),
        GTK_EVENT_CONTROLLER(drop));

    GtkEventController *ptr_motion = gtk_event_controller_motion_new();
    g_signal_connect(ptr_motion, "motion", G_CALLBACK(on_pointer_motion), nw);
    gtk_widget_add_controller(GTK_WIDGET(nw->text_view), ptr_motion);

    GtkEventController *scroll_ctrl = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_event_controller_set_propagation_phase(scroll_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(scroll_ctrl, "scroll", G_CALLBACK(on_scroll_resize), nw);
    gtk_widget_add_controller(GTK_WIDGET(nw->window), scroll_ctrl);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(nw->text_view));
    gtk_box_append(GTK_BOX(vbox), scroll);

    apply_css(nw);

    g_signal_handlers_block_by_func(buf, on_text_changed, nw);
    image_reload(nw);
    format_update_url_tags(nw);
    image_restore_file_scheme_tags(buf);
    if (nw->data->md_enabled) {
        format_update_md_tags(nw);
        format_insert_md_images(nw);
    }
    g_signal_handlers_unblock_by_func(buf, on_text_changed, nw);

    g_signal_connect(nw->window, "close-request",
                     G_CALLBACK(on_close_request), nw);

    gtk_window_present(nw->window);

    if (compositor_get_type() != COMPOSITOR_UNKNOWN)
        g_timeout_add(100, restore_position_idle, nw);

    return nw;
}
