/* Shared app-page platform boundary.  M4 pages must use this header instead
 * of pulling Pico/S31 display, PMU or waveform components into the target. */
#pragma once

#if KIIKO_BOARD_MURPHY_M4
#include "m4_epdiy_compat/epd_highlevel.h"
#include "m4_epdiy_compat/epdiy.h"
#include "m4_epdiy_compat/e0470_page_turn.h"

/* These functions are implemented by the M4 board facade.  They deliberately
 * use the shared app vocabulary so page layout and interaction remain native. */
#ifdef __cplusplus
extern "C" {
#endif
void m4_project_platform_poll(void);
bool m4_project_platform_storage_ready(void);
bool m4_project_platform_touch_read(void *touch);
int m4_project_platform_battery_percent(void);
bool m4_project_platform_charging(void);
bool m4_project_platform_time_read(int64_t *unix_sec);
#ifdef __cplusplus
}
#endif
#endif
