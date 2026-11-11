#!/usr/bin/env python3
"""Check M4 management and sort overlays remain above the navigation bar."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
unit = r'''
#include <assert.h>
typedef struct { int x, y, width, height; } EpdRect;
#define KIIKO_BOARD_MURPHY_M4 1
#define UI_LOCK_WIDTH 480
#define UI_LOCK_HEIGHT 800
#define UI_MARGIN 24
#define UI_GAP 8
#define UI_NAV_TOP 736
static int ui_content_width(void) { return 432; }
'''
unit += '#include "ui_book_manage_layout.h"\n'
unit += function("bulk_sort_rect", ROOT / "main/apps/app_book.c") + "\n"
unit += r'''
int main(void) {
    assert(ui_book_manage_rows(false) == 5);
    assert(ui_book_manage_rows(true) == 4);
    for (int i = 0; i < ui_book_manage_rows(false); ++i) {
        EpdRect row = ui_book_manage_row(i), cover = ui_book_manage_cover(false, i);
        assert(row.x == 24 && row.x + row.width == 456 && row.y + row.height < 646);
        assert(cover.x >= row.x && cover.y >= row.y);
        assert(cover.x + cover.width <= row.x + row.width);
        assert(cover.y + cover.height <= row.y + row.height);
    }
    for (int i = 0; i < ui_book_manage_rows(true); ++i) {
        EpdRect card = ui_book_manage_entry(true, i), cover = ui_book_manage_cover(true, i);
        assert(card.x >= 24 && card.x + card.width <= 456 && card.y + card.height < 646);
        assert(cover.x >= card.x && cover.y >= card.y);
        assert(cover.x + cover.width <= card.x + card.width);
        assert(cover.y + cover.height <= card.y + card.height);
    }
    EpdRect choices[] = {bulk_sort_rect(0), bulk_sort_rect(1), bulk_sort_rect(2),
                         bulk_sort_rect(3), bulk_sort_rect(4), bulk_sort_rect(5)};
    for (unsigned i = 0; i < sizeof(choices) / sizeof(choices[0]); ++i) {
        assert(choices[i].x >= 0 && choices[i].x + choices[i].width <= UI_LOCK_WIDTH);
        assert(choices[i].y >= 144 && choices[i].y + choices[i].height <= 720);
    }
    assert(bulk_sort_rect(5).y + bulk_sort_rect(5).height < UI_NAV_TOP);
}
'''

with tempfile.TemporaryDirectory(prefix="m4-management-geometry-") as directory:
    root = Path(directory)
    (root / "epdiy.h").write_text("#pragma once\n")
    source = root / "test.c"
    binary = root / "test"
    source.write_text(unit)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(root),
                    "-I" + str(ROOT / "main/ui"), str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: M4 management list/grid covers, pager clearance and sort hit rectangles")
