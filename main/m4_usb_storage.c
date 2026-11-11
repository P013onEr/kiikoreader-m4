#include "apps/usb_storage.h"
esp_err_t usb_storage_start(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t usb_storage_stop(void) { return ESP_OK; }
bool usb_storage_active(void) { return false; }
bool usb_storage_connected(void) { return false; }
void usb_storage_phy_init(void) {}
