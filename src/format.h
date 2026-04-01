// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "note.h"

void  format_update_url_tags(NoteWindow *nw);
char *format_get_url_at_iter(GtkTextBuffer *buf, GtkTextIter *iter);

void  format_url_motion_cb(GtkEventControllerMotion *ctrl,
                           double x, double y, gpointer user_data);
void  format_url_click_cb(GtkGestureClick *gesture, int n_press,
                          double x, double y, gpointer user_data);

void  format_ensure_md_tags(GtkTextBuffer *buf);
void  format_update_md_tags(NoteWindow *nw);
void  format_remove_all_md_tags(GtkTextBuffer *buf);
void  format_remove_md_images(NoteWindow *nw);
void  format_insert_md_images(NoteWindow *nw);
