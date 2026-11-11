/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "m4_reader_settings.h"
#include "nvs.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *key;
    uint8_t value;
    bool present;
} item_t;

static item_t s_items[] = {
    {"rd_size", 64, true}, {"rd_line", 140, true}, {"rd_vert", 1, true},
    {"rd_hide", 1, true}, {"rd_fonts", 0, true}
};
static unsigned s_commits;

esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    assert(!strcmp(name, "m4_ui"));
    assert(mode == NVS_READONLY || mode == NVS_READWRITE);
    *handle = mode;
    return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { (void)handle; }
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *value) {
    (void)handle;
    for (size_t i = 0; i < sizeof(s_items) / sizeof(s_items[0]); ++i) {
        if (!strcmp(s_items[i].key, key) && s_items[i].present) {
            *value = s_items[i].value;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
    assert(handle == NVS_READWRITE);
    for (size_t i = 0; i < sizeof(s_items) / sizeof(s_items[0]); ++i) {
        if (!strcmp(s_items[i].key, key)) {
            s_items[i].value = value;
            s_items[i].present = true;
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}
esp_err_t nvs_commit(nvs_handle_t handle) {
    assert(handle == NVS_READWRITE);
    ++s_commits;
    return ESP_OK;
}

int main(void) {
    m4ReaderSettingsInit();
    assert(m4ReaderSettingsFontSize() == 64);
    assert(m4ReaderSettingsLineSpacing() == 140);
    assert(m4ReaderSettingsVertical());
    assert(m4ReaderSettingsHideImages());
    assert(!m4ReaderSettingsBookFonts());

    assert(!m4ReaderSettingsSetFontSize(17));
    assert(!m4ReaderSettingsSetFontSize(91));
    assert(m4ReaderSettingsSetFontSize(72));
    assert(m4ReaderSettingsFontSize() == 72);
    assert(!m4ReaderSettingsSetLineSpacing(109));
    assert(!m4ReaderSettingsSetLineSpacing(151));
    assert(m4ReaderSettingsSetLineSpacing(120));
    assert(m4ReaderSettingsSetVertical(false));
    assert(!m4ReaderSettingsVertical());
    assert(m4ReaderSettingsSetHideImages(false));
    assert(!m4ReaderSettingsHideImages());
    assert(m4ReaderSettingsSetBookFonts(true));
    assert(m4ReaderSettingsBookFonts());
    assert(s_commits == 5);
    puts("M4 reader settings persistence tests passed");
    return 0;
}
