# Repository Guidelines

## Project Structure & Module Organization
This repository builds two STM32H723 firmware targets from one shared codebase.

- `Solution/`: shared application logic, services, algorithms, BSP adapters, and config dispatch (`Config/Chassis`, `Config/Gimbal`).
- `Infantry-Chassis/`: chassis target project (CubeMX-generated `Core/`, `Drivers/`, `Middlewares/`, linker script, target CMake).
- `Infantry-Gimbal/`: gimbal target project with the same layout.
- `Solution/ThirdParty/`: Git submodules (`PYRo-uCtrl-Unity`, `TinyUSB`), treated as external dependencies.
- `build/<preset>/`: out-of-tree artifacts from CMake presets.

## Build, Test, and Development Commands
- `git submodule update --init --recursive`: fetch required third-party sources before first build.
- `cmake --preset chassis-debug` / `cmake --preset gimbal-debug` / `cmake --preset both-debug`: configure root builds via `CMakePresets.json`.
- `cmake --build build/chassis-debug` (or `gimbal-debug`, `both-debug`): compile selected target(s).
- `cmake --preset gimbal-debug-dr16`: configure gimbal build with `DR16` remote profile.

## Coding Style & Naming Conventions
- Language baseline: C11 + C++23.
- Format with root `.clang-format` (LLVM-based): 4-space indentation, 120-column limit, pointer-left alignment, attached braces.
- Keep existing naming patterns: file names in lower-case kebab style (for example `motion-state-auto.cpp`), macros in `UPPER_SNAKE_CASE`, classes/types in project-established C++ style.
- Shared logic must respect target guards (`CHASSIS`, `GIMBAL`) and keep both build paths valid.

## Testing Guidelines
There is no top-level host unit-test pipeline in this repo. Validation is build- and hardware-centric:

- For target-specific changes, build the affected preset.
- For shared `Solution/` changes, build both chassis and gimbal presets.
- Include a brief hardware smoke-check note in PRs when behavior-facing modules are changed (control, comms, FSM, or BSP).

## Commit & Pull Request Guidelines
- Follow the existing commit pattern: `<type>: <summary>` where `type` is typically `feat`, `refactor`, `chore`, or `config`.
- Keep commits focused by subsystem (for example `Application`, `System/Service`, `Board-Support-Pack`).
- PRs should include: scope, affected target(s), presets used to build, and runtime validation notes.
- For protocol/UI or control-logic changes, include evidence (logs, captures, or screenshots) and mention any config or submodule updates explicitly.

## Configuration & Generated Code Notes
- Do not hand-edit CubeMX-generated files under `Infantry-*/Core`, `Drivers`, or `Middlewares` unless the `.ioc` flow is intentional.
- Avoid direct edits in `Solution/ThirdParty/*`; prefer submodule pointer updates when upgrading dependencies.
