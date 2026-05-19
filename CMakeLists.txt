#==============================================================================
# EPI Library – CMake Build System
#==============================================================================
#
# Supported platforms (selected via toolchain file or auto-detected):
#   • Linux / macOS / Windows  (host build, default)
#   • Dreamcast  (-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Dreamcast.cmake)
#   • PS Vita    (-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Vita.cmake)
#
# Quick-start:
#   # Host (Linux / macOS / Windows):
#   cmake -B build && cmake --build build
#
#   # Dreamcast (requires KallistiOS):
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Dreamcast.cmake -B build/dc
#   cmake --build build/dc
#
#   # PS Vita (requires VitaSDK):
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Vita.cmake -B build/vita
#   cmake --build build/vita
#
#==============================================================================
cmake_minimum_required(VERSION 3.20)

project(EPI
    VERSION     3.0.0
    DESCRIPTION "EDGE Platform Interface – lightweight C++ game library"
    LANGUAGES   CXX C
)

# ---------------------------------------------------------------------------
# C++ standard – require C++20; allow individual files to use C++23 extensions
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ---------------------------------------------------------------------------
# Platform detection
# (toolchain files set EPI_PLATFORM_* booleans; fall back to auto-detection)
# ---------------------------------------------------------------------------
if(NOT DEFINED EPI_PLATFORM_DREAMCAST)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "sh4|sh-4" OR
       CMAKE_C_COMPILER MATCHES "sh-elf")
        set(EPI_PLATFORM_DREAMCAST TRUE)
    else()
        set(EPI_PLATFORM_DREAMCAST FALSE)
    endif()
endif()

if(NOT DEFINED EPI_PLATFORM_VITA)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7" AND
       CMAKE_C_COMPILER MATCHES "arm-vita-eabi")
        set(EPI_PLATFORM_VITA TRUE)
    else()
        set(EPI_PLATFORM_VITA FALSE)
    endif()
endif()

if(NOT DEFINED EPI_PLATFORM_WIN32)
    if(WIN32 AND NOT EPI_PLATFORM_DREAMCAST AND NOT EPI_PLATFORM_VITA)
        set(EPI_PLATFORM_WIN32 TRUE)
    else()
        set(EPI_PLATFORM_WIN32 FALSE)
    endif()
endif()

if(NOT DEFINED EPI_PLATFORM_MACOS)
    if(APPLE AND NOT EPI_PLATFORM_DREAMCAST AND NOT EPI_PLATFORM_VITA)
        set(EPI_PLATFORM_MACOS TRUE)
    else()
        set(EPI_PLATFORM_MACOS FALSE)
    endif()
endif()

if(NOT DEFINED EPI_PLATFORM_LINUX)
    if(UNIX AND NOT APPLE AND
       NOT EPI_PLATFORM_DREAMCAST AND NOT EPI_PLATFORM_VITA)
        set(EPI_PLATFORM_LINUX TRUE)
    else()
        set(EPI_PLATFORM_LINUX FALSE)
    endif()
endif()

# Summary
message(STATUS "EPI platform: "
    "Dreamcast=${EPI_PLATFORM_DREAMCAST} "
    "Vita=${EPI_PLATFORM_VITA} "
    "Linux=${EPI_PLATFORM_LINUX} "
    "macOS=${EPI_PLATFORM_MACOS} "
    "Win32=${EPI_PLATFORM_WIN32}"
)

# ---------------------------------------------------------------------------
# Optional features
# ---------------------------------------------------------------------------
option(EPI_ENABLE_SH4_ACCEL  "Enable SH-4 hardware math acceleration (Dreamcast only)" OFF)
option(EPI_ENABLE_COAL2       "Link with COAL2 scripting library"                       OFF)
option(EPI_ENABLE_PHYSFS      "Enable PhysicsFS virtual filesystem layer"               OFF)
option(EPI_BUILD_TESTS        "Build EPI unit tests (host builds only)"                 OFF)
option(EPI_ENABLE_RGL         "Build the EPI render layer (r_texcache, r_shader, r_effect) – requires OpenGL" OFF)

# On Dreamcast GLdc provides OpenGL 1.x natively; enable RGL automatically.
if(EPI_PLATFORM_DREAMCAST)
    set(EPI_ENABLE_RGL ON CACHE BOOL "Forced ON for Dreamcast (GLdc)" FORCE)
endif()

