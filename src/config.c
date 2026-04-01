// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Config cfg = {
    .font      = NULL,
    .font_size = 18,
    .color     = NULL,
    .width     = 300,
    .height    = 300,
};

static const char *valid_colors[] = {
    "yellow", "green", "blue", "pink", "purple", "orange", "white", "random", NULL
};

static gboolean is_valid_color(const char *c) {
    for (int i = 0; valid_colors[i]; i++)
        if (g_ascii_strcasecmp(valid_colors[i], c) == 0)
            return TRUE;
    return FALSE;
}

/* Parse a single uzon line:
 *   key is value
 *   key is value from opt1, opt2, ...
 */
static void parse_line(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || (line[0] == '/' && line[1] == '/')) return;

    const char *is_sep = strstr(line, " is ");
    if (!is_sep) return;

    char *key = g_strndup(line, is_sep - line);
    g_strstrip(key);

    const char *rest = is_sep + 4; /* skip " is " */

    /* Check for " from " — extract value before it */
    const char *from = strstr(rest, " from ");
    char *raw_value;
    if (from)
        raw_value = g_strndup(rest, from - rest);
    else
        raw_value = g_strdup(rest);
    g_strstrip(raw_value);

    char *value;
    size_t len = strlen(raw_value);
    if (len >= 2 && raw_value[0] == '"' && raw_value[len - 1] == '"')
        value = g_strndup(raw_value + 1, len - 2);
    else
        value = g_strdup(raw_value);
    g_free(raw_value);

    if (strcmp(key, "font") == 0) {
        g_free(cfg.font);
        cfg.font = g_strdup(value);
    } else if (strcmp(key, "font_size") == 0) {
        int v = atoi(value);
        if (v > 0) cfg.font_size = v;
    } else if (strcmp(key, "color") == 0) {
        if (is_valid_color(value)) {
            g_free(cfg.color);
            cfg.color = g_strdup(value);
        }
    } else if (strcmp(key, "width") == 0) {
        int v = atoi(value);
        if (v > 0) cfg.width = v;
    } else if (strcmp(key, "height") == 0) {
        int v = atoi(value);
        if (v > 0) cfg.height = v;
    }

    g_free(key);
    g_free(value);
}

void config_load(void) {
    if (!cfg.font)  cfg.font  = g_strdup("Sans");
    if (!cfg.color) cfg.color = g_strdup("yellow");

    char *path = g_build_filename(g_get_user_config_dir(),
                                  "hanote", "config.uzon", NULL);
    char *contents = NULL;
    if (g_file_get_contents(path, &contents, NULL, NULL)) {
        char **lines = g_strsplit(contents, "\n", -1);
        for (int i = 0; lines[i]; i++)
            parse_line(lines[i]);
        g_strfreev(lines);
        g_free(contents);
    }
    g_free(path);
}

const Config *config_get(void) {
    return &cfg;
}

const char *config_color_to_hex(const char *name) {
    if (!name) return NULL;
    static const struct { const char *name; const char *hex; } map[] = {
        {"yellow",  "#fff9b1"},
        {"green",   "#c1f0c1"},
        {"blue",    "#a8d8ea"},
        {"pink",    "#f8c8dc"},
        {"purple",  "#d5b8ff"},
        {"orange",  "#ffd6a5"},
        {"white",   "#ffffff"},
        {NULL, NULL}
    };
    if (g_ascii_strcasecmp(name, "random") == 0) {
        int n = sizeof(map) / sizeof(map[0]) - 1; /* exclude NULL sentinel */
        return map[g_random_int_range(0, n)].hex;
    }
    for (int i = 0; map[i].name; i++)
        if (g_ascii_strcasecmp(map[i].name, name) == 0)
            return map[i].hex;
    return NULL;
}
