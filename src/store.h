// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "note.h"

GList *store_load(void);
void   store_save(GList *notes);
char  *store_get_path(void);
char  *store_get_images_dir(void);
