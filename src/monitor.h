// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <gtk/gtk.h>

typedef struct {
    char         *id;
    GdkRectangle  geometry;
    GdkMonitor   *gdk_ref;
} MonitorInfo;

void         monitor_init(void);
void         monitor_refresh(void);
MonitorInfo *monitor_find_by_id(const char *id);
MonitorInfo *monitor_find_primary(void);
MonitorInfo *monitor_at_point(int abs_x, int abs_y);
char        *monitor_make_id(GdkMonitor *mon);

void monitor_abs_to_rel(MonitorInfo *mon, int abs_x, int abs_y,
                         double *rel_x, double *rel_y);
void monitor_rel_to_abs(MonitorInfo *mon, double rel_x, double rel_y,
                         int *abs_x, int *abs_y);
