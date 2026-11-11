#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool m4ProjectGray4ToGray2(const uint8_t *source, size_t sourceBytes,
                           uint8_t *destination, size_t destinationBytes,
                           uint16_t width, uint16_t height);

bool m4Gray2ToSsd1677Planes(const uint8_t *gray, size_t grayBytes,
                            uint16_t width, uint16_t height,
                            uint8_t *bw, size_t bwBytes, size_t bwStride,
                            uint8_t *lsb, uint8_t *msb, size_t planeBytes);

/* Rotate the logical portrait buffer into the SSD1677 physical landscape
 * buffer. M4 logical (x,y) maps to panel (y, 479-x). */
bool m4Gray2PortraitToPanel(const uint8_t *logical, size_t logicalBytes,
                            uint8_t *panel, size_t panelBytes);
