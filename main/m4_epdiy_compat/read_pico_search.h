#pragma once
#include <stdbool.h>
#include <string.h>
static inline bool read_pico_search_match(const char *text, const char *query) {
    if (!query || !*query) return true;
    if (!text) return false;
    return strstr(text, query) != NULL;
}
static inline const char *read_pico_search_syllable(size_t index) { (void)index; return index ? NULL : "a"; }
static inline size_t read_pico_search_candidates(const char *s, uint32_t *out, size_t cap, size_t skip) { (void)s;(void)skip; if(out&&cap) out[0]=0x4e00; return cap?1:0; }
