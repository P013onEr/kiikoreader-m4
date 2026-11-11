#pragma once
#include <stdbool.h>
static inline bool read_pico_board_is_pico(void) { return false; }
static inline void rp_boot_mark(const char *s) { (void)s; }
