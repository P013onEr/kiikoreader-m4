/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "m4_gray2.h"

#include <string.h>

bool m4ProjectGray4ToGray2(const uint8_t *source, const size_t sourceBytes,
                           uint8_t *destination, const size_t destinationBytes,
                           const uint16_t width, const uint16_t height) {
    if (!source || !destination || !width || !height || (width & 1u) || (width & 3u)) return false;
    const size_t pixels = (size_t)width * height;
    const size_t expectedSourceBytes = pixels / 2u;
    const size_t expectedDestinationBytes = pixels / 4u;
    if (sourceBytes != expectedSourceBytes || destinationBytes != expectedDestinationBytes) return false;
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
        const uint8_t packed = source[pixel / 2u];
        const uint8_t gray4 = (uint8_t)((pixel & 1u) ? packed >> 4 : packed & 0x0fu);
            /* Equal four bands keep dark UI ink on the black rail. */
            const uint8_t level = (uint8_t)(gray4 >> 2);
        const size_t output = pixel / 4u;
        const uint8_t shift = (uint8_t)(6u - (pixel & 3u) * 2u);
        if ((pixel & 3u) == 0) destination[output] = 0;
        destination[output] |= (uint8_t)(level << shift);
    }
    return true;
}

bool m4Gray2ToSsd1677Planes(const uint8_t *gray, const size_t grayBytes,
                            const uint16_t width, const uint16_t height,
                            uint8_t *bw, const size_t bwBytes,
                            const size_t bwStride, uint8_t *lsb,
                            uint8_t *msb, const size_t planeBytes) {
    if (!gray || !bw || !lsb || !msb || !width || !height ||
        (width & 3u) || (width & 7u) || bwStride < width / 8u) return false;

    const size_t pixels = (size_t)width * height;
    const size_t expectedGrayBytes = pixels / 4u;
    const size_t expectedPlaneBytes = (size_t)(width / 8u) * height;
    const size_t expectedBwBytes = bwStride * height;
    if (grayBytes != expectedGrayBytes || planeBytes != expectedPlaneBytes ||
        bwBytes != expectedBwBytes) return false;

    memset(bw, 0xff, bwBytes);
    memset(lsb, 0, planeBytes);
    memset(msb, 0, planeBytes);
    for (uint16_t y = 0; y < height; ++y) {
        const size_t rowPixel = (size_t)y * width;
        const size_t rowBw = (size_t)y * bwStride;
        const size_t rowPlane = (size_t)y * (width / 8u);
        for (uint16_t x = 0; x < width; ++x) {
            const size_t pixel = rowPixel + x;
            const uint8_t packed = gray[pixel / 4u];
            const uint8_t shift = (uint8_t)(6u - (pixel & 3u) * 2u);
            const uint8_t level = (uint8_t)((packed >> shift) & 3u);
            const uint8_t mask = (uint8_t)(0x80u >> (x & 7u));
            const size_t byte = rowPlane + x / 8u;

            // The overlay planes encode dark=11 and light=01; black/white
            // remain distinguished by the binary base framebuffer.
            if (level == 0u) bw[rowBw + x / 8u] &= (uint8_t)~mask;
            if (level == 1u || level == 2u) lsb[byte] |= mask;
            if (level == 1u) msb[byte] |= mask;
        }
    }
    return true;
}

bool m4Gray2PortraitToPanel(const uint8_t *logical, const size_t logicalBytes,
                            uint8_t *panel, const size_t panelBytes) {
    const size_t logicalExpected = (size_t)480u * 800u / 4u;
    const size_t panelExpected = (size_t)800u * 480u / 4u;
    if (!logical || !panel || logicalBytes != logicalExpected || panelBytes != panelExpected)
        return false;
    memset(panel, 0xff, panelBytes);
    for (uint16_t y = 0; y < 800; ++y) {
        for (uint16_t x = 0; x < 480; ++x) {
            const size_t srcPixel = (size_t)y * 480u + x;
            const uint8_t src = logical[srcPixel >> 2];
            const uint8_t level = (uint8_t)((src >> (6u - (srcPixel & 3u) * 2u)) & 3u);
            const uint16_t px = y;
            const uint16_t py = (uint16_t)(479u - x);
            const size_t dstPixel = (size_t)py * 800u + px;
            panel[dstPixel >> 2] = (uint8_t)((panel[dstPixel >> 2] &
                ~(3u << (6u - (dstPixel & 3u) * 2u))) |
                (uint8_t)(level << (6u - (dstPixel & 3u) * 2u)));
        }
    }
    return true;
}
