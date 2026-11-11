#pragma once

// ESP-IDF logging bridge for SDK managers that are shared with Arduino builds.
#include <esp_log.h>

#define LOG_INF(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define LOG_ERR(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define LOG_WRN(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
