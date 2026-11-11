#include "m4_reader.h"

#include <BookArena.h>
#include <FreeInkBook.h>
#include <BookTypes.h>
#include <cache/PageCache.h>
#include <layout/ChapterLayout.h>
#include <render/PageRenderer.h>
#include <render/TtfFont.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <cstdio>
#include <climits>
#include <cerrno>
#include <cstring>
#include <new>
#include <sys/stat.h>
#include <unistd.h>

extern const uint8_t builtin_ttf_start[] asm("_binary_builtin_ttf_start");
extern const uint8_t builtin_ttf_end[] asm("_binary_builtin_ttf_end");

namespace {

using namespace freeink::book;

constexpr char TAG[] = "m4_reader";
constexpr uint16_t kWidth = 480;
constexpr uint16_t kHeight = 800;
constexpr size_t kPixels = static_cast<size_t>(kWidth) * kHeight;
constexpr size_t kGray2Bytes = kPixels / 4u;
constexpr size_t kBookArenaBytes = 384u * 1024u;
constexpr size_t kScratchArenaBytes = 512u * 1024u;
constexpr size_t kIndexArenaBytes = 128u * 1024u;
constexpr size_t kFontArenaBytes = 256u * 1024u;
constexpr size_t kProgressPathBytes = 192;
constexpr uint32_t kFontFingerprint = 0x4d344231u;
constexpr uint32_t kProgressMagic = 0x3452504du;
constexpr uint16_t kProgressVersion = 1;
class FileBookSource final : public BookSource {
 public:
  ~FileBookSource() override { close(); }

  bool open(const char* path) {
    close();
    file_ = std::fopen(path, "rb");
    if (file_ == nullptr || std::fseek(file_, 0, SEEK_END) != 0) {
      close();
      return false;
    }
    const long end = std::ftell(file_);
    if (end < 0 || std::fseek(file_, 0, SEEK_SET) != 0) {
      close();
      return false;
    }
    size_ = static_cast<uint64_t>(end);
    return true;
  }

  void close() {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
    size_ = 0;
  }

  int32_t readAt(const uint64_t offset, void* dst, const uint32_t len) override {
    if (file_ == nullptr || offset > static_cast<uint64_t>(LONG_MAX) ||
        std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) return -1;
    return static_cast<int32_t>(std::fread(dst, 1, len, file_));
  }

  uint64_t size() const override { return size_; }

 private:
  FILE* file_ = nullptr;
  uint64_t size_ = 0;
};

class SdCacheStorage final : public CacheStorage {
 public:
  bool prepare() {
    if (mkdir("/sdcard/.kiikoread", 0775) != 0 && errno != EEXIST) return false;
    if (mkdir("/sdcard/.kiikoread/m4-pages", 0775) != 0 && errno != EEXIST) return false;
    struct stat st{};
    return stat("/sdcard/.kiikoread/m4-pages", &st) == 0 && S_ISDIR(st.st_mode);
  }

  bool exists(const char* name) override {
    char path[224];
    struct stat st{};
    return makePath(name, path, sizeof(path)) && stat(path, &st) == 0 && S_ISREG(st.st_mode);
  }

  bool remove(const char* name) override {
    char path[224];
    if (!makePath(name, path, sizeof(path))) return false;
    return unlink(path) == 0 || errno == ENOENT;
  }

  int64_t fileSize(const char* name) override {
    char path[224];
    struct stat st{};
    return makePath(name, path, sizeof(path)) && stat(path, &st) == 0
               ? static_cast<int64_t>(st.st_size)
               : -1;
  }

  int32_t readAt(const char* name, const uint32_t offset, void* dst, const uint32_t len) override {
    char path[224];
    if (!makePath(name, path, sizeof(path))) return -1;
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return -1;
    const bool seekOk = offset <= static_cast<uint32_t>(LONG_MAX) &&
                        std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0;
    const int32_t read = seekOk ? static_cast<int32_t>(std::fread(dst, 1, len, file)) : -1;
    std::fclose(file);
    return read;
  }

  bool beginWrite(const char* name) override {
    if (writer_ != nullptr) return false;
    if (!makePath(name, finalPath_, sizeof(finalPath_)) ||
        std::snprintf(tempPath_, sizeof(tempPath_), "%s.tmp", finalPath_) >=
            static_cast<int>(sizeof(tempPath_))) return false;
    writer_ = std::fopen(tempPath_, "wb");
    return writer_ != nullptr;
  }

  bool write(const void* data, const uint32_t len) override {
    return writer_ != nullptr && std::fwrite(data, 1, len, writer_) == len;
  }

