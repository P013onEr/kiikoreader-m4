/* M4 storage status adapter for shared pages; book roots remain in settings.c. */
#include "read_pico_sd.h"

#include <string.h>

static bool s_sd_mounted;

void m4_project_sd_set_mounted(bool mounted) {
    s_sd_mounted = mounted;
}

esp_err_t read_pico_sd_get_info(read_pico_sd_info_t *info) {
    if (!info) return ESP_ERR_INVALID_ARG;
    memset(info, 0, sizeof(*info));
    info->present = s_sd_mounted;
    info->mounted = s_sd_mounted;
    info->space_error = ESP_ERR_NOT_SUPPORTED;
    info->error = s_sd_mounted ? ESP_OK : ESP_ERR_NOT_FOUND;
    memcpy(info->name, "TF", sizeof("TF"));
    return s_sd_mounted ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t read_pico_sd_start_probe(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t read_pico_sd_start_space_refresh(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t read_pico_sd_refresh_space(void) { return ESP_ERR_NOT_SUPPORTED; }
void read_pico_sd_poll_health(void) {}
bool read_pico_sd_needs_recovery(void) { return false; }
esp_err_t read_pico_sd_sync(void) { return s_sd_mounted ? ESP_OK : ESP_ERR_INVALID_STATE; }
esp_err_t read_pico_sd_remount(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t read_pico_sd_start_recovery(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t read_pico_sd_format(void) { return ESP_ERR_NOT_SUPPORTED; }
bool read_pico_sd_io_ready(void) { return s_sd_mounted; }
