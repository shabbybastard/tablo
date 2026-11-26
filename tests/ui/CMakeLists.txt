cmake_minimum_required(VERSION 3.20)

project(tablo_ui_test LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(PROJECT_ROOT "${PROJECT_ROOT}" CACHE PATH "tablo repository root")

set(LVGL_DIR "${PROJECT_ROOT}/.pio/libdeps/esp32s3/lvgl" CACHE PATH
    "LVGL 9.3 source tree (installed by PlatformIO)")
if(NOT IS_ABSOLUTE "${LVGL_DIR}")
  get_filename_component(LVGL_DIR "${LVGL_DIR}" ABSOLUTE BASE_DIR "${PROJECT_ROOT}")
  set(LVGL_DIR "${LVGL_DIR}" CACHE PATH "LVGL 9.3 source tree" FORCE)
endif()

if(NOT EXISTS "${LVGL_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "LVGL was not found at ${LVGL_DIR}.\n"
    "Run: pio pkg install -e esp32s3")
endif()

# Native LVGL with the same configuration as the firmware. TABLO_PREVIEW is
# intentionally not defined, so LV_USE_SDL stays 0 and the test needs no window
# system: LVGL renders into a plain 480x480 RGB565 buffer.
set(LV_BUILD_CONF_PATH "${PROJECT_ROOT}/include/lv_conf.h" CACHE FILEPATH
    "lv_conf.h shared with the ESP32 firmware")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Do not build LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Do not build LVGL demos" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Do not build ThorVG" FORCE)

add_subdirectory("${LVGL_DIR}" lvgl)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/core/*.cpp")
file(GLOB UI_SOURCES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/ui/*.cpp")
file(GLOB UI_FONT_SOURCES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/ui/fonts/*.c")

add_executable(tablo_ui_test
  ${CORE_SOURCES}
  ${UI_SOURCES}
  ${UI_FONT_SOURCES}
  "${PROJECT_ROOT}/src/hal_wasm/mock_provider.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/platform_test.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_ui.cpp"
)

target_include_directories(tablo_ui_test PRIVATE
  "${PROJECT_ROOT}/src"
  "${PROJECT_ROOT}/include"
)

target_compile_definitions(tablo_ui_test PRIVATE
  TABLO_UI_TEST=1
  "TABLO_MOCK_DATA_PATH=\"${PROJECT_ROOT}/preview/mock_data.json\""
)

target_compile_options(tablo_ui_test PRIVATE -Wall -Wextra)
target_link_libraries(tablo_ui_test PRIVATE lvgl)

find_package(Threads REQUIRED)
target_link_libraries(tablo_ui_test PRIVATE Threads::Threads)

set_target_properties(tablo_ui_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)
