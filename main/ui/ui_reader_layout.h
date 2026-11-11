/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：标准显示状态栏、返回/书名/收藏及底部工具栏；非沉浸全屏只保留状态及章节进度，沉浸全屏保留铺屏细线。
 * English: Standard shows status, Back/title/Favorite and bottom tools; nonimmersive fullscreen retains status/chapter progress and immersive fullscreen retains a thin line.
 */
#pragma once
#include <stdbool.h>
#include "epdiy.h"
#include "ui_kit.h"
/* Reader geometry follows the active portrait canvas; M4 has a compact
 * header/footer while retaining the project's overlay behavior. */
#if KIIKO_BOARD_MURPHY_M4
enum { UI_READER_STANDARD_TOP=88, UI_READER_STANDARD_BOTTOM=676,
       UI_READER_METADATA_TOP=48, UI_READER_METADATA_BOTTOM=720,
       UI_READER_FULLSCREEN_TOP=0, UI_READER_FULLSCREEN_BOTTOM=768,
       UI_READER_FOOTER_TEXT_Y=744, UI_READER_FOOTER_TEXT_PX=16,
       UI_READER_HEADER_TEXT_PX=20, UI_READER_TOOLS_TOP=736 };
#else
enum { UI_READER_STANDARD_TOP=176, UI_READER_STANDARD_BOTTOM=1048,
       UI_READER_METADATA_TOP=80, UI_READER_METADATA_BOTTOM=1168,
       UI_READER_FULLSCREEN_TOP=0, UI_READER_FULLSCREEN_BOTTOM=1204,
       UI_READER_FOOTER_TEXT_Y=1180, UI_READER_FOOTER_TEXT_PX=21,
       UI_READER_HEADER_TEXT_PX=25, UI_READER_TOOLS_TOP=1096 };
#endif
static inline EpdRect ui_reader_area(bool fullscreen,bool immersive) {
    int top=!fullscreen?UI_READER_STANDARD_TOP:immersive?UI_READER_FULLSCREEN_TOP:UI_READER_METADATA_TOP;
    int bottom=!fullscreen?UI_READER_STANDARD_BOTTOM:immersive?UI_READER_FULLSCREEN_BOTTOM:UI_READER_METADATA_BOTTOM;
    return (EpdRect){0,top,UI_LOCK_WIDTH,bottom-top};
}
static inline EpdRect ui_reader_body(bool fullscreen,bool immersive,int margin,bool vertical) {
    // 工具栏及题头悬浮在正文上；显隐只改变可见/刷新范围，不改变排版。
    // Header/tools overlay the body; visibility changes the paint crop, never pagination.
    (void)fullscreen;
    EpdRect r=ui_reader_area(true,immersive);
    // 正式版正文上留24像素、下留2像素；横竖排继续使用用户边距。
    // Retain the release's24px top and2px bottom text inset, with configured horizontal/vertical margins.
    if(immersive){r.y+=24;r.height-=26;}
    if(vertical)return (EpdRect){margin,r.y+margin,UI_LOCK_WIDTH-2*margin,r.height-2*margin};
    return (EpdRect){margin,r.y,UI_LOCK_WIDTH-2*margin,r.height};
}
static inline EpdRect ui_reader_header(void){return (EpdRect){0,0,UI_LOCK_WIDTH,UI_READER_METADATA_TOP};}
static inline EpdRect ui_reader_back_control(void){return (EpdRect){UI_MARGIN,UI_READER_METADATA_TOP,64,56};}
static inline EpdRect ui_reader_favorite_control(void){return (EpdRect){UI_LOCK_WIDTH-UI_MARGIN-64,UI_READER_METADATA_TOP,64,56};}
static inline EpdRect ui_reader_footer(void){return (EpdRect){0,UI_READER_METADATA_BOTTOM,UI_LOCK_WIDTH,UI_LOCK_HEIGHT-UI_READER_METADATA_BOTTOM};}
static inline EpdRect ui_reader_progress(void){return (EpdRect){UI_MARGIN,UI_READER_METADATA_BOTTOM-6,UI_LOCK_WIDTH-2*UI_MARGIN,6};}
static inline EpdRect ui_reader_fullscreen_footer(void){return (EpdRect){0,UI_READER_FULLSCREEN_BOTTOM,UI_LOCK_WIDTH,UI_LOCK_HEIGHT-UI_READER_FULLSCREEN_BOTTOM};}
static inline EpdRect ui_reader_fullscreen_progress(void){return (EpdRect){UI_MARGIN,UI_READER_FULLSCREEN_BOTTOM-4,UI_LOCK_WIDTH-2*UI_MARGIN,4};}
static inline EpdRect ui_reader_footer_for(bool fullscreen,bool immersive){return !fullscreen?(EpdRect){0,UI_READER_STANDARD_BOTTOM,UI_LOCK_WIDTH,UI_LOCK_HEIGHT-UI_READER_STANDARD_BOTTOM}:immersive?ui_reader_fullscreen_footer():ui_reader_footer();}
_Static_assert(UI_READER_METADATA_BOTTOM <= UI_LOCK_HEIGHT,"Reader metadata stays on screen");
_Static_assert(UI_READER_FULLSCREEN_BOTTOM <= UI_LOCK_HEIGHT,"Fullscreen keeps only the release progress strip");

_Static_assert(UI_READER_STANDARD_BOTTOM <= UI_READER_TOOLS_TOP,"Standard metadata fits above the tools");
