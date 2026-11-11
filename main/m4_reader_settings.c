/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "m4_reader_settings.h"

#include "nvs.h"

static uint8_t s_font_size = 48;
static uint8_t s_line_spacing = 130;
static bool s_vertical;
static bool s_hide_images;
static bool s_book_fonts = true;
static bool s_initialized;

void m4ReaderSettingsInit(void) {
    if (s_initialized) return;
    s_initialized = true;
    nvs_handle_t handle;
    if (nvs_open("m4_ui", NVS_READONLY, &handle) != ESP_OK) return;
    uint8_t value = 0;
    if (nvs_get_u8(handle, "rd_size", &value) == ESP_OK && value >= 18 && value <= 90)
        s_font_size = value;
    if (nvs_get_u8(handle, "rd_line", &value) == ESP_OK && value >= 110 && value <= 150)
        s_line_spacing = value;
    if (nvs_get_u8(handle, "rd_vert", &value) == ESP_OK) s_vertical = value != 0;
    if (nvs_get_u8(handle, "rd_hide", &value) == ESP_OK) s_hide_images = value != 0;
    if (nvs_get_u8(handle, "rd_fonts", &value) == ESP_OK) s_book_fonts = value != 0;
    nvs_close(handle);
}

static bool save_u8(const char *key, uint8_t value) {
    nvs_handle_t handle;
    if (nvs_open("m4_ui", NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t error = nvs_set_u8(handle, key, value);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK;
}

uint8_t m4ReaderSettingsFontSize(void) { return s_font_size; }
bool m4ReaderSettingsSetFontSize(uint8_t value) {
    if (value < 18 || value > 90 || !save_u8("rd_size", value)) return false;
    s_font_size = value;
    return true;
}
uint8_t m4ReaderSettingsLineSpacing(void) { return s_line_spacing; }
bool m4ReaderSettingsSetLineSpacing(uint8_t value) {
    if (value < 110 || value > 150 || !save_u8("rd_line", value)) return false;
    s_line_spacing = value;
    return true;
}
bool m4ReaderSettingsVertical(void) { return s_vertical; }
bool m4ReaderSettingsSetVertical(bool value) {
    if (!save_u8("rd_vert", value ? 1 : 0)) return false;
    s_vertical = value;
    return true;
}
bool m4ReaderSettingsHideImages(void) { return s_hide_images; }
bool m4ReaderSettingsSetHideImages(bool value) {
    if (!save_u8("rd_hide", value ? 1 : 0)) return false;
    s_hide_images = value;
    return true;
}
bool m4ReaderSettingsBookFonts(void) { return s_book_fonts; }
bool m4ReaderSettingsSetBookFonts(bool value) {
    if (!save_u8("rd_fonts", value ? 1 : 0)) return false;
    s_book_fonts = value;
    return true;
}
