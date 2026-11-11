/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "settings.h"
#include "ttf_font.h"

#include "nvs.h"

#include <string.h>

static char s_font_path[TTF_FONT_PATH_MAX];
static bool s_loaded;

static void load_font_path(void) {
    if (s_loaded) return;
    s_loaded = true;
    nvs_handle_t handle;
    if (nvs_open("m4_ui", NVS_READONLY, &handle) != ESP_OK) return;
    size_t length = sizeof(s_font_path);
    if (nvs_get_str(handle, "font_path", s_font_path, &length) != ESP_OK) s_font_path[0] = '\0';
    nvs_close(handle);
}

const char *app_settings_font_path(void) {
    load_font_path();
    return s_font_path;
}

const char *app_settings_fonts_dir(void) {
    return "/sdcard/fonts";
}

void app_settings_set_font_path(const char *path) {
    if (!path || strlen(path) >= sizeof(s_font_path)) return;
    nvs_handle_t handle;
    if (nvs_open("m4_ui", NVS_READWRITE, &handle) != ESP_OK) return;
    const esp_err_t err = nvs_set_str(handle, "font_path", path);
    if (err == ESP_OK && nvs_commit(handle) == ESP_OK) {
        memcpy(s_font_path, path, strlen(path) + 1);
        s_loaded = true;
    }
    nvs_close(handle);
}
