#pragma once

#include <stddef.h>
#include <stdint.h>

bool murphyM4ProjectReaderOpen(const char *path);
bool murphyM4ProjectReaderNext(void);
bool murphyM4ProjectReaderPrevious(void);
const uint8_t *murphyM4ProjectReaderFrame(size_t *byteCount);
const char *murphyM4ProjectReaderTitle(void);
uint32_t murphyM4ProjectReaderPage(void);
uint32_t murphyM4ProjectReaderPageCount(void);
size_t murphyM4ProjectReaderTocCount(void);
bool murphyM4ProjectReaderTocTitle(size_t index, char *title, size_t capacity);
bool murphyM4ProjectReaderJumpToToc(size_t index);
bool murphyM4ProjectReaderAdjustFont(int direction);
bool murphyM4ProjectReaderToggleVertical(void);
bool murphyM4ProjectReaderToggleImages(void);
bool murphyM4ProjectReaderToggleBookFonts(void);
bool murphyM4ProjectReaderAdjustLineSpacing(int direction);
uint8_t murphyM4ProjectReaderFontPx(void);
bool murphyM4ProjectReaderRefresh(void);
