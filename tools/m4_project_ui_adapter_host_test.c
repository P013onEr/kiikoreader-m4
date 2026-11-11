#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "epdiy.h"
#include "m4_app_backend.h"
#include "m4_project_ui_adapter.h"

int main(void) {
    const size_t srcBytes = m4ProjectUiGray4Bytes();
    const size_t dstBytes = m4ProjectUiGray2Bytes();
    unsigned char *src = (unsigned char *)malloc(srcBytes);
    unsigned char *dst = (unsigned char *)malloc(dstBytes);
    assert(src && dst);
    m4ProjectUiClear(src, 15);
    m4ProjectUiScaleGray4ToGray2(src, dst);
    for (size_t i = 0; i < dstBytes; ++i) assert(dst[i] == 0xff);
    m4ProjectUiClear(src, 0);
    m4ProjectUiScaleGray4ToGray2(src, dst);
    for (size_t i = 0; i < dstBytes; ++i) assert(dst[i] == 0);
    /* Common UI ink must stay in a visible, deterministic band: 0x20/0x30
     * remain black instead of being promoted to a pale gray plane. */
    for (unsigned nibble = 0; nibble < 16; ++nibble) {
        m4ProjectUiClear(src, (uint8_t)nibble);
        m4ProjectUiScaleGray4ToGray2(src, dst);
        assert((dst[0] >> 6) == (nibble >> 2));
    }
    m4ProjectUiClear(src, 15);
    /* A small dark source mark must remain local, not darken the entire page. */
    epd_draw_pixel(0, 0, 0x00, src);
    m4ProjectUiScaleGray4ToGray2(src, dst);
    assert((dst[0] >> 6) < 3);
    assert((dst[dstBytes - 1] & 0x03) == 3);
    unsigned char *panel = (unsigned char *)malloc(480u * 800u / 4u);
    assert(panel);
    m4ProjectUiGray4ToPanelGray2(src, panel);
    assert((panel[(479u * 800u) / 4u] >> 6) < 3);
    free(panel);
    /* Project epdiy coordinates map onto the native M4 raster before packing. */
    memset(src, 0xff, srcBytes);
    epd_draw_pixel(0, 0, 0x00, src);
    epd_draw_pixel(4, 0, 0xf0, src);
    assert(epd_get_pixel(0, 479, M4_PROJECT_FB_WIDTH, M4_PROJECT_FB_HEIGHT, src) == 0x00);
    assert(epd_get_pixel(0, 478, M4_PROJECT_FB_WIDTH, M4_PROJECT_FB_HEIGHT, src) == 0xff);
    assert(epd_width() == M4_PROJECT_FB_WIDTH && epd_height() == M4_PROJECT_FB_HEIGHT);
    /* A 2x2 black source block covers the first downsampled pixel's footprint. */
    m4ProjectUiClear(src, 15);
    for (int yy = 0; yy < 2; ++yy)
        for (int xx = 0; xx < 2; ++xx) epd_draw_pixel(xx, yy, 0, src);
    m4ProjectUiScaleGray4ToGray2(src, dst);
    assert((dst[0] >> 6) == 0);
    int x = -1, y = -1;
    m4ProjectUiTouchToNative(479, 799, &x, &y);
    assert(x == 479 && y == 799);
    m4ProjectUiTouchToNative(0, 0, &x, &y);
    assert(x == 0 && y == 0);
    free(dst);
    free(src);

    assert(m4AppGray4Bytes() == M4_PROJECT_FB_WIDTH * M4_PROJECT_FB_HEIGHT / 2u);
    assert(m4AppLogicalGray2Bytes() == 480u * 800u / 4u);
    assert(m4AppPanelGray2Bytes() == 800u * 480u / 4u);

    unsigned char *panelGray2 = (unsigned char *)malloc(m4AppPanelGray2Bytes());
    unsigned char *logicalGray2 = (unsigned char *)malloc(m4AppLogicalGray2Bytes());
    unsigned char *projectGray4 = (unsigned char *)malloc(m4AppGray4Bytes());
    const size_t planeBytes = 800u * 480u / 8u;
    unsigned char *bw = (unsigned char *)malloc(planeBytes);
    unsigned char *lsb = (unsigned char *)malloc(planeBytes);
    unsigned char *msb = (unsigned char *)malloc(planeBytes);
    assert(panelGray2 && logicalGray2 && projectGray4 && bw && lsb && msb);
    m4ProjectUiClear(projectGray4, 15);
    for (int yy = 796; yy < 800; ++yy)
        for (int xx = 476; xx < 480; ++xx)
            epd_draw_pixel(xx, yy, 0, projectGray4);
    assert(m4AppProjectFrameToLogical(projectGray4, m4AppGray4Bytes(),
                                      logicalGray2, m4AppLogicalGray2Bytes()));
    assert(m4AppLogicalToPanel(logicalGray2, m4AppLogicalGray2Bytes(),
                               panelGray2, m4AppPanelGray2Bytes()));
    /* Bottom-right logical edge maps to physical (799,3). */
    assert((panelGray2[((3u * 800u + 799u) / 4u)] & 0x03) < 3);
    assert(!m4AppProjectFrameToLogical(projectGray4, m4AppGray4Bytes() - 1,
                                       logicalGray2, m4AppLogicalGray2Bytes()));

    int px = -1, py = -1, pw = -1, ph = -1;
    assert(m4AppLogicalRectToPanel(10, 20, 30, 40, &px, &py, &pw, &ph));
    assert(px == 20 && py == 440 && pw == 40 && ph == 30);
    assert(m4AppLogicalRectToPanel(-10, -10, 20, 20, &px, &py, &pw, &ph));
    assert(px == 0 && py == 470 && pw == 10 && ph == 10);
    assert(!m4AppLogicalRectToPanel(480, 0, 1, 1, &px, &py, &pw, &ph));
    memset(logicalGray2, 0xff, m4AppLogicalGray2Bytes());
    /* Top-row Gray2 levels 0..3 map onto the panel's last gate row. */
    logicalGray2[0] = 0x1b;
    assert(m4AppGray2ToSsd1677(logicalGray2, m4AppLogicalGray2Bytes(),
                               bw, planeBytes, 100, lsb, msb, planeBytes));
    const size_t row0 = 479u * 100u;
    const size_t row1 = 478u * 100u;
    const size_t row2 = 477u * 100u;
    const size_t row3 = 476u * 100u;
    assert((bw[row0] & 0x80u) == 0); /* level 0 black */
    assert((lsb[row1] & 0x80u) != 0 && (msb[row1] & 0x80u) != 0);
    assert((lsb[row2] & 0x80u) != 0 && (msb[row2] & 0x80u) == 0);
    assert((lsb[row3] & 0x80u) == 0 && (msb[row3] & 0x80u) == 0);
    assert((bw[row3] & 0x80u) != 0); /* level 3 white */
    free(panelGray2);
    free(logicalGray2);
    free(projectGray4);
    free(bw);
    free(lsb);
    free(msb);
    return 0;
}
