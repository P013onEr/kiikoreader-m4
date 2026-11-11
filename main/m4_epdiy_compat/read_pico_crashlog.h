#pragma once
#include <stdint.h>
#include <stdarg.h>
static inline void rp_crash_log(const char *fmt, ...) { (void)fmt; }
static inline uint8_t rp_crash_log_count(void) { return 0; }
static inline const char *rp_crash_log_line(uint8_t index) { (void)index; return 0; }
static inline uint32_t rp_crash_log_serial(void) { return 0; }
static inline void rp_crash_log_clear(void) {}
