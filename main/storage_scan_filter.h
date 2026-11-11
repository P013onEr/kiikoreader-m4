/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * Read-only filtering for operating-system metadata and incomplete transfer files.
 */
#pragma once

#include <stdbool.h>
#include <string.h>
#include <strings.h>

static inline bool storage_name_is_os_metadata(const char *name) {
    if (!name || !*name || name[0] == '.') return true;
    static const char *const names[] = {
        "Thumbs.db", "ehthumbs.db", "desktop.ini", "folder.htt",
        "IconCache.db", "IndexerVolumeGuid", "WPSettings.dat",
        "$RECYCLE.BIN", "RECYCLER", "System Volume Information",
        "__MACOSX", "Temporary Items",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!strcasecmp(name, names[i])) return true;
    return false;
}

static inline bool storage_name_is_os_directory(const char *name) {
    if (storage_name_is_os_metadata(name)) return true;
    return !strcasecmp(name, ".Spotlight-V100") ||
           !strcasecmp(name, ".Trashes") ||
           !strcasecmp(name, ".fseventsd");
}

static inline bool storage_file_is_temporary(const char *name) {
    if (!name || !*name || !strncmp(name, "~$", 2)) return true;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return !strcasecmp(dot, ".tmp") || !strcasecmp(dot, ".temp") ||
           !strcasecmp(dot, ".part") || !strcasecmp(dot, ".crdownload") ||
           !strcasecmp(dot, ".download");
}
