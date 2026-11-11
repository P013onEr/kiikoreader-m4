/* M4 refresh facade for the shared application pages. */
#pragma once

#include "epd_highlevel.h"

typedef enum {
    E0470_TURN_LTR = 0,
    E0470_TURN_RTL = 1,
    E0470_TURN_TTB = 2,
    E0470_TURN_BTT = 3,
} e0470_turn_dir_t;

enum EpdDrawError e0470_page_turn(EpdiyHighlevelState *, EpdRect,
                                  e0470_turn_dir_t);
enum EpdDrawError e0470_page_turn_with_waveform(EpdiyHighlevelState *, EpdRect,
                                                e0470_turn_dir_t,
                                                const EpdWaveform *, unsigned);
static inline unsigned e0470_page_turn_tick_us(void) { return 1000; }
static inline void e0470_page_turn_set_tick_us(unsigned v) { (void)v; }
static inline void e0470_page_turn_release(void) {}
#define E0470_TURN_FAST_TICK_US 1000u
