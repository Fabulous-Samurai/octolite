# Octolite 🐙💎

**Octolite** is a high-performance, **Zero-Copy Memory Allocator and Basic Entity Component System (ECS)** backend designed from scratch for modern multimedia engines and Vulkan renderers. 

It provides ultra-low latency memory allocations using a specialized **Quad-Pong (Double-Buffer per slot group)** ring structure, guaranteeing fast, concurrent access with minimal cache pollution.

## 🚀 Core Features

- **Zero-Copy Design:** No internal `memcpy` operations during buffer packing or data pipelines. Memory handles are sliced directly from pre-allocated virtual memory segments.
- **Quad-Pong Multi-Buffering:** Implements a double-bank matrix (`feed` and `grain` states) split across 4 distinct slot groups to completely segregate async simulation/compute updates from rendering routines.
- **Hardware-Level Precision:** Uses explicit virtual memory locks via `VirtualLock` to lock pages into physical RAM, avoiding os paging stalls. Structures are strictly `alignas(64)` cache-line aligned to prevent false sharing.
- **Bit-Packed Tagged Pointers:** Virtual addresses are packed with 16-bit metadata tags using bit-shifting (`ADDR_MASK` manipulation) above the 48-bit address space, allowing lock-free lifetime checks.

## 📐 Architecture Overview

```text
[Slot Group 0] ---> [ Bank 0: Feed  ] <--- CPU Allocation / Write Cache
               ---> [ Bank 1: Grain ] <--- GPU Compute Pipeline / Read
               
[Slot Group 1] ---> [ Bank 0: Feed  ] 
               ---> [ Bank 1: Grain ]
...
Total Buffer Matrix: 4 Slot Groups × 2 Banks = 8 Virtual Linear Pools
```

## 🛠️ Prerequisites & Build Pipeline

The project utilizes **C++26** features (`#embed` support and delete reasons) and relies on standard modern CMake compilation trees.

### Requirements:
- **Compiler:** GCC 16.2+ (MSYS2 UCRT64) or Clang 18+ with C++26 support.
- **Graphics API:** Vulkan SDK (1.4+ recommended).
- **Build System:** CMake 3.20+ & Ninja.

### Standard Build Steps:

Configure and compile specific demonstration targets directly via CMake variables:

```powershell
# 1. Configure for Basic ECS Engine Demo (CHOICE=1)
cmake -S . -B build -DCHOICE=1 -DCMAKE_BUILD_TYPE=Release

# 2. Build via Ninja / Make
cmake --build build

# 3. Execute Binary Output
.\build\example_basic_ecs.exe
```

### Build Matrix Targets (`-DCHOICE=` options):
- `-DCHOICE=1` : **Basic ECS Demo** - Validates the zero-copy tagged allocation bounds.
- `-DCHOICE=2` : **Rotating Shapes (Vulkan)** - Renders 3D procedural sphere clusters utilizing asynchronous compute integration directly fed by the custom memory lanes.
- `-DCHOICE=3` : **Benchmarks** - Stress-tests the raw allocation engine boundaries.
- `-DCHOICE=5` : Compiles **All** executable targets simultaneously.

## 🔮 Next-Gen: Moving towards Octulip 🐙

Octolite serves as the foundational validation step for a more advanced, memory supervisor module named **Octulip**. 

While Octolite verifies the baseline multi-banking, **Octulip** scales this architecture into an **Octa-Pong** setup—featuring 8 synchronized slot groups utilizing a **3-Bank (Feed -> Grain -> Harvest)** state pipeline matrix (24 independent memory pools total) to eliminate all thread synchronization stalls between Input, Compute, and Vulkan Presentation pipelines.
