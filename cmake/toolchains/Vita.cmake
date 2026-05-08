#==============================================================================
# CMake Toolchain File – Sony PlayStation Vita (VitaSDK / ARM Cortex-A9)
#==============================================================================
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Vita.cmake \
#         -B build/vita
#
# Requirements:
#   • VitaSDK installed and $VITASDK set in the environment
#     (standard VitaSDK install sets $VITASDK).
#   • Alternatively, set -DVITASDK=<path> on the cmake command line.
#
# VitaSDK home: https://vitasdk.org/
#
#==============================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR armv7-a)

# ---------------------------------------------------------------------------
# Locate VitaSDK
# ---------------------------------------------------------------------------
if(NOT DEFINED VITASDK)
    if(DEFINED ENV{VITASDK})
        set(VITASDK "$ENV{VITASDK}" CACHE PATH "VitaSDK root directory")
    else()
        message(FATAL_ERROR
            "VITASDK is not set.\n"
            "Set the VITASDK environment variable or pass "
            "-DVITASDK=<path> to cmake.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Compilers (arm-vita-eabi toolchain bundled with VitaSDK)
# ---------------------------------------------------------------------------
set(CMAKE_C_COMPILER   "${VITASDK}/bin/arm-vita-eabi-gcc"    CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${VITASDK}/bin/arm-vita-eabi-g++"    CACHE FILEPATH "")
set(CMAKE_AR           "${VITASDK}/bin/arm-vita-eabi-ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB       "${VITASDK}/bin/arm-vita-eabi-ranlib"  CACHE FILEPATH "")
set(CMAKE_OBJCOPY      "${VITASDK}/bin/arm-vita-eabi-objcopy" CACHE FILEPATH "")

# vita-elf-create and vita-make-fself are used in the link step; we expose
# them as CMake variables so the top-level CMakeLists can invoke them.
set(VITA_ELF_CREATE    "${VITASDK}/bin/vita-elf-create"      CACHE FILEPATH "")
set(VITA_MAKE_FSELF    "${VITASDK}/bin/vita-make-fself"      CACHE FILEPATH "")

# ---------------------------------------------------------------------------
# ARM Cortex-A9 + NEON flags
# ---------------------------------------------------------------------------
# -mcpu=cortex-a9  – target the Vita's Cortex-A9 cores
# -mfpu=neon       – enable NEON SIMD (Vita's PVR SGX543MP4+ shares the VFP
#                    pipeline, and NEON is available on the A9 cores)
# -mfloat-abi=hard – use hardware floating-point ABI (faster than softfp)
# -O2              – good balance of code size and speed for 444 MHz A9
set(_VITA_CPU_FLAGS "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb-interwork")
set(_VITA_OPT_FLAGS "-O2 -ffast-math -fomit-frame-pointer")

set(_VITA_DEFINES
    "-DVITA"
    "-D__vita__"
    "-DPLATFORM_VITA"
    "-DHAVE_VITASDK"
    "-DSCEULE=0"  # Little-endian
)

set(_VITA_INCLUDES
    "-I${VITASDK}/arm-vita-eabi/include"
    "-I${VITASDK}/include"
)

set(CMAKE_C_FLAGS_INIT   "${_VITA_CPU_FLAGS} ${_VITA_OPT_FLAGS} ${_VITA_DEFINES} ${_VITA_INCLUDES}")
set(CMAKE_CXX_FLAGS_INIT "${_VITA_CPU_FLAGS} ${_VITA_OPT_FLAGS} ${_VITA_DEFINES} ${_VITA_INCLUDES} -std=c++20 -fno-exceptions -fno-rtti")

# ---------------------------------------------------------------------------
# Linker
# ---------------------------------------------------------------------------
set(_VITA_LIB_DIRS "-L${VITASDK}/arm-vita-eabi/lib")
set(_VITA_LIBS     "-lSceCtrl_stub -lSceTouch_stub -lSceMotion_stub -lSceKernel_stub -lc -lgcc")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${_VITA_LIB_DIRS} ${_VITA_LIBS}")

# Prevent CMake from trying to run test executables on the host.
set(CMAKE_CROSSCOMPILING_EMULATOR "" CACHE STRING "")

# ---------------------------------------------------------------------------
# Platform feature switches used by EPI's CMakeLists.txt
# ---------------------------------------------------------------------------
set(EPI_PLATFORM_VITA      TRUE  CACHE BOOL "Building for PS Vita" FORCE)
set(EPI_PLATFORM_DREAMCAST FALSE CACHE BOOL "" FORCE)
set(EPI_PLATFORM_LINUX     FALSE CACHE BOOL "" FORCE)
set(EPI_PLATFORM_WIN32     FALSE CACHE BOOL "" FORCE)
set(EPI_PLATFORM_MACOS     FALSE CACHE BOOL "" FORCE)
