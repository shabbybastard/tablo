cmake_minimum_required(VERSION 3.20)

project(tablo_wasm LANGUAGES C CXX)

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

set(LV_BUILD_CONF_PATH "${PROJECT_ROOT}/include/lv_conf.h" CACHE FILEPATH
    "lv_conf.h shared with the ESP32 firmware")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Do not build LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Do not build LVGL demos" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Do not build ThorVG" FORCE)

# lv_conf.h only enables the SDL driver under TABLO_PREVIEW. The definition and
# the SDL include flags must be visible while LVGL itself is being compiled.
add_compile_definitions(TABLO_PREVIEW=1)
add_compile_options(-sUSE_SDL=2)

add_subdirectory("${LVGL_DIR}" lvgl)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/core/*.cpp")
file(GLOB UI_SOURCES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/ui/*.cpp")
file(GLOB UI_FONT_SOURCES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/ui/fonts/*.c")
file(GLOB HAL_WASM_SOURCES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/hal_wasm/*.cpp")

add_executable(tablo
  ${CORE_SOURCES}
  ${UI_SOURCES}
  ${UI_FONT_SOURCES}
  ${HAL_WASM_SOURCES}
)

# Keep the preview in sync with the firmware version from VERSION.
file(STRINGS "${PROJECT_ROOT}/VERSION" TABLO_VERSION LIMIT_COUNT 1)
if(NOT TABLO_VERSION)
  set(TABLO_VERSION "0.0.0-dev")
endif()

target_compile_definitions(tablo PRIVATE TABLO_PREVIEW=1
  FW_VERSION="${TABLO_VERSION}")
target_include_directories(tablo PRIVATE
  "${PROJECT_ROOT}/src"
  "${PROJECT_ROOT}/include"
)
target_link_libraries(tablo PRIVATE lvgl)

target_link_options(tablo PRIVATE
  -sUSE_SDL=2
  -sALLOW_MEMORY_GROWTH=1
  -sEXIT_RUNTIME=0
  -sSTACK_SIZE=1048576
  "-sEXPORTED_FUNCTIONS=['_main','_tablo_preview_pointer']"
  "--preload-file=${PROJECT_ROOT}/preview/mock_data.json@/mock_data.json"
)

set_target_properties(tablo PROPERTIES
  OUTPUT_NAME "tablo"
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)

# The preloaded JSON is not a compiler input; make edits to it relink the app.
set_property(TARGET tablo APPEND PROPERTY LINK_DEPENDS
  "${PROJECT_ROOT}/preview/mock_data.json")
