#pragma once
#include <cstddef>
#include <cstdint>
inline unsigned long fakeNow = 0;
inline unsigned long millis() { return fakeNow; }
inline void delay(unsigned long ms) { fakeNow += ms; }
inline double ledcSetup(uint8_t, uint32_t, uint8_t) { return 25000; }
inline bool ledcAttachPin(uint8_t, uint8_t) { return true; }
inline void ledcDetachPin(int8_t) {}
inline void ledcWrite(uint8_t, uint32_t) {}
#include <initializer_list>
constexpr int HIGH = 1, LOW = 0, OUTPUT = 3;
inline int Serial = 0;
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
