/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool m4ProjectBlitGray8ToGray4(uint8_t *framebuffer, size_t framebufferBytes,
                               uint16_t framebufferWidth, uint16_t framebufferHeight,
                               const uint8_t *gray8, size_t gray8Bytes,
                               uint16_t imageWidth, uint16_t imageHeight,
                               int32_t x, int32_t y);
bool m4ProjectFitImage(uint32_t sourceWidth, uint32_t sourceHeight,
                       uint16_t boundsWidth, uint16_t boundsHeight,
                       uint16_t *imageWidth, uint16_t *imageHeight);
