/* SPDX-License-Identifier: Apache-2.0 */
#include "m4_app_backend.h"

#include "m4_gray2.h"
#include <string.h>

size_t m4AppGray4Bytes(void) {
    return (size_t)M4_PROJECT_FB_WIDTH * M4_PROJECT_FB_HEIGHT / 2u;
}

size_t m4AppLogicalGray2Bytes(void) {
    return (size_t)M4_APP_LOGICAL_WIDTH * M4_APP_LOGICAL_HEIGHT / 4u;
}

size_t m4AppPanelGray2Bytes(void) {
    return (size_t)M4_APP_PANEL_WIDTH * M4_APP_PANEL_HEIGHT / 4u;
}

bool m4AppProjectFrameToLogical(const uint8_t *projectGray4, size_t projectBytes,
                                uint8_t *logicalGray2, size_t logicalBytes) {
    if (!projectGray4 || !logicalGray2 || projectBytes != m4AppGray4Bytes() ||
        logicalBytes != m4AppLogicalGray2Bytes()) return false;
    m4ProjectUiScaleGray4ToGray2(projectGray4, logicalGray2);
    return true;
}

bool m4AppLogicalToPanel(const uint8_t *logicalGray2, size_t logicalBytes,
                         uint8_t *panelGray2, size_t panelBytes) {
    return m4Gray2PortraitToPanel(logicalGray2, logicalBytes, panelGray2, panelBytes);
}

bool m4AppGray2ToSsd1677(const uint8_t *logicalGray2, size_t logicalBytes,
                         uint8_t *bw, size_t bwBytes, size_t bwStride,
                         uint8_t *lsb, uint8_t *msb, size_t planeBytes) {
    const size_t rowBytes = M4_APP_PANEL_WIDTH / 8u;
    if (!logicalGray2 || logicalBytes != m4AppLogicalGray2Bytes() ||
        !bw || !lsb || !msb || bwStride < rowBytes ||
        bwBytes != bwStride * M4_APP_PANEL_HEIGHT ||
        planeBytes != rowBytes * M4_APP_PANEL_HEIGHT) return false;
    memset(bw, 0xff, bwBytes);
    memset(lsb, 0, planeBytes);
    memset(msb, 0, planeBytes);
    for (uint16_t y = 0; y < M4_APP_LOGICAL_HEIGHT; ++y) {
        for (uint16_t x = 0; x < M4_APP_LOGICAL_WIDTH; ++x) {
            const size_t srcPixel = (size_t)y * M4_APP_LOGICAL_WIDTH + x;
            const uint8_t shift = (uint8_t)(6u - (srcPixel & 3u) * 2u);
            const uint8_t level = (uint8_t)((logicalGray2[srcPixel / 4u] >> shift) & 3u);
            const uint16_t panelX = y;
            const uint16_t panelY = (uint16_t)(M4_APP_LOGICAL_WIDTH - 1u - x);
            const size_t panelOffset = (size_t)panelY * bwStride + panelX / 8u;
            const uint8_t mask = (uint8_t)(0x80u >> (panelX & 7u));
            const size_t planeOffset = (size_t)panelY * rowBytes + panelX / 8u;
            if (level == 0u) bw[panelOffset] &= (uint8_t)~mask;
            if (level == 1u || level == 2u) lsb[planeOffset] |= mask;
            if (level == 1u) msb[planeOffset] |= mask;
        }
    }
    return true;
}

bool m4AppLogicalRectToPanel(int x, int y, int width, int height,
                             int *panelX, int *panelY,
                             int *panelWidth, int *panelHeight) {
    if (!panelX || !panelY || !panelWidth || !panelHeight || width <= 0 || height <= 0)
        return false;
    const int left = x < 0 ? 0 : x;
    const int top = y < 0 ? 0 : y;
    const int right = x + width > M4_APP_LOGICAL_WIDTH ? M4_APP_LOGICAL_WIDTH : x + width;
    const int bottom = y + height > M4_APP_LOGICAL_HEIGHT ? M4_APP_LOGICAL_HEIGHT : y + height;
    if (left >= right || top >= bottom) return false;
    /* Logical (x,y) -> panel (y, 479-x); expand the half-open rectangle. */
    *panelX = top;
    *panelY = M4_APP_LOGICAL_WIDTH - right;
    *panelWidth = bottom - top;
    *panelHeight = right - left;
    return true;
}
