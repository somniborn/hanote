// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <glib.h>

typedef struct {
    char *font;
    int   font_size;
    char *color;
    int   width;
    int   height;
} Config;

void         config_load(void);
const Config *config_get(void);
const char   *config_color_to_hex(const char *name);
