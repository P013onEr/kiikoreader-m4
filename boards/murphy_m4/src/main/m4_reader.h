#pragma once

#include <stddef.h>
#include <stdint.h>

bool murphyM4ReaderOpen(const char* path);
bool murphyM4ReaderNext();
bool murphyM4ReaderPrevious();
const uint8_t* murphyM4ReaderFrame(size_t* byteCount);
const char* murphyM4ReaderTitle();
uint32_t murphyM4ReaderPage();
uint32_t murphyM4ReaderPageCount();
bool murphyM4ReaderStoredProgress(const char* path, uint32_t fileSize,
                                 uint16_t* spine, uint32_t* page);
