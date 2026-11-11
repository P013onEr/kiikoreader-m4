/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：按用户确认预览统一管理页列表/图标、搜索、排序、选择和底栏的绘制/命中。
 * English: Shared management list/grid, search, order, selection and footer paint/hit geometry approved by the user.
 */
#pragma once
#include <stdbool.h>
#include "epdiy.h"
#include "ui_nav_layout.h"
static inline int ui_book_manage_rows(bool grid){return UI_LOCK_WIDTH==480?(grid?4:5):(grid?6:5);}
static inline EpdRect ui_book_manage_row(int i){return UI_LOCK_WIDTH==480?(EpdRect){UI_MARGIN,258+i*68,ui_content_width(),62}:(EpdRect){UI_MARGIN,250+i*86,ui_content_width(),86};}
static inline EpdRect ui_book_manage_entry(bool grid,int i){
    if (!grid) return ui_book_manage_row(i);
    if(UI_LOCK_WIDTH==480)return(EpdRect){UI_MARGIN+(i%2)*222,258+(i/2)*176,210,168};
    const int cols=UI_LOCK_WIDTH==480?2:3;
    const int gap=UI_LOCK_WIDTH==480?12:UI_GAP;
    int w=(ui_content_width()-(cols-1)*gap)/cols;
    return (EpdRect){UI_MARGIN+(i%cols)*(w+gap),UI_LOCK_WIDTH==480?244+(i/cols)*166:260+(i/3)*220,w,UI_LOCK_WIDTH==480?154:210};
}
static inline EpdRect ui_book_manage_cover(bool grid,int i){
    EpdRect r=ui_book_manage_entry(grid,i);
    if(UI_LOCK_WIDTH==480&&grid)return(EpdRect){r.x+(r.width-86)/2,r.y+6,86,112};
    if(UI_LOCK_WIDTH==480)return(EpdRect){r.x+8,r.y+4,42,54};
    return grid?(EpdRect){r.x+8,r.y+8,r.width-16,r.height-(UI_LOCK_WIDTH==480?36:38)}:(EpdRect){r.x+8,r.y+8,64,70};
}
static inline EpdRect ui_book_manage_search(void){return (EpdRect){UI_MARGIN,120,ui_content_width(),48};}
static inline EpdRect ui_book_manage_layout(void){return (EpdRect){UI_LOCK_WIDTH-72,70,56,44};}
static inline EpdRect ui_book_manage_sort(void){return (EpdRect){UI_MARGIN,182,ui_content_width()/2-UI_GAP,42};}
static inline EpdRect ui_book_manage_select(void){return (EpdRect){UI_LOCK_WIDTH/2+UI_GAP,182,ui_content_width()/2-UI_GAP,42};}
static inline EpdRect ui_book_manage_action(int i){
    int action=i-3,x=action*UI_LOCK_WIDTH/4,end=(action+1)*UI_LOCK_WIDTH/4;
    return action>=0&&action<4?(EpdRect){x,UI_NAV_TOP,end-x,UI_LOCK_HEIGHT-UI_NAV_TOP}:(EpdRect){0};
}
#if KIIKO_BOARD_MURPHY_M4
_Static_assert(258+4*68+62<UI_PAGER_TOP,"M4 management list finishes before pager");
_Static_assert(258+176+168<UI_PAGER_TOP,"M4 management grid finishes before pager");
#else
_Static_assert(310+5*130<1006,"List must finish before pager");
_Static_assert(322+330+314<1006,"Grid must finish before pager");
_Static_assert(1006+50<1096-8,"Pager leaves footer guard");
_Static_assert(1096+120==1216,"Four actions share native navigation geometry");
#endif
