<img width="1920" height="700" alt="BannerPNG" src="https://github.com/user-attachments/assets/259aed1a-583c-442c-a7f8-3dd6485899e0" />

# 🎮 Syncade Retro Platform

**Syncade** is a modular and extensible foundation for multi-system emulation, built upon the **Libretro API**. The project is designed to orchestrate emulation cores, manage virtual memory mapping, and process real-time audio/video signals using **OpenGL (via GLAD/GLFW)** for high-performance rendering and **SDL2** for a resilient audio pipeline.

Unlike conventional frontends, Syncade is engineered as a learning and experimentation tool, allowing developers to dive deep into the host-to-guest communication, environment callbacks, and hardware abstraction layers.



## Project Goals

Syncade provides a robust platform for developers and enthusiasts to:

* **Understand the Libretro Architecture:** Explore how environment commands, input state polling, and frame synchronization work in a real-world implementation.
* **Experiment with A/V Pipelines:** Implement advanced techniques such as **Cubic Resampling (Catmull-Rom)** for audio and OpenGL texture processing for immediate visual feedback.
* **Build Decoupled Systems:** Create an infrastructure where the Input system, Audio backend, and Video renderer are independent modules, making it easy to swap backends (e.g., moving to Vulkan or OpenAL).
* **Multicore Abstraction:** Implement dynamic library loading (`.dll` / `.so`) to allow the system to switch between different hardware architectures (Arcade, Consoles, Handhelds) transparently.

## Technical Highlights

Focused on simplicity and educational value, Syncade stands out by providing:

1.  **Hybrid Synchronization:** A "Push" model for video frame submission and a "Pull" model (via Ring Buffer) for audio, ensuring stability across varying refresh rates.
2.  **Audio Resilience:** A robust initialization logic with automatic fallbacks and real-time resampling to ensure compatibility with any host hardware sample rate.

3.  **Extensible Modularity:** The architecture allows contributors to implement one subsystem at a time (e.g., focusing strictly on Input or Shader passes) and see immediate results on screen.

## System Architecture
The project is organized into logical modules that mirror real hardware components:

* **Core Engine (`LibretroCore`):** The central orchestrator managing the emulator lifecycle and the C/C++ bridge.
* **Video Provider:** Manages OpenGL context, pixel alignment (pitch), and texture transfers from the Core to the GPU.
* **Audio Backend:** An SDL2-based system using a circular buffer to prevent audio crackling and ensure low latency.
* **Input Mapper:** Translates GLFW keyboard and gamepad events into the standard Libretro Joypad map.

---

## Getting Started

### Prerequisites
- **C++17 Compiler** (MSVC, GCC, or Clang)
- **CMake** (3.10+)
- **Dependencies:** GLFW3, GLAD, SDL2

### Build Instructions
```bash
git clone [https://github.com/youruser/syncade.git](https://github.com/youruser/syncade.git)
cd syncade
mkdir build && cd build
cmake ..
cmake --build .
