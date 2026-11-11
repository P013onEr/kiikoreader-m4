#include <assert.h>
#include <string.h>

#include "read_pico_sd.h"

int main(void) {
    read_pico_sd_info_t info;
    memset(&info, 0xa5, sizeof(info));

    m4_project_sd_set_mounted(false);
    assert(read_pico_sd_get_info(&info) == ESP_ERR_NOT_FOUND);
    assert(!info.present && !info.mounted && !info.space_valid);
    assert(info.space_error == ESP_ERR_NOT_SUPPORTED);
    assert(!read_pico_sd_io_ready());
    assert(read_pico_sd_sync() == ESP_ERR_INVALID_STATE);

    m4_project_sd_set_mounted(true);
    assert(read_pico_sd_get_info(&info) == ESP_OK);
    assert(info.present && info.mounted && !info.space_valid);
    assert(!info.needs_format && !strcmp(info.name, "TF"));
    assert(read_pico_sd_io_ready());
    assert(read_pico_sd_sync() == ESP_OK);
    assert(read_pico_sd_get_info(NULL) == ESP_ERR_INVALID_ARG);
    assert(read_pico_sd_start_probe() == ESP_ERR_NOT_SUPPORTED);
    assert(read_pico_sd_start_recovery() == ESP_ERR_NOT_SUPPORTED);
    assert(read_pico_sd_format() == ESP_ERR_NOT_SUPPORTED);
    return 0;
}
