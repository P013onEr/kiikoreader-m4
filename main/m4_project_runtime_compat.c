/* M4 has a single full-panel Gray2 refresh path. These compatibility hooks
 * keep the shared app loop linked without claiming unsupported hardware. */
#include "display.h"
#include "ui_nav.h"
#include "display_main_waveform.h"
#include "epdiy.h"
#include "app_transfer_mode.h"
#include "sleep.h"
#include "continuous_du.h"
#include "ota_online.h"
#include "ota_update.h"
#include "factory/pmu_selftest.h"
#include "esp_system.h"

static const EpdWaveform kM4Waveform = {0};

const EpdWaveform *display_main_gray_waveform(void) { return &kM4Waveform; }
const EpdWaveform *display_main_normal_waveform(void) { return &kM4Waveform; }
const EpdWaveform *display_main_bw_waveform(void) { return &kM4Waveform; }
const EpdWaveform *display_settings_scroll_waveform(void) { return &kM4Waveform; }
void display_main_transition_cancel(void) {}
void display_main_transition_disarm(void) {}
void display_main_transition_arm(const void *owner, bool leaving_shelf, bool changing_page) {
    (void)owner; (void)leaving_shelf; (void)changing_page;
}
void display_main_transition_shelf_page(void) {}
void display_main_transition_cover_bands(const EpdRect *bands, unsigned count) {
    (void)bands; (void)count;
}
void display_main_transition_gray_bands(const EpdRect *bands, unsigned count) {
    (void)bands; (void)count;
}

bool continuous_du_init(void) { return false; }
void continuous_du_reset(void) {}
void app_lock_feedback(EpdiyHighlevelState *hl) { (void)hl; }
bool pico_online_busy(void) { return false; }
esp_err_t pico_online_check(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t pico_online_download(void) { return ESP_ERR_NOT_SUPPORTED; }
void pico_online_get_status(pico_update_status_t *status) { if (status) *status = (pico_update_status_t){0}; }
void pico_online_cancel_join(void) {}
bool pico_online_display_begin(void) { return false; }
void pico_online_display_end(bool held) { (void)held; }
esp_err_t pico_online_commit(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t pico_ota_inspect(const char *path, pico_ota_info_t *info) {
    (void)path;
    if (info) *info = (pico_ota_info_t){0};
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t pico_ota_install(const char *path, char *message, size_t message_size) {
    (void)path;
    if (message && message_size) message[0] = '\0';
    return ESP_ERR_NOT_SUPPORTED;
}
void pico_ota_confirm_running(void) {}
void pmu_selftest_prepare_powerdown(void) {}
void app_restart_host(void) { esp_restart(); }
esp_err_t app_sync_time_to_pmu(void) { return ESP_ERR_NOT_SUPPORTED; }
void enter_lock_and_sleep(EpdiyHighlevelState *hl, int64_t *ignore_until_ms,
                          sc7a20h_handle_t acc, cst836u_handle_t touch,
                          bool reader_background) {
    (void)hl; (void)ignore_until_ms; (void)acc; (void)touch; (void)reader_background;
}
