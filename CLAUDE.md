# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

Two MCU targets built from one `Solution/` codebase, selected via CMake cache variable:

```bash
# Configure (from repo root)
cmake --preset Debug -DBUILD_TARGET=CHASSIS   # or GIMBAL / BOTH
cmake --preset Debug -DBUILD_TARGET=GIMBAL -DREMOTE_DEVICE=DR16  # DR16 | VideoLink | GamePad

# Build (from target subdirectory, e.g. Infantry-Chassis/)
cmake --build build/Debug
```

Each target (`Infantry-Chassis/`, `Infantry-Gimbal/`) has its own CMakeLists.txt that pulls sources from `../Solution/`. The root CMakeLists.txt dispatches based on `BUILD_TARGET`. Both targets use Ninja + gcc-arm-none-eabi toolchain.

**Submodules:** `git submodule update --init --recursive` is required before building. Submodules are under `Solution/ThirdParty/` (PYRo-uCtrl-Unity, TinyUSB).

## Target Selection & Conditional Compilation

The `CHASSIS` or `GIMBAL` macro is defined per-target in each CMakeLists.txt. Shared code uses `#ifdef CHASSIS` / `#elifdef GIMBAL` guards (most notably `Blackboard` and `MovtionCtrlApp`). When editing shared files, always consider both targets.

Config headers are dispatched from `Solution/Config/config.h`:
- `Config/Chassis/hw-config.h` + `algo-config.h` for CHASSIS
- `Config/Gimbal/hw-config.h` + `algo-config.h` for GIMBAL

## Architecture

### Entry Point
`Solution/app-main.cpp` → `ApplicationEntry()` → `StaticAppBase::startApplications()` — this iterates a static registry of all app instances and creates FreeRTOS static tasks.

### App Framework (`Solution/System/Thread/application-base.h`)
All application threads inherit from `StaticAppBase` + `Singleton<T>` (CRTP). The base class handles FreeRTOS static task creation and performance profiling. Four scheduling strategies:
- **PeriodicApp** — fixed-rate loop (`vTaskDelayUntil`)
- **ContinuousApp** — tight spin loop
- **NotifyApp** — wakes on `xTaskNotifyTake` (ISR-safe)
- **QueueApp** — wakes on `xQueueReceive` with IPCMsg

Each app must implement `init()` and `run()`. The task entry calls `init()` then enters the scheduling loop.

### Shared State (`Solution/System/DataHub/blackboard.h`)
`Blackboard` is a `Singleton` containing all inter-task data as `SeqVariable<T>` fields — a seqlock (lock-free, ISR-safe) wrapping POD structs. Writers use `write()` / `writeFromISR()`, readers use `read()`. Data is organized into zones: Intent (commands), State (sensor feedback), Output (actuator commands), Intermediate.

### Key Apps (under `Solution/Application/`)
- **MovtionCtrlApp** — chassis omnidirectional control (S-curve planners, PID) or gimbal yaw/pitch control, depending on target
- **FireCtrlApp** (Gimbal only) — shooting FSM with states: Passive → SpinUp → Ready → SingleFire/BurstFire, uses `pyro::fsm_t`

### Services (under `Solution/System/Service/`)
Low-level drivers and communication: commander (input parsing), state-estimator, motor-actuator, heart-beat, real-time-comm, vision-comm, referee, super-cap-comm, usb-device, interrupt-callback, ui-renderer-srvc.

### Input Pipeline
`Solution/System/Input/` → `commander.h/cpp` → `Blackboard` SeqVariables → Apps read commands.

### Algorithms (`Solution/Algorithm/`)
- `Motion/s-curve-planner.h` — Jerk-limited velocity trajectory
- `Power/power-limiter.h` — chassis power cap enforcement
- `Shoot/speed-compensater.h` — friction wheel speed compensation
- `Shoot/heat-controller.h` — referee heat limit management

### Middleware (PYRo-uCtrl-Unity submodule)
Provides: `pyro::pid_t` (PID), `pyro::fsm_t` (FSM), `pyro::can_hub_t` (CAN bus), DJI/DM motor drivers, INS, EKF/KF/OLS algorithms.

## Code Style

- `.clang-format` at repo root: LLVM-based, 4-space indent, 120-column limit, Attach braces, pointer left-aligned
- Header layout: 5-section template — (1) includes, (2) enum/define, (3) interface/class, (4) decorator, (5) factories
- File header comment block with `@file`, `@brief`, `@attention`, `@note`, `@author`, `@date`, `@version`
- Language: comments are in Chinese; code identifiers in English
