/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "m4_image_draw.h"

#include <limits.h>

bool m4ProjectFitImage(const uint32_t sourceWidth, const uint32_t sourceHeight,
                       const uint16_t boundsWidth, const uint16_t boundsHeight,
                       uint16_t *imageWidth, uint16_t *imageHeight) {
    if (!sourceWidth || !sourceHeight || !boundsWidth || !boundsHeight ||
        !imageWidth || !imageHeight) return false;
    uint64_t width = boundsWidth;
    uint64_t height = (uint64_t)sourceHeight * boundsWidth / sourceWidth;
    if (!height) height = 1;
    if (height > boundsHeight) {
        height = boundsHeight;
        width = (uint64_t)sourceWidth * boundsHeight / sourceHeight;
        if (!width) width = 1;
    }
    if (width > boundsWidth || height > boundsHeight) return false;
    *imageWidth = (uint16_t)width;
    *imageHeight = (uint16_t)height;
    return true;
}

static uint8_t quantizeGray2(const uint8_t gray, const int32_t x, const int32_t y) {
    (void)x;
    (void)y;
    /* Match the UI and reader paths: fixed four equal luminance bands. */
    uint8_t level = (uint8_t)(gray >> 6);
    return level;
}

static void putGray2(uint8_t *framebuffer, const uint16_t width,
                     const uint16_t x, const uint16_t y, const uint8_t level) {
    const size_t pixel = (size_t)y * width + x;
    uint8_t *packed = &framebuffer[pixel / 2u];
    const uint8_t gray4 = (uint8_t)(level * 5u);
    if ((pixel & 1u) == 0) {
        *packed = (uint8_t)((*packed & 0xf0u) | gray4);
    } else {
        *packed = (uint8_t)((*packed & 0x0fu) | (gray4 << 4));
    }
}

bool m4ProjectBlitGray8ToGray4(uint8_t *framebuffer, const size_t framebufferBytes,
                               const uint16_t framebufferWidth, const uint16_t framebufferHeight,
                               const uint8_t *gray8, const size_t gray8Bytes,
                               const uint16_t imageWidth, const uint16_t imageHeight,
                               const int32_t x, const int32_t y) {
    if (!framebuffer || !gray8 || !framebufferWidth || !framebufferHeight ||
        !imageWidth || !imageHeight || (framebufferWidth & 1u) != 0u ||
        framebufferWidth > SIZE_MAX / framebufferHeight ||
        imageWidth > SIZE_MAX / imageHeight) {
        return false;
    }
    const size_t pixels = (size_t)framebufferWidth * framebufferHeight;
    const size_t imagePixels = (size_t)imageWidth * imageHeight;
    if (framebufferBytes != pixels / 2u || gray8Bytes != imagePixels) return false;

    const int64_t right = (int64_t)x + imageWidth;
    const int64_t bottom = (int64_t)y + imageHeight;
    const int32_t leftClip = x < 0 ? 0 : x;
    const int32_t topClip = y < 0 ? 0 : y;
    const int32_t rightClip = right > framebufferWidth ? framebufferWidth : (int32_t)right;
    const int32_t bottomClip = bottom > framebufferHeight ? framebufferHeight : (int32_t)bottom;
    if (leftClip >= rightClip || topClip >= bottomClip) return true;

    for (int32_t dstY = topClip; dstY < bottomClip; ++dstY) {
        const uint16_t srcY = (uint16_t)(dstY - y);
        for (int32_t dstX = leftClip; dstX < rightClip; ++dstX) {
            const uint16_t srcX = (uint16_t)(dstX - x);
            const uint8_t gray = gray8[(size_t)srcY * imageWidth + srcX];
            putGray2(framebuffer, framebufferWidth, (uint16_t)dstX, (uint16_t)dstY,
                     quantizeGray2(gray, dstX, dstY));
        }
    }
    return true;
}
