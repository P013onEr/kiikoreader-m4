/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

void m4ReaderSettingsInit(void);
uint8_t m4ReaderSettingsFontSize(void);
bool m4ReaderSettingsSetFontSize(uint8_t value);
uint8_t m4ReaderSettingsLineSpacing(void);
bool m4ReaderSettingsSetLineSpacing(uint8_t value);
bool m4ReaderSettingsVertical(void);
bool m4ReaderSettingsSetVertical(bool value);
bool m4ReaderSettingsHideImages(void);
bool m4ReaderSettingsSetHideImages(bool value);
bool m4ReaderSettingsBookFonts(void);
bool m4ReaderSettingsSetBookFonts(bool value);
