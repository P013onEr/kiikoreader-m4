/* Murphy M4 settings entry: preserve the project tab while exposing only
 * settings backed by this board's real hardware. */
#include "app/app.h"
#include "app/app_registry.h"
#include "m4_display_bridge.h"
#include "m4_epdiy_compat/read_pico_board.h"
#include "settings.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_gesture.h"
#include <string.h>
#include <stdio.h>

enum { SETTINGS_VIEW_ROOT = 0, SETTINGS_VIEW_DISPLAY, SETTINGS_VIEW_STORAGE };
static int s_view;

static void settings_render(app_ctx_t *ctx, uint8_t *fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_draw_header(fb, s_view == SETTINGS_VIEW_ROOT ? "设置" : "设置  /  设备", "Murphy M4");
    if (s_view != SETTINGS_VIEW_ROOT) ui_nav_back(fb, 18, 78);
    if (s_view == SETTINGS_VIEW_ROOT) {
        ui_text(fb, 28, 192, 24, "连接与时间", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 238, 18, "Wi-Fi / 蓝牙 / 日期与时间", EPD_DRAW_ALIGN_LEFT, false);
        ui_hairline(fb, 278, 24, UI_LOCK_WIDTH - 24, UI_GRAY_LIGHT);
        ui_text(fb, 28, 326, 24, "显示", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 372, 18, "SSD1677 · 480 × 800 · 4级灰阶", EPD_DRAW_ALIGN_LEFT, false);
        ui_hairline(fb, 412, 24, UI_LOCK_WIDTH - 24, UI_GRAY_LIGHT);
        ui_text(fb, 28, 460, 24, "存储卡", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 506, 18, "/sdcard", EPD_DRAW_ALIGN_LEFT, false);
    } else if (s_view == SETTINGS_VIEW_DISPLAY) {
        ui_text(fb, 28, 192, 24, "屏幕", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 244, 18, "SSD1677 · 480 × 800 · 4级灰阶", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 326, 24, "刷新模式", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 378, 18, "全屏刷新", EPD_DRAW_ALIGN_LEFT, false);
    } else {
        ui_text(fb, 28, 192, 24, "存储卡", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 244, 18, "/sdcard", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 326, 24, "状态", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 28, 378, 18, "由 FreeInk 存储服务管理", EPD_DRAW_ALIGN_LEFT, false);
    }
    ui_nav_draw(fb, 3);
}

static bool settings_present(app_ctx_t *ctx, app_redraw_t redraw) {
    (void)redraw;
    (void)m4_display_bridge_full(ctx->hl);
    return true;
}

static app_redraw_t settings_gesture(app_ctx_t *ctx, const ui_gesture_event_t *event) {
    if (!ctx || !event || event->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    int tab = ui_nav_hit(event->x0, event->y0);
    if (tab >= 0) {
        ui_nav_request(ctx, tab);
        return APP_REDRAW_NONE;
    }
    if (s_view != SETTINGS_VIEW_ROOT && event->x0 < 110 && event->y0 < 170) {
        s_view = SETTINGS_VIEW_ROOT;
        return APP_REDRAW_PAGE;
    }
    if (s_view == SETTINGS_VIEW_ROOT) {
        if (event->y0 >= 300 && event->y0 < 420) s_view = SETTINGS_VIEW_DISPLAY;
        else if (event->y0 >= 430 && event->y0 < 560) s_view = SETTINGS_VIEW_STORAGE;
        else return APP_REDRAW_NONE;
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}

const app_desc_t app_device_settings = {
    .title = "设置 Settings",
    .detail = "显示、存储与设备",
    .enter_full = true,
    .persistent_navigation = true,
    .binary_ui = false,
    .render = settings_render,
    .present = settings_present,
    .on_gesture = settings_gesture,
};
