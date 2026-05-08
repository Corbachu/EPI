#==============================================================================
# CMake Toolchain File – Sega Dreamcast (KallistiOS / SH-4)
#==============================================================================
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Dreamcast.cmake \
#         -B build/dreamcast
#
# Requirements:
#   • KallistiOS cross-compiler installed and $KOS_BASE set in the environment
#     (standard KOS install sets $KOS_BASE, $KOS_CC_BASE, $DC_ARM_BASE, etc.)
#   • Alternatively, set -DKOS_BASE=<path> on the cmake command line.
#
# KallistiOS environment variables used:
#   KOS_BASE       – root of the KOS tree  (e.g. /opt/toolchains/dc/kos)
#   KOS_CC_BASE    – root of the SH-4 gcc toolchain (e.g. /opt/toolchains/dc/sh-elf)
#
#==============================================================================

# Tell CMake we're cross-compiling.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR sh4)

# ---------------------------------------------------------------------------
# Locate KallistiOS
# ---------------------------------------------------------------------------
if(NOT DEFINED KOS_BASE)
    if(DEFINED ENV{KOS_BASE})
        set(KOS_BASE "$ENV{KOS_BASE}" CACHE PATH "KallistiOS root directory")
    else()
        message(FATAL_ERROR
            "KOS_BASE is not set.\n"
            "Set the KOS_BASE environment variable or pass "
            "-DKOS_BASE=<path> to cmake.")
    endif()
endif()

if(NOT DEFINED KOS_CC_BASE)
    if(DEFINED ENV{KOS_CC_BASE})
        set(KOS_CC_BASE "$ENV{KOS_CC_BASE}" CACHE PATH "KOS SH-4 toolchain root")
    else()
        set(KOS_CC_BASE "${KOS_BASE}/../sh-elf" CACHE PATH "KOS SH-4 toolchain root")
    endif()
endif()

# ---------------------------------------------------------------------------
# Compilers
# ---------------------------------------------------------------------------
set(CMAKE_C_COMPILER   "${KOS_CC_BASE}/bin/sh-elf-gcc"   CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${KOS_CC_BASE}/bin/sh-elf-g++"   CACHE FILEPATH "")
set(CMAKE_AR           "${KOS_CC_BASE}/bin/sh-elf-ar"    CACHE FILEPATH "")
set(CMAKE_RANLIB       "${KOS_CC_BASE}/bin/sh-elf-ranlib" CACHE FILEPATH "")
set(CMAKE_OBJCOPY      "${KOS_CC_BASE}/bin/sh-elf-objcopy" CACHE FILEPATH "")

# ---------------------------------------------------------------------------
# SH-4 CPU / FPU flags
# ---------------------------------------------------------------------------
# -m4-single-only   – use single-precision FPU (SH-4's native mode)
# -ml               – little-endian byte order
# -O2               – optimise for speed (good balance for 200 MHz SH-4)
# -ffast-math       – allow reciprocal / fused multiply-add approximations
# -fomit-frame-pointer – free a general-purpose register
set(_DC_CPU_FLAGS "-m4-single-only -ml")
set(_DC_OPT_FLAGS "-O2 -ffast-math -fomit-frame-pointer")

# Dreamcast-specific preprocessor defines understood by EPI.
set(_DC_DEFINES
    "-DDREAMCAST"
    "-D_arch_dreamcast"
    "-DPLATFORM_DREAMCAST"
    "-DHAVE_KOS"
)

# KOS include paths
set(_DC_INCLUDES
    "-I${KOS_BASE}/include"
    "-I${KOS_BASE}/kernel/arch/dreamcast/include"
    "-I${KOS_BASE}/addons/include"
)

set(CMAKE_C_FLAGS_INIT   "${_DC_CPU_FLAGS} ${_DC_OPT_FLAGS} ${_DC_DEFINES} ${_DC_INCLUDES}")
set(CMAKE_CXX_FLAGS_INIT "${_DC_CPU_FLAGS} ${_DC_OPT_FLAGS} ${_DC_DEFINES} ${_DC_INCLUDES} -std=c++20 -fno-exceptions -fno-rtti")

# ---------------------------------------------------------------------------
# Linker
# ---------------------------------------------------------------------------
set(_DC_LIB_DIRS "-L${KOS_BASE}/lib/dreamcast -L${KOS_BASE}/addons/lib/dreamcast")
set(_DC_LIBS     "-lkallisti -lc -lgcc -lm")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${_DC_LIB_DIRS} ${_DC_LIBS}")

# Prevent CMake from trying to run test executables on the host.
set(CMAKE_CROSSCOMPILING_EMULATOR "" CACHE STRING "")

# ---------------------------------------------------------------------------
# Platform feature switches used by EPI's CMakeLists.txt
# ---------------------------------------------------------------------------
set(EPI_PLATFORM_DREAMCAST TRUE  CACHE BOOL "Building for Dreamcast" FORCE)
set(EPI_PLATFORM_VITA      FALSE CACHE BOOL "" FORCE)
set(EPI_PLATFORM_LINUX     FALSE CACHE BOOL "" FORCE)
set(EPI_PLATFORM_WIN32     FALSE CACHE BOOL "" FORCE)
set(EPI_PLATFORM_MACOS     FALSE CACHE BOOL "" FORCE)
