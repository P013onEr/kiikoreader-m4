/* SPDX-License-Identifier: Apache-2.0
 * Murphy M4 application display contract.
 *
 * The maintained project renders into its native Gray4 framebuffer. This
 * contract keeps the M4-specific logical canvas and panel packing in one
 * place so an app-loop backend can present frames without knowing SSD1677
 * bit order or rotation details.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "m4_project_ui_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    M4_APP_LOGICAL_WIDTH = M4_PROJECT_UI_WIDTH,
    M4_APP_LOGICAL_HEIGHT = M4_PROJECT_UI_HEIGHT,
    M4_APP_PANEL_WIDTH = 800,
    M4_APP_PANEL_HEIGHT = 480,
};

size_t m4AppGray4Bytes(void);
size_t m4AppLogicalGray2Bytes(void);
size_t m4AppPanelGray2Bytes(void);

/* Convert a native project frame to the logical M4 Gray2 canvas. */
bool m4AppProjectFrameToLogical(const uint8_t *projectGray4, size_t projectBytes,
                                uint8_t *logicalGray2, size_t logicalBytes);

/* Convert a logical M4 Gray2 canvas to SSD1677 physical row-major Gray2. */
bool m4AppLogicalToPanel(const uint8_t *logicalGray2, size_t logicalBytes,
                         uint8_t *panelGray2, size_t panelBytes);

/* Encode a logical Gray2 frame into the SSD1677 BW base and two gray planes.
 * The caller supplies panel-sized buffers; no display I/O is performed here. */
bool m4AppGray2ToSsd1677(const uint8_t *logicalGray2, size_t logicalBytes,
                         uint8_t *bw, size_t bwBytes, size_t bwStride,
                         uint8_t *lsb, uint8_t *msb, size_t planeBytes);

/* Map a logical M4 dirty rectangle to the physical panel rectangle. */
bool m4AppLogicalRectToPanel(int x, int y, int width, int height,
                             int *panelX, int *panelY,
                             int *panelWidth, int *panelHeight);

#ifdef __cplusplus
}
#endif
