// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "monitor.h"
#include <string.h>

static GArray *monitors = NULL;

static void clear_monitors(void) {
    if (!monitors) return;
    for (guint i = 0; i < monitors->len; i++) {
        MonitorInfo *mi = &g_array_index(monitors, MonitorInfo, i);
        g_free(mi->id);
    }
    g_array_set_size(monitors, 0);
}

char *monitor_make_id(GdkMonitor *mon) {
    const char *mfr   = gdk_monitor_get_manufacturer(mon);
    const char *model = gdk_monitor_get_model(mon);
    if (mfr && mfr[0] && model && model[0])
        return g_strdup_printf("%s %s", mfr, model);
    const char *conn = gdk_monitor_get_connector(mon);
    return g_strdup(conn ? conn : "unknown");
}

void monitor_refresh(void) {
    if (!monitors)
        monitors = g_array_new(FALSE, FALSE, sizeof(MonitorInfo));
    clear_monitors();

    GdkDisplay *display = gdk_display_get_default();
    GListModel *list = gdk_display_get_monitors(display);
    guint n = g_list_model_get_n_items(list);

    /* First pass: build candidate IDs */
    char **ids = g_new0(char *, n);
    for (guint i = 0; i < n; i++) {
        GdkMonitor *mon = g_list_model_get_item(list, i);
        ids[i] = monitor_make_id(mon);
        g_object_unref(mon);
    }

    /* Second pass: append connector for duplicates */
    for (guint i = 0; i < n; i++) {
        gboolean dup = FALSE;
        for (guint j = 0; j < n; j++) {
            if (i != j && strcmp(ids[i], ids[j]) == 0) {
                dup = TRUE;
                break;
            }
        }
        GdkMonitor *mon = g_list_model_get_item(list, i);
        MonitorInfo mi = {0};
        if (dup) {
            const char *conn = gdk_monitor_get_connector(mon);
            mi.id = g_strdup_printf("%s [%s]", ids[i], conn ? conn : "?");
        } else {
            mi.id = g_strdup(ids[i]);
        }
        gdk_monitor_get_geometry(mon, &mi.geometry);
        mi.gdk_ref = mon;
        g_array_append_val(monitors, mi);
        g_object_unref(mon);
    }

    for (guint i = 0; i < n; i++)
        g_free(ids[i]);
    g_free(ids);
}

void monitor_init(void) {
    monitor_refresh();
    GdkDisplay *display = gdk_display_get_default();
    g_signal_connect_swapped(gdk_display_get_monitors(display),
        "items-changed", G_CALLBACK(monitor_refresh), NULL);
}

MonitorInfo *monitor_find_by_id(const char *id) {
    if (!id || !monitors) return NULL;
    for (guint i = 0; i < monitors->len; i++) {
        MonitorInfo *mi = &g_array_index(monitors, MonitorInfo, i);
        if (strcmp(mi->id, id) == 0) return mi;
    }
    /* Partial match: saved ID might lack connector suffix, or vice versa */
    for (guint i = 0; i < monitors->len; i++) {
        MonitorInfo *mi = &g_array_index(monitors, MonitorInfo, i);
        if (g_str_has_prefix(mi->id, id) || g_str_has_prefix(id, mi->id))
            return mi;
    }
    return NULL;
}

MonitorInfo *monitor_find_primary(void) {
    if (!monitors || monitors->len == 0) return NULL;
    for (guint i = 0; i < monitors->len; i++) {
        MonitorInfo *mi = &g_array_index(monitors, MonitorInfo, i);
        if (mi->geometry.x == 0 && mi->geometry.y == 0) return mi;
    }
    return &g_array_index(monitors, MonitorInfo, 0);
}

MonitorInfo *monitor_at_point(int abs_x, int abs_y) {
    if (!monitors) return NULL;
    for (guint i = 0; i < monitors->len; i++) {
        MonitorInfo *mi = &g_array_index(monitors, MonitorInfo, i);
        GdkRectangle *g = &mi->geometry;
        if (abs_x >= g->x && abs_x < g->x + g->width &&
            abs_y >= g->y && abs_y < g->y + g->height)
            return mi;
    }
    return monitor_find_primary();
}

void monitor_abs_to_rel(MonitorInfo *mon, int abs_x, int abs_y,
                         double *rel_x, double *rel_y) {
    *rel_x = (mon->geometry.width > 0)
        ? (double)(abs_x - mon->geometry.x) / mon->geometry.width : 0.0;
    *rel_y = (mon->geometry.height > 0)
        ? (double)(abs_y - mon->geometry.y) / mon->geometry.height : 0.0;
    if (*rel_x < 0.0) *rel_x = 0.0;
    if (*rel_y < 0.0) *rel_y = 0.0;
    if (*rel_x > 1.0) *rel_x = 1.0;
    if (*rel_y > 1.0) *rel_y = 1.0;
}

void monitor_rel_to_abs(MonitorInfo *mon, double rel_x, double rel_y,
                         int *abs_x, int *abs_y) {
    *abs_x = mon->geometry.x + (int)(rel_x * mon->geometry.width);
    *abs_y = mon->geometry.y + (int)(rel_y * mon->geometry.height);
}
