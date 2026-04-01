// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "note.h"
#include "store.h"
#include "config.h"
#include "monitor.h"
#include "compositor.h"

GList *all_notes = NULL;
static GList *all_windows = NULL;

void app_save_notes(void) {
    store_save(all_notes);
}

void app_sync_positions(void) {
    if (compositor_get_type() == COMPOSITOR_UNKNOWN) return;
    for (GList *l = all_windows; l; l = l->next) {
        NoteWindow *nw = l->data;
        int abs_x, abs_y;
        if (compositor_get_window_position(nw, &abs_x, &abs_y)) {
            MonitorInfo *mon = monitor_at_point(abs_x, abs_y);
            if (mon) {
                g_free(nw->data->monitor_id);
                nw->data->monitor_id = g_strdup(mon->id);
                monitor_abs_to_rel(mon, abs_x, abs_y,
                                   &nw->data->rel_x, &nw->data->rel_y);
            }
        }
    }
}

void app_create_note(GtkApplication *app, const char *color) {
    NoteData *nd = note_data_new(color);
    int count = g_list_length(all_notes);
    nd->rel_x = 0.3 + 0.02 * (count % 10);
    nd->rel_y = 0.3 + 0.02 * (count % 10);
    all_notes = g_list_append(all_notes, nd);
    NoteWindow *nw = note_window_new(app, nd);
    all_windows = g_list_append(all_windows, nw);
    app_save_notes();
}

void app_remove_window(NoteWindow *nw) {
    all_windows = g_list_remove(all_windows, nw);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    monitor_init();
    compositor_init();

    all_notes = store_load();

    if (all_notes == NULL) {
        const Config *c = config_get();
        const char *hex = config_color_to_hex(c->color);
        if (!hex) hex = "#fff9b1";
        NoteData *nd = note_data_new(hex);
        all_notes = g_list_append(all_notes, nd);
    }

    for (GList *l = all_notes; l != NULL; l = l->next) {
        NoteData *nd = l->data;
        NoteWindow *nw = note_window_new(app, nd);
        all_windows = g_list_append(all_windows, nw);
    }

    app_save_notes();
}

int main(int argc, char *argv[]) {
    config_load();

    GtkApplication *app = gtk_application_new("com.suhokang.hanote",
                                               G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    g_list_free_full(all_notes, (GDestroyNotify)note_data_free);
    g_list_free(all_windows);
    g_object_unref(app);

    return status;
}
