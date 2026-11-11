/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    int width;
    int height;
} EpdRect;

enum EpdRotation {
    EPD_ROT_LANDSCAPE = 0,
    EPD_ROT_PORTRAIT = 1,
    EPD_ROT_INVERTED_LANDSCAPE = 2,
    EPD_ROT_INVERTED_PORTRAIT = 3,
};

enum EpdFontFlags {
    EPD_DRAW_ALIGN_LEFT = 0x2,
    EPD_DRAW_ALIGN_RIGHT = 0x4,
    EPD_DRAW_ALIGN_CENTER = 0x8,
};

void epd_draw_pixel(int x, int y, uint8_t color, uint8_t *framebuffer);
uint8_t epd_get_pixel(int x, int y, int fb_width, int fb_height, const uint8_t *framebuffer);
void epd_draw_line(int x0, int y0, int x1, int y1, uint8_t color, uint8_t *framebuffer);
void epd_draw_circle(int x, int y, int radius, uint8_t color, uint8_t *framebuffer);
void epd_fill_circle(int x, int y, int radius, uint8_t color, uint8_t *framebuffer);
void epd_fill_circle_helper(int x, int y, int radius, int corners, int delta,
                            uint8_t color, uint8_t *framebuffer);
void epd_draw_rotated_image(EpdRect area, const uint8_t *image, uint8_t *framebuffer);
void epd_fill_rect(EpdRect rect, uint8_t color, uint8_t *framebuffer);
int epd_width(void);
int epd_height(void);
int epd_rotated_display_width(void);
int epd_rotated_display_height(void);
enum EpdRotation epd_get_rotation(void);

#ifdef __cplusplus
}
#endif
