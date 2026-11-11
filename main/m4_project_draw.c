/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "epdiy.h"
#include "m4_project_ui_adapter.h"

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

int epd_width(void) { return 800; }
int epd_height(void) { return 480; }
int epd_rotated_display_width(void) { return M4_PROJECT_UI_WIDTH; }
int epd_rotated_display_height(void) { return M4_PROJECT_UI_HEIGHT; }
enum EpdRotation epd_get_rotation(void) { return EPD_ROT_INVERTED_PORTRAIT; }

void epd_draw_pixel(int x, int y, uint8_t color, uint8_t *framebuffer) {
    if (x < 0 || y < 0 || x >= M4_PROJECT_UI_WIDTH || y >= M4_PROJECT_UI_HEIGHT || framebuffer == 0) return;
    const int px = y;
    const int py = 479 - x;
    uint8_t *pixel = framebuffer + (size_t)py * 400u + (size_t)px / 2u;
    if (px & 1) {
        *pixel = (uint8_t)((*pixel & 0x0fu) | (color & 0xf0u));
    } else {
        *pixel = (uint8_t)((*pixel & 0xf0u) | (color >> 4));
    }
}

uint8_t epd_get_pixel(int x, int y, int fb_width, int fb_height, const uint8_t *framebuffer) {
    if (!framebuffer || x < 0 || y < 0 || x >= fb_width || y >= fb_height) return 0xff;
    const size_t offset = (size_t)y * (size_t)fb_width / 2u + (size_t)x / 2u;
    const uint8_t packed = framebuffer[offset];
    const uint8_t nibble = (uint8_t)((x & 1) ? (packed >> 4) : (packed & 0x0fu));
    return (uint8_t)(nibble * 0x11u);
}

void epd_fill_rect(EpdRect rect, uint8_t color, uint8_t *framebuffer) {
    if (framebuffer == 0 || rect.width <= 0 || rect.height <= 0) return;
    int left = rect.x < 0 ? 0 : rect.x;
    int top = rect.y < 0 ? 0 : rect.y;
    int right = rect.x + rect.width;
    int bottom = rect.y + rect.height;
    if (right > M4_PROJECT_UI_WIDTH) right = M4_PROJECT_UI_WIDTH;
    if (bottom > M4_PROJECT_UI_HEIGHT) bottom = M4_PROJECT_UI_HEIGHT;
    if (right <= left || bottom <= top) return;
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            epd_draw_pixel(x, y, color, framebuffer);
}

void epd_draw_line(int x0, int y0, int x1, int y1, uint8_t color, uint8_t *fb) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        epd_draw_pixel(x0, y0, color, fb);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void epd_draw_circle(int x0, int y0, int r, uint8_t color, uint8_t *fb) {
    int f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    epd_draw_pixel(x0, y0 + r, color, fb); epd_draw_pixel(x0, y0 - r, color, fb);
    epd_draw_pixel(x0 + r, y0, color, fb); epd_draw_pixel(x0 - r, y0, color, fb);
    while (x < y) {
        if (f >= 0) { --y; ddy += 2; f += ddy; }
        ++x; ddx += 2; f += ddx;
        epd_draw_pixel(x0+x,y0+y,color,fb); epd_draw_pixel(x0-x,y0+y,color,fb);
        epd_draw_pixel(x0+x,y0-y,color,fb); epd_draw_pixel(x0-x,y0-y,color,fb);
        epd_draw_pixel(x0+y,y0+x,color,fb); epd_draw_pixel(x0-y,y0+x,color,fb);
        epd_draw_pixel(x0+y,y0-x,color,fb); epd_draw_pixel(x0-y,y0-x,color,fb);
    }
}

void epd_fill_circle_helper(int x0, int y0, int r, int corners, int delta, uint8_t color, uint8_t *fb) {
    for (int y = -r; y <= r; ++y) {
        int span = (int)sqrt((double)r * r - (double)y * y);
        if ((corners & 1) && y <= 0) epd_fill_rect((EpdRect){x0, y0+y, span+1, 1}, color, fb);
        if ((corners & 2) && y <= 0) epd_fill_rect((EpdRect){x0-span, y0+y, span+1, 1}, color, fb);
        (void)delta;
    }
}

void epd_fill_circle(int x0, int y0, int r, uint8_t color, uint8_t *fb) {
    for (int y = -r; y <= r; ++y) {
        int span = (int)sqrt((double)r * r - (double)y * y);
        epd_fill_rect((EpdRect){x0-span, y0+y, span*2+1, 1}, color, fb);
    }
}

void epd_draw_rotated_image(EpdRect area, const uint8_t *image, uint8_t *fb) {
    if (!image || area.width <= 0 || area.height <= 0) return;
    for (int y = 0; y < area.height; ++y)
        for (int x = 0; x < area.width; ++x)
            epd_draw_pixel(area.x+x, area.y+y,
                           epd_get_pixel(x, y, area.width, area.height, image), fb);
}
