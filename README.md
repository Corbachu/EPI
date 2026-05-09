# EPI Library V3.0

<img width="1536" height="1024" alt="libEPI" src="https://github.com/user-attachments/assets/21a3855f-1517-4f38-9b55-97850b628e7a" />

EPI (EDGE Platform Interface) is a lightweight C++ game library designed for
cross-platform game development.  Its design prioritises constrained and
embedded targets – particularly the **Sega Dreamcast** and **Sony PlayStation
Vita** – while remaining fully usable on Linux, macOS, and Windows.

---

## Supported Platforms

| Platform           | Toolchain / SDK                   | C++ Standard |
|--------------------|-----------------------------------|--------------|
| Linux              | GCC / Clang (host)                | C++20        |
| macOS              | Apple Clang (host)                | C++20        |
| Windows            | MSVC / MinGW                      | C++20        |
| **Dreamcast**      | KallistiOS + sh-elf-g++           | C++20        |
| **PS Vita**        | VitaSDK + arm-vita-eabi-g++       | C++20        |

---

## Building

### Prerequisites

* CMake ≥ 3.20
* A C++20-capable compiler

### Host build (Linux / macOS / Windows)

```sh
cmake -B build
cmake --build build
```

### Dreamcast (KallistiOS)

1. Install [KallistiOS](https://github.com/KallistiOS/KallistiOS) and set the
   `KOS_BASE` and `KOS_CC_BASE` environment variables as documented by KOS.
2. Configure and build:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Dreamcast.cmake \
      -B build/dreamcast
cmake --build build/dreamcast
```

Optional flags:
* `-DEPI_ENABLE_SH4_ACCEL=ON` – enables SH-4 hardware fixed-point math
  acceleration (faster distance / projection calculations).

### PS Vita (VitaSDK)

1. Install [VitaSDK](https://vitasdk.org/) and set the `VITASDK` environment
   variable to the SDK root (e.g. `/usr/local/vitasdk`).
2. Configure and build:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Vita.cmake \
      -B build/vita
cmake --build build/vita
```

---

## Platform-Specific Features

### Dreamcast

* **Memory management** – `epi_dreamcast.cc` queries `HW_MEMSIZE` (set by
  KallistiOS at boot) to detect whether 16 MB or 32 MB RAM is present, then
  carves out a secondary memory pool for large allocations beyond the system
  heap baseline.  Use `epi::DualAlloc` / `epi::DualFree` / `epi::DualRealloc`
  to benefit from the dual-pool allocator.
* **SH-4 math acceleration** – `fxp_vector_sh4.h` provides `epi::sh4::Dist2`,
  `epi::sh4::Dist3`, `epi::sh4::PerpDist`, and `epi::sh4::AlongDist` using the
  SH-4 hardware `fsqrt` and reciprocal instructions.  Enabled by
  `DITD_ENABLE_EPI_SH4_ACCEL=1` or the `-DEPI_ENABLE_SH4_ACCEL=ON` CMake
  option.
* **Input** – `input_dreamcast.cc` maps the KallistiOS Maple-bus controller
  API (`cont_state_t`) to the portable `epi::input` interface.

### PS Vita

* **Memory management** – `epi_vita.cc` queries the kernel via
  `sceKernelTotalFreeMemSize()` at runtime and creates a secondary EPI memory
  pool from the headroom above the 64 MB CRT baseline, giving applications a
  typical extra pool of ~176 MB on a 256 MB budget.
* **Input** – `input_vita.cc` handles:
  * Digital buttons and dual analogue sticks (`SceCtrlData`)
  * Front and rear capacitive touch panels (`SceTouchData`, up to 8 contacts
    each)
  * 6-DOF IMU: 3-axis accelerometer + 3-axis gyroscope
    (`SceMotionSensorState`)

---

## Cross-Platform Input API

```cpp
#include "input.h"

// Initialise after platform EPI::Init()
epi::input::Init();

// Game loop
while (running) {
    epi::input::Poll();

    // Digital buttons
    if (epi::input::ButtonsPressed() & epi::input::BTN_A)
        jump();

    // Analogue sticks
    epi::input::AnalogAxes axes;
    epi::input::Axes(&axes);
    move(axes.left_x, axes.left_y);

    // Touch (Vita only; safe to call on Dreamcast – returns empty state)
    epi::input::TouchState touch;
    epi::input::Touch(&touch);
    for (size_t i = 0; i < touch.front_count; ++i)
        handle_tap(touch.front[i].x, touch.front[i].y);

    // Motion (Vita only)
    epi::input::MotionState motion;
    epi::input::Motion(&motion);
    tilt(motion.accel_x, motion.accel_y);
}

epi::input::Shutdown();
```

---

## Dual-Memory Allocator API (Dreamcast & Vita)

```cpp
// Prefer the extra pool for large allocations; fall back to system heap.
void* buf = epi::DualAlloc(64 * 1024, /*preferExtra=*/1);
// … use buf …
epi::DualFree(buf);

// Query pool status
if (epi::HasExtraMemoryPool()) {
    printf("Extra pool: %u bytes free\n", epi::GetExtraMemoryPoolBytes());
}
```

---

## Module Overview

| Module              | Files                               | Description                              |
|---------------------|-------------------------------------|------------------------------------------|
| Platform backend    | `epi_dreamcast.*`, `epi_vita.*`, …  | Init/Shutdown + dual-memory allocator    |
| Input               | `input.h`, `input_dreamcast.*`, `input_vita.*` | Cross-platform input abstraction |
| Memory manager      | `memmanager.*`, `epi_dual_memory.h` | Slab allocator + dual-pool helpers       |
| Fixed-point math    | `fxp_*.h/cc`, `fxp_vector_sh4.h`   | SH-4-accelerated fixed-point math        |
| Image loading       | `image_*.h/cc`, `stb_image.*`       | PNG, JPEG, TGA, KMG image codecs         |
| Sound               | `sound_*.h/cc`                      | WAV, VOC, MUS→MIDI conversion            |
| Archives            | `archive_stuff/*`                   | WAD archive inspection and editing APIs  |
| Legacy id helpers   | `kmq2/*`                            | Quake II byte-order, hunk, and parsing helpers |
| Containers          | `arrays.*`, `tarray.h`, `pri_heap.*` | Lightweight collections                  |
| Math                | `math_*.h/cc`                       | Vectors, matrices, quaternions, colour   |
| Render helpers      | `rgl_vertex.h`                      | Generic vertex storage for GL pipelines  |
| Filesystem          | `filesystem.*`, `file.*`, `path.*`  | Platform-abstracted file I/O             |

---

## License

GNU General Public License v2 or later – see [LICENSE](LICENSE) for details.
