#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "epd_highlevel.h"
#include "cst836u.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bind the already-initialized FreeInk panel and project framebuffer. */
void m4_display_bridge_bind(void *freeink_display, EpdiyHighlevelState *state);
/* Allocate/bind the shared project's native M4 480x800 Gray4 canvas, packed
 * in the rotated 800x480 panel framebuffer layout. */
uint8_t *m4_display_bridge_framebuffer(EpdiyHighlevelState *state);
void m4_input_bridge_bind(void *freeink_input);
bool m4_input_bridge_read(cst836u_touch_t *touch);

/* Present the shared project's Gray4 framebuffer on the M4 SSD1677. */
enum EpdDrawError m4_display_bridge_full(EpdiyHighlevelState *state);
enum EpdDrawError m4_display_bridge_area(EpdiyHighlevelState *state, EpdRect area);

/* Input bridge state is supplied by the board's single FreeInk poller. */
void m4_input_bridge_reset(void);
void m4_input_bridge_set_touch(bool pressed, bool released, int x, int y);
void m4_input_bridge_set_keys(uint32_t pressed_mask, uint32_t released_mask);
bool m4_input_bridge_take_touch(bool *pressed, bool *released, int *x, int *y);
uint32_t m4_input_bridge_take_pressed(void);
uint32_t m4_input_bridge_take_released(void);

#ifdef __cplusplus
}
#endif
