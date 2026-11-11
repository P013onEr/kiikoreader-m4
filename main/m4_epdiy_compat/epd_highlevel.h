/* M4 compatibility contract for the shared project app pages. */
#pragma once

#include <stdint.h>
#include "epdiy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *framebuffer;
    int width;
    int height;
} EpdiyHighlevelState;

typedef struct { const uint8_t *phases; unsigned phase_count; } EpdWaveform;
/* Shared enum contract; M4 maps these values to SSD1677 waveforms locally. */
enum EpdDrawMode {
    MODE_DU = 0x1,
    MODE_GC16 = 0x2,
    MODE_GL16 = 0x5,
};

enum EpdDrawError {
    EPD_DRAW_SUCCESS = 0,
    EPD_DRAW_ERROR = -1,
};

EpdRect epd_full_screen(void);
void guard_draw_result(EpdiyHighlevelState *hl, enum EpdDrawError result);
static inline void epd_poweron(void) {}
static inline void epd_poweroff(void) {}
enum EpdDrawError update_display_full(EpdiyHighlevelState *hl);
enum EpdDrawError update_display_white(EpdiyHighlevelState *hl);

#ifdef __cplusplus
}
#endif
