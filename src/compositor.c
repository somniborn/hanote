// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "compositor.h"
#include <string.h>
#include <json-glib/json-glib.h>

static CompositorType comp_type = COMPOSITOR_UNKNOWN;

void compositor_init(void) {
    if (g_getenv("HYPRLAND_INSTANCE_SIGNATURE"))
        comp_type = COMPOSITOR_HYPRLAND;
    else if (g_getenv("SWAYSOCK"))
        comp_type = COMPOSITOR_SWAY;
    else if (g_getenv("KDE_SESSION_VERSION"))
        comp_type = COMPOSITOR_KDE;
    else
        comp_type = COMPOSITOR_UNKNOWN;
}

CompositorType compositor_get_type(void) {
    return comp_type;
}

static char *run_cmd(const char *cmd) {
    char *out = NULL;
    g_spawn_command_line_sync(cmd, &out, NULL, NULL, NULL);
    return out;
}

/* ── Hyprland ──────────────────────────────────────────────────── */

static char *hypr_find_address(NoteWindow *nw) {
    char *json_str = run_cmd("hyprctl clients -j");
    if (!json_str) return NULL;

    char *addr = NULL;
    JsonParser *parser = json_parser_new();
    if (json_parser_load_from_data(parser, json_str, -1, NULL)) {
        JsonArray *arr = json_node_get_array(json_parser_get_root(parser));
        for (guint i = 0; i < json_array_get_length(arr); i++) {
            JsonObject *obj = json_array_get_object_element(arr, i);
            const char *title = json_object_get_string_member(obj, "title");
            const char *cls = json_object_get_string_member(obj, "class");
            if (cls && strcmp(cls, "com.suhokang.hanote") == 0 &&
                title && nw->data->id && strcmp(title, nw->data->id) == 0) {
                const char *a = json_object_get_string_member(obj, "address");
                if (a) addr = g_strdup(a);
                break;
            }
        }
    }
    g_object_unref(parser);
    g_free(json_str);
    return addr;
}

static void hypr_move(NoteWindow *nw, int x, int y) {
    gtk_window_set_title(nw->window, nw->data->id);

    /* Small delay for compositor to register the title change */
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);

    char *addr = hypr_find_address(nw);
    if (addr) {
        char *cmd = g_strdup_printf(
            "hyprctl dispatch movewindowpixel exact %d %d,address:%s", x, y, addr);
        g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
        g_free(cmd);
        g_free(addr);
    }
    gtk_window_set_title(nw->window, "");
}

static gboolean hypr_get_pos(NoteWindow *nw, int *x, int *y) {
    gtk_window_set_title(nw->window, nw->data->id);
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);

    gboolean found = FALSE;
    char *json_str = run_cmd("hyprctl clients -j");
    if (!json_str) goto done;

    JsonParser *parser = json_parser_new();
    if (json_parser_load_from_data(parser, json_str, -1, NULL)) {
        JsonArray *arr = json_node_get_array(json_parser_get_root(parser));
        for (guint i = 0; i < json_array_get_length(arr); i++) {
            JsonObject *obj = json_array_get_object_element(arr, i);
            const char *title = json_object_get_string_member(obj, "title");
            const char *cls = json_object_get_string_member(obj, "class");
            if (cls && strcmp(cls, "com.suhokang.hanote") == 0 &&
                title && nw->data->id && strcmp(title, nw->data->id) == 0) {
                JsonArray *at = json_object_get_array_member(obj, "at");
                if (at && json_array_get_length(at) >= 2) {
                    *x = (int)json_array_get_int_element(at, 0);
                    *y = (int)json_array_get_int_element(at, 1);
                    found = TRUE;
                }
                break;
            }
        }
    }
    g_object_unref(parser);
    g_free(json_str);
done:
    gtk_window_set_title(nw->window, "");
    return found;
}

/* ── Sway ──────────────────────────────────────────────────────── */

static int sway_find_con_id(NoteWindow *nw, JsonNode *node) {
    if (!node || !JSON_NODE_HOLDS_OBJECT(node)) return -1;
    JsonObject *obj = json_node_get_object(node);

    if (json_object_has_member(obj, "app_id") && json_object_has_member(obj, "name")) {
        const char *app = json_object_get_string_member(obj, "app_id");
        const char *name = json_object_get_string_member(obj, "name");
        if (app && strcmp(app, "com.suhokang.hanote") == 0 &&
            name && nw->data->id && strcmp(name, nw->data->id) == 0) {
            return (int)json_object_get_int_member(obj, "id");
        }
    }

    if (json_object_has_member(obj, "nodes")) {
        JsonArray *nodes = json_object_get_array_member(obj, "nodes");
        for (guint i = 0; i < json_array_get_length(nodes); i++) {
            int id = sway_find_con_id(nw, json_array_get_element(nodes, i));
            if (id >= 0) return id;
        }
    }
    if (json_object_has_member(obj, "floating_nodes")) {
        JsonArray *nodes = json_object_get_array_member(obj, "floating_nodes");
        for (guint i = 0; i < json_array_get_length(nodes); i++) {
            int id = sway_find_con_id(nw, json_array_get_element(nodes, i));
            if (id >= 0) return id;
        }
    }
    return -1;
}

