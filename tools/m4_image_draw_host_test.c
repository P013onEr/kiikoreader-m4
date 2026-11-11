#include "m4_image_draw.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t gray4_at(const uint8_t *framebuffer, const unsigned width,
                        const unsigned x, const unsigned y) {
    const size_t pixel = (size_t)y * width + x;
    const uint8_t packed = framebuffer[pixel / 2u];
    return (uint8_t)((pixel & 1u) ? packed >> 4 : packed & 0x0fu);
}

int main(void) {
    uint16_t fittedWidth = 0, fittedHeight = 0;
    assert(m4ProjectFitImage(1600, 900, 480, 800, &fittedWidth, &fittedHeight));
    assert(fittedWidth == 480 && fittedHeight == 270);
    assert(m4ProjectFitImage(900, 1600, 480, 800, &fittedWidth, &fittedHeight));
    assert(fittedWidth == 450 && fittedHeight == 800);
    assert(m4ProjectFitImage(1, UINT32_MAX, 480, 800, &fittedWidth, &fittedHeight));
    assert(fittedWidth == 1 && fittedHeight == 800);
    assert(!m4ProjectFitImage(0, 10, 480, 800, &fittedWidth, &fittedHeight));

    uint8_t framebuffer[16];
    memset(framebuffer, 0xff, sizeof(framebuffer));
    const uint8_t source[] = {0, 85, 170, 255};
    assert(m4ProjectBlitGray8ToGray4(framebuffer, sizeof(framebuffer), 8, 4,
                                     source, sizeof(source), 4, 1, 2, 1));
    assert(gray4_at(framebuffer, 8, 2, 1) == 0);
    assert(gray4_at(framebuffer, 8, 3, 1) == 5);
    assert(gray4_at(framebuffer, 8, 4, 1) == 10);
    assert(gray4_at(framebuffer, 8, 5, 1) == 15);
    assert(gray4_at(framebuffer, 8, 0, 0) == 15);

    memset(framebuffer, 0xff, sizeof(framebuffer));
    const uint8_t clipped[] = {0, 255, 255, 0};
    assert(m4ProjectBlitGray8ToGray4(framebuffer, sizeof(framebuffer), 8, 4,
                                     clipped, sizeof(clipped), 2, 2, -1, 2));
    assert(gray4_at(framebuffer, 8, 0, 2) == 15);
    assert(gray4_at(framebuffer, 8, 0, 3) == 0);
    assert(gray4_at(framebuffer, 8, 1, 2) == 15);

    uint8_t gradient[16];
    for (unsigned i = 0; i < sizeof(gradient); ++i) gradient[i] = 128;
    memset(framebuffer, 0xff, sizeof(framebuffer));
    assert(m4ProjectBlitGray8ToGray4(framebuffer, sizeof(framebuffer), 8, 4,
                                     gradient, sizeof(gradient), 4, 4, 0, 0));
    for (unsigned y = 0; y < 4; ++y) {
        for (unsigned x = 0; x < 4; ++x) {
            const uint8_t tone = gray4_at(framebuffer, 8, x, y);
            assert(tone == 10);
        }
    }
    assert(!m4ProjectBlitGray8ToGray4(framebuffer, sizeof(framebuffer), 8, 4,
                                      source, sizeof(source) - 1u, 4, 1, 0, 0));
    assert(!m4ProjectBlitGray8ToGray4(framebuffer, sizeof(framebuffer), 7, 4,
                                      source, sizeof(source), 4, 1, 0, 0));
    puts("M4 Gray8 image blit tests passed");
    return 0;
}