  bool endWrite() override {
    if (writer_ == nullptr) return false;
    bool ok = std::fflush(writer_) == 0;
    if (ok) ok = fsync(fileno(writer_)) == 0;
    if (std::fclose(writer_) != 0) ok = false;
    writer_ = nullptr;
    if (ok && rename(tempPath_, finalPath_) == 0) return true;
    (void)unlink(tempPath_);
    return false;
  }

 private:
  static bool makePath(const char* name, char* out, const size_t capacity) {
    if (name == nullptr || name[0] == '\0' || std::strchr(name, '/') != nullptr) return false;
    const int written = std::snprintf(out, capacity, "/sdcard/.kiikoread/m4-pages/%s", name);
    return written > 0 && static_cast<size_t>(written) < capacity;
  }

  FILE* writer_ = nullptr;
  char finalPath_[224]{};
  char tempPath_[232]{};
};

struct ReaderState {
  FileBookSource source;
  Book book;
  TtfFont font;
  FontChain fonts;
  Arena bookArena;
  Arena scratch;
  Arena indexArena;
  Arena fontArena;
  SdCacheStorage storage;
  PageCacheReader cache;
  LayoutParams params;
  uint8_t* bookMemory = nullptr;
  uint8_t* scratchMemory = nullptr;
  uint8_t* indexMemory = nullptr;
  uint8_t* fontMemory = nullptr;
  uint8_t* gray8 = nullptr;
  uint8_t* gray2 = nullptr;
  char cacheName[64]{};
  uint16_t cachedSpine = UINT16_MAX;
  uint16_t spine = 0;
  uint32_t page = 0;
  uint32_t fileSize = 0;
  char path[kProgressPathBytes]{};
  bool fontReady = false;
  bool ready = false;
};

struct ProgressRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t spine;
  uint32_t fileSize;
  uint32_t page;
  uint16_t pathLength;
  uint16_t reserved;
  uint32_t checksum;
  char path[kProgressPathBytes];
};

static_assert(sizeof(ProgressRecord) == 216, "M4 progress record layout changed");

ReaderState s_reader;

static uint8_t quantizeGray(const uint8_t value, const uint16_t x, const uint16_t y) {
  (void)x; (void)y;
  return static_cast<uint8_t>(value >> 6);
}

static bool allocate(ReaderState& state) {
  if (state.bookMemory != nullptr) return true;
  state.bookMemory = static_cast<uint8_t*>(heap_caps_malloc(kBookArenaBytes, MALLOC_CAP_SPIRAM));
  state.scratchMemory = static_cast<uint8_t*>(heap_caps_malloc(kScratchArenaBytes, MALLOC_CAP_SPIRAM));
  state.indexMemory = static_cast<uint8_t*>(heap_caps_malloc(kIndexArenaBytes, MALLOC_CAP_SPIRAM));
  state.fontMemory = static_cast<uint8_t*>(heap_caps_malloc(kFontArenaBytes, MALLOC_CAP_SPIRAM));
  state.gray8 = static_cast<uint8_t*>(heap_caps_malloc(kPixels, MALLOC_CAP_SPIRAM));
  state.gray2 = static_cast<uint8_t*>(heap_caps_malloc(kGray2Bytes, MALLOC_CAP_SPIRAM));
  if (!state.bookMemory || !state.scratchMemory || !state.indexMemory || !state.fontMemory ||
      !state.gray8 || !state.gray2) {
    ESP_LOGE(TAG, "reader PSRAM allocation failed");
    return false;
  }
  state.bookArena.init(state.bookMemory, kBookArenaBytes);
  state.scratch.init(state.scratchMemory, kScratchArenaBytes);
  state.indexArena.init(state.indexMemory, kIndexArenaBytes);
  state.fontArena.init(state.fontMemory, kFontArenaBytes);
  state.fontReady = state.font.init(builtin_ttf_start,
      static_cast<uint32_t>(builtin_ttf_end - builtin_ttf_start), state.fontArena);
  if (!state.fontReady || !state.fonts.add(&state.font)) {
    ESP_LOGE(TAG, "built-in reader font initialization failed");
    return false;
  }
  state.ready = true;
  return true;
}

static void makeParams(ReaderState& state) {
  state.params = LayoutParams{};
  state.params.pageWidth = kWidth;
  state.params.pageHeight = kHeight;
  state.params.marginLeft = 28;
  state.params.marginRight = 28;
  state.params.marginTop = 60;
  state.params.marginBottom = 52;
  state.params.baseSizePx = 18;
  state.params.language = state.book.metadata().language[0] != '\0'
                              ? state.book.metadata().language : "zh-CN";
  state.params.font = &state.fonts;
}

