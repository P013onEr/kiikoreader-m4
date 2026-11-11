/* SPDX-License-Identifier: Apache-2.0 */
#include "m4_project_ui_adapter.h"
#include "m4_gray2.h"

#include <string.h>

static uint8_t get4(const uint8_t *p, size_t i) {
    const uint8_t b = p[i >> 1];
    return (uint8_t)((b >> ((i & 1u) ? 4 : 0)) & 0x0fu);
}

static void put2(uint8_t *p, size_t i, uint8_t v) {
    const unsigned shift = (unsigned)(6u - ((i & 3u) * 2u));
    p[i >> 2] = (uint8_t)((p[i >> 2] & ~(3u << shift)) | ((v & 3u) << shift));
}

size_t m4ProjectUiGray4Bytes(void) {
    return (size_t)M4_PROJECT_FB_WIDTH * M4_PROJECT_FB_HEIGHT / 2u;
}

size_t m4ProjectUiGray2Bytes(void) {
    return (size_t)M4_PROJECT_UI_WIDTH * M4_PROJECT_UI_HEIGHT / 4u;
}

void m4ProjectUiClear(uint8_t *gray4, uint8_t value) {
    if (!gray4) return;
    memset(gray4, (uint8_t)((value & 0x0fu) * 0x11u), m4ProjectUiGray4Bytes());
}

void m4ProjectUiMapPoint(int x, int y, int *mappedX, int *mappedY) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= M4_PROJECT_LAYOUT_WIDTH) x = M4_PROJECT_LAYOUT_WIDTH - 1;
    if (y >= M4_PROJECT_LAYOUT_HEIGHT) y = M4_PROJECT_LAYOUT_HEIGHT - 1;
    if (mappedX) *mappedX = x * (M4_PROJECT_UI_WIDTH - 1) / (M4_PROJECT_LAYOUT_WIDTH - 1);
    if (mappedY) *mappedY = y * (M4_PROJECT_UI_HEIGHT - 1) / (M4_PROJECT_LAYOUT_HEIGHT - 1);
}

void m4ProjectUiMapRect(int x, int y, int width, int height, int *mappedX,
                        int *mappedY, int *mappedWidth, int *mappedHeight) {
    int64_t x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int64_t x1 = (int64_t)x + width, y1 = (int64_t)y + height;
    if (x1 > M4_PROJECT_LAYOUT_WIDTH) x1 = M4_PROJECT_LAYOUT_WIDTH;
    if (y1 > M4_PROJECT_LAYOUT_HEIGHT) y1 = M4_PROJECT_LAYOUT_HEIGHT;
    if (width <= 0 || height <= 0 || x0 >= x1 || y0 >= y1) {
        if (mappedX) *mappedX = 0;
        if (mappedY) *mappedY = 0;
        if (mappedWidth) *mappedWidth = 0;
        if (mappedHeight) *mappedHeight = 0;
        return;
    }
    const int sx = (int)(x0 * M4_PROJECT_UI_WIDTH / M4_PROJECT_LAYOUT_WIDTH);
    const int sy = (int)(y0 * M4_PROJECT_UI_HEIGHT / M4_PROJECT_LAYOUT_HEIGHT);
    const int ex = (int)((x1 * M4_PROJECT_UI_WIDTH + M4_PROJECT_LAYOUT_WIDTH - 1) / M4_PROJECT_LAYOUT_WIDTH);
    const int ey = (int)((y1 * M4_PROJECT_UI_HEIGHT + M4_PROJECT_LAYOUT_HEIGHT - 1) / M4_PROJECT_LAYOUT_HEIGHT);
    if (mappedX) *mappedX = sx;
    if (mappedY) *mappedY = sy;
    if (mappedWidth) *mappedWidth = ex - sx;
    if (mappedHeight) *mappedHeight = ey - sy;
}

void m4ProjectUiScaleGray4ToGray2(const uint8_t *src, uint8_t *dst) {
    if (!src || !dst) return;
    memset(dst, 0xff, m4ProjectUiGray2Bytes());
    /* The project framebuffer is physically packed (800x480), while all
     * callers draw in the native portrait 480x800 coordinate space. Undo the
     * one quarter-turn here; never resample the UI. */
    for (int y = 0; y < M4_PROJECT_UI_HEIGHT; ++y)
        for (int x = 0; x < M4_PROJECT_UI_WIDTH; ++x) {
            const int px = y;
            const int py = M4_PROJECT_UI_WIDTH - 1 - x;
            const uint8_t gray = get4(src, (size_t)py * M4_PROJECT_FB_WIDTH + px);
            /* Use stable four equal bands. The previous rounded scale sent
             * common UI ink (0x30) to a gray plane, where a thin
             * glyph or rule can disappear on the SSD1677. */
            put2(dst, (size_t)y * M4_PROJECT_UI_WIDTH + x,
                  (uint8_t)(gray >> 2));
        }
}

void m4ProjectUiGray4ToPanelGray2(const uint8_t *src, uint8_t *dst) {
    if (!src || !dst) return;
    uint8_t logical[M4_PROJECT_UI_WIDTH * M4_PROJECT_UI_HEIGHT / 4u];
    m4ProjectUiScaleGray4ToGray2(src, logical);
    m4Gray2PortraitToPanel(logical, sizeof(logical), dst,
                           (size_t)M4_PROJECT_FB_WIDTH * M4_PROJECT_FB_HEIGHT / 4u);
}

void m4ProjectUiTouchToNative(int x, int y, int *nativeX, int *nativeY) {
    if (nativeX) *nativeX = x < 0 ? 0 : x >= M4_PROJECT_UI_WIDTH ? M4_PROJECT_UI_WIDTH - 1 : x;
    if (nativeY) *nativeY = y < 0 ? 0 : y >= M4_PROJECT_UI_HEIGHT ? M4_PROJECT_UI_HEIGHT - 1 : y;
}
