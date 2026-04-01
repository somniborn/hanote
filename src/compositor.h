// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "note.h"

typedef enum {
    COMPOSITOR_HYPRLAND,
    COMPOSITOR_SWAY,
    COMPOSITOR_KDE,
    COMPOSITOR_UNKNOWN
} CompositorType;

void           compositor_init(void);
CompositorType compositor_get_type(void);
void           compositor_move_window(NoteWindow *nw, int abs_x, int abs_y);
gboolean       compositor_get_window_position(NoteWindow *nw, int *abs_x, int *abs_y);