static bool openCacheForSpine(ReaderState& state, const uint16_t spineIndex) {
  const ManifestItem* item = state.book.spineItem(spineIndex);
  if (item == nullptr) return false;
  const ZipEntry* entry = state.book.zip().find(item->href);
  if (entry == nullptr) return false;
  const uint32_t generation = layoutGenerationHash(state.params, kFontFingerprint);
  if (!pageCacheName(spineIndex, generation, state.cacheName, sizeof(state.cacheName))) return false;
  state.indexArena.reset();
  state.cache.~PageCacheReader();
  new (&state.cache) PageCacheReader();
  BookStatus status = state.cache.open(state.storage, state.cacheName, generation, state.indexArena);
  if (status == BookStatus::Ok) {
    state.cachedSpine = spineIndex;
    return true;
  }
  if (state.storage.exists(state.cacheName)) (void)state.storage.remove(state.cacheName);

  state.indexArena.reset();
  PageCacheWriter writer;
  if (!writer.begin(state.storage, state.cacheName, generation, state.indexArena)) {
    ESP_LOGE(TAG, "cannot create page cache for spine %u", static_cast<unsigned>(spineIndex));
    return false;
  }
  uint32_t totalPages = 0;
  uint32_t totalChars = 0;
  state.scratch.reset();
  status = ChapterLayout::layout(state.source, state.book.zip(), *entry, item->href,
                                 state.params, state.scratch, writer, &totalPages, &totalChars);
  if (status != BookStatus::Ok || writer.failed()) {
    writer.abort();
    ESP_LOGE(TAG, "spine %u layout/cache failed: %s pages=%u chars=%u",
             static_cast<unsigned>(spineIndex), bookStatusName(status),
             static_cast<unsigned>(totalPages), static_cast<unsigned>(totalChars));
    return false;
  }
  writer.setTotalChars(totalChars);
  const bool saved = writer.finish();
  if (!saved) {
    ESP_LOGE(TAG, "spine %u layout/cache failed: %s pages=%u chars=%u",
             static_cast<unsigned>(spineIndex), bookStatusName(status),
             static_cast<unsigned>(totalPages), static_cast<unsigned>(totalChars));
    return false;
  }
  ESP_LOGI(TAG, "spine %u cached: %u pages, %u chars",
           static_cast<unsigned>(spineIndex), static_cast<unsigned>(totalPages),
           static_cast<unsigned>(totalChars));
  state.indexArena.reset();
  state.cache.~PageCacheReader();
  new (&state.cache) PageCacheReader();
  status = state.cache.open(state.storage, state.cacheName, generation, state.indexArena);
  state.cachedSpine = status == BookStatus::Ok ? spineIndex : UINT16_MAX;
  return status == BookStatus::Ok;
}

static uint32_t progressHash(const void* data, const size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  uint32_t hash = UINT32_C(2166136261);
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * UINT32_C(16777619);
  return hash;
}

static uint32_t pathHash(const char* path) {
  uint32_t hash = UINT32_C(2166136261);
  for (const auto* p = reinterpret_cast<const unsigned char*>(path); *p; ++p)
    hash = (hash ^ *p) * UINT32_C(16777619);
  return hash;
}

static bool progressFilePath(const char* path, char* out, const size_t capacity) {
  const int written = std::snprintf(out, capacity, "/sdcard/.kiikoread/progress/m4-%08x.bin",
                                   static_cast<unsigned>(pathHash(path)));
  return written > 0 && static_cast<size_t>(written) < capacity;
}

static bool loadProgressRecord(const char* path, const uint32_t fileSize,
                               uint16_t* spine, uint32_t* page) {
  if (path == nullptr || spine == nullptr || page == nullptr ||
      std::strlen(path) >= kProgressPathBytes) return false;
  char filename[112];
  if (!progressFilePath(path, filename, sizeof(filename))) return false;
  FILE* file = std::fopen(filename, "rb");
  if (file == nullptr) return false;
  ProgressRecord record{};
  bool ok = std::fread(&record, 1, sizeof(record), file) == sizeof(record) &&
            std::fgetc(file) == EOF;
  std::fclose(file);
  const uint32_t storedChecksum = record.checksum;
  record.checksum = 0;
  const size_t pathLength = std::strlen(path) + 1;
  ok = ok && record.magic == kProgressMagic && record.version == kProgressVersion &&
       record.fileSize == fileSize && record.pathLength == pathLength &&
       std::memcmp(record.path, path, pathLength) == 0 &&
       storedChecksum == progressHash(&record, sizeof(record));
  if (!ok) return false;
  *spine = record.spine;
  *page = record.page;
  return true;
}

