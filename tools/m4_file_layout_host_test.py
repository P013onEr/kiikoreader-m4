#!/usr/bin/env python3
"""Check M4 file-list/grid geometry, pager clearance, and matching touch cells."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / "main/apps/app_files.c"

unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef struct { int x, y, width, height; } EpdRect;
#define KIIKO_BOARD_MURPHY_M4 1
#define UI_LOCK_WIDTH 480
#define UI_LOCK_HEIGHT 800
#define UI_MARGIN 24
#define UI_FILES_LIST_TOP 250
#define UI_FILES_ROWS 6
#define UI_FILES_ROW_HEIGHT 58
#define UI_PAGER_TOP 646
enum { FILE_VIEW_LIST, FILE_VIEW_MOVE };
static int s_view = FILE_VIEW_LIST;
static bool s_file_grid;
static EpdRect file_action_rect(int index) { return (EpdRect){index ? 244 : 24, 566, 212, 56}; }
static bool ui_rect_hit(EpdRect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}
'''
for name in ("file_entry_rect", "file_entry_hit", "visible_rows"):
    unit += function(name, source) + "\n"
unit += r'''
int main(void) {
    assert(visible_rows() == 6);
    for (int grid = 0; grid < 2; ++grid) {
        s_file_grid = grid != 0;
        assert(visible_rows() == 6);
        for (int i = 0; i < visible_rows(); ++i) {
            EpdRect r = file_entry_rect(i);
            assert(r.x >= 0 && r.y >= UI_FILES_LIST_TOP);
            assert(r.x + r.width <= UI_LOCK_WIDTH);
            assert(r.y + r.height < UI_PAGER_TOP);
            assert(file_entry_hit(r.x + r.width / 2, r.y + r.height / 2) == i);
        }
    }
    s_file_grid = true;
    assert(file_entry_rect(4).y == 494 && file_entry_rect(5).y == 494);
    assert(file_entry_rect(5).y + file_entry_rect(5).height == 606);
    s_file_grid = false;
    assert(file_entry_rect(5).y + file_entry_rect(5).height == 598);
    s_view = FILE_VIEW_MOVE;
    assert(visible_rows() == 3);
    puts("PASS: M4 list/grid use six hit-matched entries above pager; move rows fit above actions");
}
'''

with tempfile.TemporaryDirectory(prefix="m4-file-layout-") as directory:
    c_file = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    c_file.write_text(unit)
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
         str(c_file), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
