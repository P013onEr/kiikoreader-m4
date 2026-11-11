/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：文件首页和目录的固定布局，绘制与命中共用；内容不进入底栏保护带。
 * English: Shared file-hub/directory paint and hit geometry, outside the footer guard.
 */
#pragma once
#include "epdiy.h"
#include "ui_kit.h"
#include "ui_nav_layout.h"
#if KIIKO_BOARD_MURPHY_M4
#define UI_FILES_LIST_TOP 250
#define UI_FILES_ROWS 6
#else
#define UI_FILES_LIST_TOP 210
#define UI_FILES_ROWS 7
#endif
#define UI_FILES_ROW_HEIGHT 58
#define UI_FILES_LIST_END (UI_FILES_LIST_TOP + UI_FILES_ROW_HEIGHT * UI_FILES_ROWS)
#define UI_FILES_FOOTER_TOP UI_NAV_TOP
static inline EpdRect ui_files_storage_rect(void) { return (EpdRect){UI_MARGIN,140,UI_LOCK_WIDTH-2*UI_MARGIN,130}; }
static inline EpdRect ui_files_category_rect(int i) { int w=(UI_LOCK_WIDTH-2*UI_MARGIN-2*UI_GAP)/3; return (EpdRect){UI_MARGIN+i*(w+UI_GAP),290,w,120}; }
static inline EpdRect ui_files_transfer_rect(int i) { return (EpdRect){UI_MARGIN,430+i*70,UI_LOCK_WIDTH-2*UI_MARGIN,70}; }
static inline EpdRect ui_files_batch_rect(int i) {
    int x=i*UI_LOCK_WIDTH/4, end=(i+1)*UI_LOCK_WIDTH/4;
    return (EpdRect){x,UI_FILES_FOOTER_TOP,end-x,UI_NAV_HEIGHT};
}
/* Small panels use the same proportions and a shorter content viewport. */