static bool saveProgressRecord(const ReaderState& state) {
  const size_t pathLength = std::strlen(state.path) + 1;
  if (pathLength > kProgressPathBytes || state.fileSize == 0) return false;
  if ((mkdir("/sdcard/.kiikoread", 0775) != 0 && errno != EEXIST) ||
      (mkdir("/sdcard/.kiikoread/progress", 0775) != 0 && errno != EEXIST)) return false;
  char filename[112], temp[120];
  if (!progressFilePath(state.path, filename, sizeof(filename)) ||
      std::snprintf(temp, sizeof(temp), "%s.tmp", filename) >= static_cast<int>(sizeof(temp))) return false;
  ProgressRecord record{};
  record.magic = kProgressMagic;
  record.version = kProgressVersion;
  record.spine = state.spine;
  record.fileSize = state.fileSize;
  record.page = state.page;
  record.pathLength = static_cast<uint16_t>(pathLength);
  std::memcpy(record.path, state.path, pathLength);
  record.checksum = progressHash(&record, sizeof(record));
  FILE* file = std::fopen(temp, "wb");
  if (file == nullptr) return false;
  bool ok = std::fwrite(&record, 1, sizeof(record), file) == sizeof(record) &&
            std::fflush(file) == 0 && fsync(fileno(file)) == 0;
  if (std::fclose(file) != 0) ok = false;
  if (ok && std::rename(temp, filename) == 0) return true;
  (void)unlink(temp);
  return false;
}

static bool renderCurrent(ReaderState& state) {
  if (!state.ready || state.spine >= state.book.spineCount() ||
      !openCacheForSpine(state, state.spine)) return false;
  state.scratch.reset();
  const size_t mark = state.scratch.mark();
  Page page{};
  BookStatus status = state.cache.readPage(state.page, state.scratch, &page);
  if (status == BookStatus::Ok) {
    std::memset(state.gray8, 0xff, kPixels);
    const FrameTarget target{state.gray8, static_cast<int16_t>(kWidth),
        static_cast<int16_t>(kHeight), static_cast<int16_t>(kWidth),
        FrameFormat::Gray8, FrameRotation::None};
    status = PageRenderer::render(page, state.fonts, state.source, state.book.zip(),
                                  state.scratch, target);
  }
  if (status == BookStatus::Ok) {
    for (uint16_t y = 0; y < kHeight; ++y) {
      for (uint16_t x = 0; x < kWidth; ++x) {
        const uint8_t level = quantizeGray(state.gray8[static_cast<size_t>(y) * kWidth + x], x, y);
        const size_t pixel = static_cast<size_t>(y) * kWidth + x;
        const size_t index = pixel / 4u;
        const uint8_t shift = static_cast<uint8_t>(6u - ((pixel & 3u) * 2u));
        if ((pixel & 3u) == 0) state.gray2[index] = 0;
        state.gray2[index] |= static_cast<uint8_t>(level << shift);
      }
    }
  } else {
    ESP_LOGE(TAG, "page render failed: %s spine=%u page=%u",
             bookStatusName(status), static_cast<unsigned>(state.spine),
             static_cast<unsigned>(state.page));
  }
  state.scratch.release(mark);
  return status == BookStatus::Ok;
}

static bool moveToSpine(ReaderState& state, const int direction) {
  int32_t candidate = static_cast<int32_t>(state.spine) + direction;
  while (candidate >= 0 && candidate < static_cast<int32_t>(state.book.spineCount())) {
    state.spine = static_cast<uint16_t>(candidate);
    state.page = direction > 0 ? 0 : UINT32_MAX;
    if (!openCacheForSpine(state, state.spine)) return false;
    const uint32_t count = state.cache.pageCount();
    if (count != 0) {
      state.page = direction > 0 ? 0 : count - 1;
      return renderCurrent(state);
    }
    candidate += direction;
  }
  return false;
}

static bool openFirstSpine(ReaderState& state) {
  for (uint16_t spine = 0; spine < state.book.spineCount(); ++spine) {
    state.spine = spine;
    if (!openCacheForSpine(state, spine)) return false;
    if (state.cache.pageCount() == 0) continue;
    state.page = 0;
    return renderCurrent(state);
  }
  return false;
}

}  // namespace