static void sway_move(NoteWindow *nw, int x, int y) {
    gtk_window_set_title(nw->window, nw->data->id);
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);

    char *json_str = run_cmd("swaymsg -t get_tree");
    if (!json_str) goto done;

    JsonParser *parser = json_parser_new();
    if (json_parser_load_from_data(parser, json_str, -1, NULL)) {
        int con_id = sway_find_con_id(nw, json_parser_get_root(parser));
        if (con_id >= 0) {
            char *cmd = g_strdup_printf(
                "swaymsg '[con_id=%d] floating enable, move position %d %d'",
                con_id, x, y);
            g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
            g_free(cmd);
        }
    }
    g_object_unref(parser);
    g_free(json_str);
done:
    gtk_window_set_title(nw->window, "");
}

static gboolean sway_get_pos_recursive(NoteWindow *nw, JsonNode *node,
                                        int *x, int *y) {
    if (!node || !JSON_NODE_HOLDS_OBJECT(node)) return FALSE;
    JsonObject *obj = json_node_get_object(node);

    if (json_object_has_member(obj, "app_id") && json_object_has_member(obj, "name")) {
        const char *app = json_object_get_string_member(obj, "app_id");
        const char *name = json_object_get_string_member(obj, "name");
        if (app && strcmp(app, "com.suhokang.hanote") == 0 &&
            name && nw->data->id && strcmp(name, nw->data->id) == 0) {
            if (json_object_has_member(obj, "rect")) {
                JsonObject *rect = json_object_get_object_member(obj, "rect");
                *x = (int)json_object_get_int_member(rect, "x");
                *y = (int)json_object_get_int_member(rect, "y");
                return TRUE;
            }
        }
    }

    const char *keys[] = {"nodes", "floating_nodes", NULL};
    for (int k = 0; keys[k]; k++) {
        if (!json_object_has_member(obj, keys[k])) continue;
        JsonArray *arr = json_object_get_array_member(obj, keys[k]);
        for (guint i = 0; i < json_array_get_length(arr); i++) {
            if (sway_get_pos_recursive(nw, json_array_get_element(arr, i), x, y))
                return TRUE;
        }
    }
    return FALSE;
}

static gboolean sway_get_pos(NoteWindow *nw, int *x, int *y) {
    gtk_window_set_title(nw->window, nw->data->id);
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);

    gboolean found = FALSE;
    char *json_str = run_cmd("swaymsg -t get_tree");
    if (json_str) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, json_str, -1, NULL))
            found = sway_get_pos_recursive(nw, json_parser_get_root(parser), x, y);
        g_object_unref(parser);
        g_free(json_str);
    }
    gtk_window_set_title(nw->window, "");
    return found;
}

/* ── KDE ───────────────────────────────────────────────────────── */

static void kde_move(NoteWindow *nw, int x, int y) {
    gtk_window_set_title(nw->window, nw->data->id);
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);

    char *find_cmd = g_strdup_printf(
        "kdotool search --class com.suhokang.hanote --name '%s'", nw->data->id);
    char *wid_str = run_cmd(find_cmd);
    g_free(find_cmd);

    if (wid_str && wid_str[0]) {
        g_strstrip(wid_str);
        char *cmd = g_strdup_printf("kdotool windowmove %s %d %d", wid_str, x, y);
        g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
        g_free(cmd);
    }
    g_free(wid_str);
    gtk_window_set_title(nw->window, "");
}

static gboolean kde_get_pos(NoteWindow *nw, int *x, int *y) {
    gtk_window_set_title(nw->window, nw->data->id);
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);

    gboolean found = FALSE;
    char *find_cmd = g_strdup_printf(
        "kdotool search --class com.suhokang.hanote --name '%s'", nw->data->id);
    char *wid_str = run_cmd(find_cmd);
    g_free(find_cmd);

    if (wid_str && wid_str[0]) {
        g_strstrip(wid_str);
        char *geo_cmd = g_strdup_printf("kdotool windowgeometry %s", wid_str);
        char *geo = run_cmd(geo_cmd);
        g_free(geo_cmd);
        if (geo) {
            /* kdotool windowgeometry output: "Position: X,Y\nSize: WxH" */
            int px, py;
            if (sscanf(geo, "Position: %d,%d", &px, &py) == 2) {
                *x = px;
                *y = py;
                found = TRUE;
            }
            g_free(geo);
        }
    }
    g_free(wid_str);
    gtk_window_set_title(nw->window, "");
    return found;
}

/* ── Public API ────────────────────────────────────────────────── */

void compositor_move_window(NoteWindow *nw, int abs_x, int abs_y) {
    switch (comp_type) {
    case COMPOSITOR_HYPRLAND: hypr_move(nw, abs_x, abs_y); break;
    case COMPOSITOR_SWAY:     sway_move(nw, abs_x, abs_y); break;
    case COMPOSITOR_KDE:      kde_move(nw, abs_x, abs_y);  break;
    case COMPOSITOR_UNKNOWN:  break;
    }
}

gboolean compositor_get_window_position(NoteWindow *nw, int *abs_x, int *abs_y) {
    switch (comp_type) {
    case COMPOSITOR_HYPRLAND: return hypr_get_pos(nw, abs_x, abs_y);
    case COMPOSITOR_SWAY:     return sway_get_pos(nw, abs_x, abs_y);
    case COMPOSITOR_KDE:      return kde_get_pos(nw, abs_x, abs_y);
    case COMPOSITOR_UNKNOWN:  return FALSE;
    }
    return FALSE;
}
