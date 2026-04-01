// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "note.h"

void     image_reload(NoteWindow *nw);
void     image_restore_file_scheme_tags(GtkTextBuffer *buf);

gboolean image_paste_key_cb(GtkEventControllerKey *ctrl, guint keyval,
                            guint keycode, GdkModifierType state,
                            gpointer user_data);
gboolean image_drop_cb(GtkDropTarget *target, const GValue *value,
                       double x, double y, gpointer user_data);
