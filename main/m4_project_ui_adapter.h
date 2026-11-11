/* SPDX-License-Identifier: Apache-2.0
 * M4 bridge for the maintained project UI framebuffer.
 * Project pages use the M4 logical 480x800 coordinate system; this adapter is
 * the only place that knows the SSD1677 physical target is rotated 800x480.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    M4_PROJECT_LAYOUT_WIDTH = 684,
    M4_PROJECT_LAYOUT_HEIGHT = 1216,
    M4_PROJECT_UI_WIDTH = 480,
    M4_PROJECT_UI_HEIGHT = 800,
    /* Physical SSD1677 framebuffer: portrait UI is rotated at pixel write. */
    M4_PROJECT_FB_WIDTH = 800,
    M4_PROJECT_FB_HEIGHT = 480,
};

size_t m4ProjectUiGray4Bytes(void);
size_t m4ProjectUiGray2Bytes(void);
void m4ProjectUiClear(uint8_t *gray4, uint8_t value);
void m4ProjectUiMapPoint(int x, int y, int *mappedX, int *mappedY);
void m4ProjectUiMapRect(int x, int y, int width, int height, int *mappedX,
                        int *mappedY, int *mappedWidth, int *mappedHeight);

/* Area-filter native project Gray4 into the M4 Gray2 logical canvas. */
void m4ProjectUiScaleGray4ToGray2(const uint8_t *src, uint8_t *dst);

/* Convert native project Gray4 directly to the physical M4 Gray2 layout. */
void m4ProjectUiGray4ToPanelGray2(const uint8_t *src, uint8_t *dst);

/* Clamp touch coordinates to the shared M4 logical canvas. */
void m4ProjectUiTouchToNative(int x, int y, int *nativeX, int *nativeY);

#ifdef __cplusplus
}
#endif
