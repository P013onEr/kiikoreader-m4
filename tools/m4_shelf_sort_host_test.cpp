/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "m4_shelf_sort.h"

#include <cassert>
#include <cstdio>

int main() {
    const m4_shelf_sort_key_t alpha{"Alpha", "/sdcard/a.epub", 100, 20, false, false};
    const m4_shelf_sort_key_t beta{"Beta", "/sdcard/b.epub", 200, 10, false, false};
    const m4_shelf_sort_key_t unread{"Gamma", "/sdcard/c.epub", 0, 0, false, false};
    const m4_shelf_sort_key_t directory{"Folder", "/sdcard/folder", 0, 0, true, false};
    const m4_shelf_sort_key_t sameNameA{"Same", "/sdcard/a.epub", 0, 0, false, false};
    const m4_shelf_sort_key_t sameNameB{"same", "/sdcard/b.epub", 0, 0, false, false};
    const m4_shelf_sort_key_t favorite{"Zeta", "/sdcard/z.epub", 0, 0, false, true};

    assert(m4_shelf_sort_compare(&alpha, &beta, M4_SHELF_SORT_NAME_ASC) < 0);
    assert(m4_shelf_sort_compare(&alpha, &beta, M4_SHELF_SORT_NAME_DESC) > 0);
    assert(m4_shelf_sort_compare(&beta, &alpha, M4_SHELF_SORT_RECENT) < 0);
    assert(m4_shelf_sort_compare(&alpha, &unread, M4_SHELF_SORT_RECENT) < 0);
    assert(m4_shelf_sort_compare(&beta, &alpha, M4_SHELF_SORT_ADDED) < 0);
    assert(m4_shelf_sort_compare(&directory, &alpha, M4_SHELF_SORT_NAME_DESC) < 0);
    assert(m4_shelf_sort_compare(&sameNameA, &sameNameB, M4_SHELF_SORT_NAME_ASC) < 0);
    assert(m4_shelf_sort_compare(&sameNameB, &sameNameA, M4_SHELF_SORT_NAME_ASC) > 0);
    assert(m4_shelf_sort_compare(&favorite, &alpha, M4_SHELF_SORT_NAME_ASC) < 0);
    std::puts("M4 shelf sort tests passed");
}
