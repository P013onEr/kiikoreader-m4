#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    bool present;
    bool mounted;
    bool needs_format;
    char name[8];
    uint64_t capacity_bytes;
    uint64_t filesystem_bytes;
    bool space_valid;
    esp_err_t space_error;
    esp_err_t error;
    uint64_t total_bytes;
    uint64_t free_bytes;
} read_pico_sd_info_t;
void m4_project_sd_set_mounted(bool mounted);
esp_err_t read_pico_sd_get_info(read_pico_sd_info_t *info);
esp_err_t read_pico_sd_start_probe(void);
esp_err_t read_pico_sd_start_space_refresh(void);
esp_err_t read_pico_sd_refresh_space(void);
void read_pico_sd_poll_health(void);
bool read_pico_sd_needs_recovery(void);
esp_err_t read_pico_sd_sync(void);
esp_err_t read_pico_sd_remount(void);
esp_err_t read_pico_sd_start_recovery(void);
esp_err_t read_pico_sd_format(void);
bool read_pico_sd_io_ready(void);