# ---------------------------------------------------------------------------
# Core source files – compiled on every platform
# ---------------------------------------------------------------------------
set(EPI_CORE_SOURCES
    epi/archive.cc
    epi/archive_wad.cc
    epi/arrays.cc
    epi/bytearray.cc
    epi/camera.cc
    epi/console.cc
    epi/errors.cc
    epi/exe_path.cc
    epi/file.cc
    epi/file_memory.cc
    epi/file_sub.cc
    epi/filesystem.cc
    epi/fxp_bbox.cc
    epi/fxp_fixed.cc
    epi/fxp_vector.cc
    epi/image_data.cc
    epi/image_hq2x.cc
    epi/image_jpeg.cc
    epi/image_png.cc
    epi/image_tga.cc
    epi/kmq2/epi_q2byteorder.cc
    epi/kmq2/epi_quake2hunk.cc
    epi/kmq2/epi_quake2stuff.cc
    epi/lowpass_filter.cc
    epi/math_angle.cc
    epi/math_bbox.cc
    epi/math_color.cc
    epi/math_crc.cc
    epi/math_matrix.cc
    epi/math_md5.cc
    epi/math_oddity.cc
    epi/math_quaternion.cc
    epi/math_random.cc
    epi/math_vector.cc
    epi/memmanager.cc
    epi/mersenne_twist.cc
    epi/model_data.cc
    epi/model_loader.cc
    epi/model_md2.cc
    epi/model_md3.cc
    epi/model_hlmdl.cc
    epi/model_md5.cc
    epi/model_aitdbody.cc
    epi/mus_2_midi.cc
    epi/path.cc
    epi/sort_cmp.cc
    epi/sound_data.cc
    epi/sound_gather.cc
    epi/sound_loader.cc
    epi/sound_voc.cc
    epi/sound_wav.cc
    epi/stb_image.cc
    epi/stb_image_write.cc
    epi/str_format.cc
    epi/timestamp.cc
    epi/timer_utility.cc
    epi/utility.cc
    epi/vector_format.cc
    # epi/utility_misc.cc  -- excluded: has engine-level dependencies (StringDup, AssertFail)
)

# ---------------------------------------------------------------------------
# Platform-specific source selection
# ---------------------------------------------------------------------------
if(EPI_PLATFORM_DREAMCAST)
    list(APPEND EPI_CORE_SOURCES
        epi/epi_dreamcast.cc
        epi/epi_memtest.cc
        epi/image_kmg.cc
        epi/input_dreamcast.cc
    )
    # Dreamcast uses POSIX-compatible filesystem helpers (same as Linux)
    list(APPEND EPI_CORE_SOURCES epi/filesystem_linux.cc)

elseif(EPI_PLATFORM_VITA)
    list(APPEND EPI_CORE_SOURCES
        epi/epi_vita.cc
        epi/input_vita.cc
    )
    list(APPEND EPI_CORE_SOURCES epi/filesystem_linux.cc)

elseif(EPI_PLATFORM_WIN32)
    list(APPEND EPI_CORE_SOURCES
        epi/epi_win32.cc
        epi/filesystem_win32.cc
    )

elseif(EPI_PLATFORM_MACOS)
    list(APPEND EPI_CORE_SOURCES
        epi/epi_macosx.cc
        epi/filesystem_linux.cc
        epi/exe_path.cc
    )

elseif(EPI_PLATFORM_LINUX)
    list(APPEND EPI_CORE_SOURCES
        epi/epi_linux.cc
        epi/filesystem_linux.cc
    )
endif()

# ---------------------------------------------------------------------------
# Render layer (r_texcache, r_shader, r_effect) – optional, requires OpenGL
# ---------------------------------------------------------------------------
if(EPI_ENABLE_RGL)
    if(NOT EPI_PLATFORM_DREAMCAST)
        find_package(OpenGL REQUIRED)
    endif()

    list(APPEND EPI_CORE_SOURCES
        epi/r_texcache.cc
        epi/r_shader.cc
        epi/r_effect.cc
        epi/model_skin.cc
    )
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(epi STATIC ${EPI_CORE_SOURCES})

target_include_directories(epi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/epi>
    $<INSTALL_INTERFACE:include/epi>
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
if(EPI_PLATFORM_DREAMCAST)
    target_compile_definitions(epi PUBLIC DREAMCAST _arch_dreamcast PLATFORM_DREAMCAST)
    if(EPI_ENABLE_SH4_ACCEL)
        target_compile_definitions(epi PUBLIC DITD_ENABLE_EPI_SH4_ACCEL=1)
    endif()

elseif(EPI_PLATFORM_VITA)
    target_compile_definitions(epi PUBLIC VITA __vita__ PLATFORM_VITA)

elseif(EPI_PLATFORM_WIN32)
    target_compile_definitions(epi PUBLIC _WINDOWS WIN32_LEAN_AND_MEAN)
    target_compile_definitions(epi PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(EPI_ENABLE_COAL2)
    target_compile_definitions(epi PUBLIC HAVE_COAL2)
endif()

if(EPI_ENABLE_PHYSFS)
    target_compile_definitions(epi PUBLIC HAVE_PHYSFS)
endif()

if(EPI_ENABLE_RGL)
    target_compile_definitions(epi PUBLIC EPI_ENABLE_RGL=1)
    if(NOT EPI_PLATFORM_DREAMCAST AND OpenGL_FOUND)
        target_link_libraries(epi PUBLIC OpenGL::GL)
    endif()
endif()

# ---------------------------------------------------------------------------
# Compiler warning flags (host builds only)
# ---------------------------------------------------------------------------
if(NOT EPI_PLATFORM_DREAMCAST AND NOT EPI_PLATFORM_VITA)
    if(MSVC)
        target_compile_options(epi PRIVATE /W4 /WX-)
    else()
        target_compile_options(epi PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
            -Wno-sign-compare
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests (host only)
# ---------------------------------------------------------------------------
if(EPI_BUILD_TESTS AND NOT EPI_PLATFORM_DREAMCAST AND NOT EPI_PLATFORM_VITA)
    enable_testing()
    add_subdirectory(tests EXCLUDE_FROM_ALL)
endif()
