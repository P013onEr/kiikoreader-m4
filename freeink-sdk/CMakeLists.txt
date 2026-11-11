# FreeInk SDK as an ESP-IDF component (no arduino-esp32).
#
# Builds the display (FreeInkDisplay + BoardConfig), FreeInkUI and
# FreeInkFont's TTF engine (stb_truetype; FreeInkUIRasterFont.h draws it) on
# plain ESP-IDF, with idf/ supplying the slice of the Arduino API they call. Arduino
# and PlatformIO builds never read this file: they pull libs/* through lib_deps.
# See docs/esp-idf.md.
#
# Pick the device(s) in the project's top-level CMakeLists.txt, before
# project(), for example:
#   idf_build_set_property(COMPILE_DEFINITIONS "FREEINK_DEVICE_STICKY=1" APPEND)

file(GLOB _display_srcs
     "${CMAKE_CURRENT_LIST_DIR}/libs/display/FreeInkDisplay/src/*.cpp"
     "${CMAKE_CURRENT_LIST_DIR}/libs/display/FreeInkDisplay/src/bus/*.cpp"
     "${CMAKE_CURRENT_LIST_DIR}/libs/display/FreeInkDisplay/src/driver/*.cpp")

set(_m4_input_srcs
    "${CMAKE_CURRENT_LIST_DIR}/libs/hardware/InputManager/src/InputManager.cpp")
set(_m4_hardware_srcs
    "${CMAKE_CURRENT_LIST_DIR}/libs/hardware/Rtc/src/Rtc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/libs/hardware/BatteryMonitor/src/BatteryMonitor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/libs/hardware/FrontlightManager/src/FrontlightManager.cpp")

# FreeInkBook is freestanding C++17, so it can be linked directly into the
# M4 IDF image. Keep it M4-only until the other board backends expose the same
# 800x480 display/storage contract.
file(GLOB_RECURSE _m4_book_srcs
     "${CMAKE_CURRENT_LIST_DIR}/libs/book/FreeInkBook/src/*.cpp"
     "${CMAKE_CURRENT_LIST_DIR}/libs/book/FreeInkBook/src/*.c")

idf_component_register(
  SRCS ${_display_srcs}
       ${_m4_input_srcs}
       ${_m4_hardware_srcs}
       ${_m4_book_srcs}
       "libs/ui/FreeInkUI/src/FreeInkUI.cpp"
       "libs/font/FreeInkFont/src/TtfFont.cpp"
       "libs/font/FreeInkFont/src/Gsub.cpp"
       "libs/font/FreeInkFont/src/Gpos.cpp"
       "libs/font/FreeInkFont/src/FontAlloc.c"
       "idf/src/ArduinoCompat.cpp"
  INCLUDE_DIRS "idf/include"
               "libs/hardware/BoardConfig/include"
               "libs/hardware/InputManager/include"
               "libs/hardware/Rtc/include"
               "libs/hardware/BatteryMonitor/include"
               "libs/hardware/FrontlightManager/include"
               "libs/display/FreeInkDisplay/include"
               "libs/ui/FreeInkUI/include"
               "libs/font/FreeInkFont/include"
               "libs/book/FreeInkBook/include"
               "libs/book/FreeInkBook/include/epub"
               "libs/book/FreeInkBook/include/layout"
               "libs/book/FreeInkBook/include/render"
               "libs/book/FreeInkBook/include/cache"
               "libs/book/FreeInkBook/include/css"
               "libs/book/FreeInkBook/include/text"
               "libs/book/FreeInkBook/third_party/expat"
               "libs/book/FreeInkBook/third_party/libunibreak"
               "libs/book/FreeInkBook/third_party/miniz"
               "libs/book/FreeInkBook/third_party/pngle"
               "libs/book/FreeInkBook/third_party/tjpgd"
               "libs/book/FreeInkBook/third_party/stb"
  PRIV_INCLUDE_DIRS "libs/display/FreeInkDisplay/src"
                    "libs/font/FreeInkFont/third_party/stb"
                    "libs/book/FreeInkBook/src"
                    "libs/book/FreeInkBook/src/vendor"
  REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi esp_adc esp_timer heap
  PRIV_REQUIRES esp_driver_ledc esp_pm)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-function)
set_source_files_properties(
  "${CMAKE_CURRENT_LIST_DIR}/libs/book/FreeInkBook/src/vendor/miniz_impl.c"
  PROPERTIES COMPILE_OPTIONS "-Wno-type-limits")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations)