bool murphyM4ReaderOpen(const char* path) {
  ReaderState& state = s_reader;
  if (!allocate(state)) return false;
  state.source.close();
  state.book.~Book();
  new (&state.book) Book();
  state.bookArena.reset();
  state.scratch.reset();
  state.cachedSpine = UINT16_MAX;
  state.spine = 0;
  state.page = 0;
  state.fileSize = 0;
  state.path[0] = '\0';
  if (!state.source.open(path)) {
    ESP_LOGE(TAG, "cannot open EPUB: %s", path ? path : "(null)");
    return false;
  }
  struct stat sourceStat{};
  if (path == nullptr || std::strlen(path) >= sizeof(state.path) ||
      stat(path, &sourceStat) != 0 || !S_ISREG(sourceStat.st_mode) || sourceStat.st_size <= 0 ||
      static_cast<uint64_t>(sourceStat.st_size) > UINT32_MAX) {
    ESP_LOGE(TAG, "EPUB path or size is not supported");
    return false;
  }
  std::snprintf(state.path, sizeof(state.path), "%s", path);
  state.fileSize = static_cast<uint32_t>(sourceStat.st_size);
  if (!state.storage.prepare()) {
    ESP_LOGE(TAG, "cannot prepare /sdcard/.kiikoread/m4-pages");
    return false;
  }
  BookStatus status = state.book.open(state.source, state.bookArena, state.scratch);
  if (status != BookStatus::Ok || state.book.spineCount() == 0) {
    ESP_LOGE(TAG, "EPUB open failed: %s", bookStatusName(status));
    return false;
  }
  makeParams(state);
  ESP_LOGI(TAG, "opened '%s', spine=%u", state.book.metadata().title,
           static_cast<unsigned>(state.book.spineCount()));
  uint16_t savedSpine = 0;
  uint32_t savedPage = 0;
  if (loadProgressRecord(state.path, state.fileSize, &savedSpine, &savedPage) &&
      savedSpine < state.book.spineCount() && openCacheForSpine(state, savedSpine)) {
    const uint32_t pageCount = state.cache.pageCount();
    if (pageCount != 0) {
      state.spine = savedSpine;
      state.page = savedPage < pageCount ? savedPage : pageCount - 1;
      if (renderCurrent(state)) {
        ESP_LOGI(TAG, "restored reading position: spine=%u page=%u/%u",
                 static_cast<unsigned>(state.spine), static_cast<unsigned>(state.page + 1),
                 static_cast<unsigned>(pageCount));
        return true;
      }
    }
  }
  state.spine = 0;
  state.page = 0;
  return openFirstSpine(state);
}

bool murphyM4ReaderNext() {
  ReaderState& state = s_reader;
  if (!state.ready || !openCacheForSpine(state, state.spine)) return false;
  if (state.page + 1u < state.cache.pageCount()) {
    ++state.page;
    const bool rendered = renderCurrent(state);
    if (rendered && !saveProgressRecord(state))
      ESP_LOGW(TAG, "reading position save failed: spine=%u page=%u",
               static_cast<unsigned>(state.spine), static_cast<unsigned>(state.page));
    return rendered;
  }
  const bool moved = moveToSpine(state, 1);
  if (moved && !saveProgressRecord(state))
    ESP_LOGW(TAG, "reading position save failed: spine=%u page=%u",
             static_cast<unsigned>(state.spine), static_cast<unsigned>(state.page));
  return moved;
}

bool murphyM4ReaderPrevious() {
  ReaderState& state = s_reader;
  if (!state.ready || !openCacheForSpine(state, state.spine)) return false;
  if (state.page > 0) {
    --state.page;
    const bool rendered = renderCurrent(state);
    if (rendered && !saveProgressRecord(state))
      ESP_LOGW(TAG, "reading position save failed: spine=%u page=%u",
               static_cast<unsigned>(state.spine), static_cast<unsigned>(state.page));
    return rendered;
  }
  const bool moved = moveToSpine(state, -1);
  if (moved && !saveProgressRecord(state))
    ESP_LOGW(TAG, "reading position save failed: spine=%u page=%u",
             static_cast<unsigned>(state.spine), static_cast<unsigned>(state.page));
  return moved;
}

const uint8_t* murphyM4ReaderFrame(size_t* byteCount) {
  if (byteCount != nullptr) *byteCount = s_reader.gray2 != nullptr ? kGray2Bytes : 0;
  return s_reader.gray2;
}

const char* murphyM4ReaderTitle() {
  return s_reader.ready ? s_reader.book.metadata().title : "";
}

uint32_t murphyM4ReaderPage() { return s_reader.page + 1u; }

uint32_t murphyM4ReaderPageCount() {
  return s_reader.ready && s_reader.cachedSpine == s_reader.spine ? s_reader.cache.pageCount() : 0;
}

bool murphyM4ReaderStoredProgress(const char* path, const uint32_t fileSize,
                                 uint16_t* spine, uint32_t* page) {
  return loadProgressRecord(path, fileSize, spine, page);
}
