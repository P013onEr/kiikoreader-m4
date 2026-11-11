/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>

typedef enum {
    M4_SHELF_SORT_NAME_ASC = 0,
    M4_SHELF_SORT_NAME_DESC = 1,
    M4_SHELF_SORT_RECENT = 2,
    M4_SHELF_SORT_ADDED = 3
} m4_shelf_sort_t;

typedef struct {
    const char *title;
    const char *path;
    uint32_t last_read;
    uint64_t added_order;
    bool directory;
    bool favorite;
} m4_shelf_sort_key_t;

static inline int m4_shelf_sort_compare(const m4_shelf_sort_key_t *a,
                                        const m4_shelf_sort_key_t *b,
                                        m4_shelf_sort_t mode) {
    if (a->directory != b->directory) return a->directory ? -1 : 1;
    if (a->favorite != b->favorite) return a->favorite ? -1 : 1;
    if (mode == M4_SHELF_SORT_RECENT && a->last_read != b->last_read)
        return a->last_read > b->last_read ? -1 : 1;
    if (mode == M4_SHELF_SORT_ADDED && a->added_order != b->added_order)
        return a->added_order < b->added_order ? -1 : 1;
    const int name = strcasecmp(a->title, b->title);
    if (name) return mode == M4_SHELF_SORT_NAME_DESC ? -name : name;
    return strcmp(a->path, b->path);
}
