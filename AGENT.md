# Agent Project Index

This file records the current working context for future coding agents. `AGENTS.md` remains the repository-wide guideline; this file adds the current task index, UI constraints, and active design assumptions.

## Repository Index

- `Solution/`: shared application code, services, algorithms, data hubs, BSP adapters, and target config dispatch.
- `Solution/Application/`: high-level apps. UI scheduling and blackboard-to-UI input adaptation are in `ui-maker-app.cpp/.h`.
- `Solution/System/Service/`: runtime services, including referee communication, super-cap comms, commander, motor actuator, and realtime comms.
- `Solution/ThirdParty/referee-hud-ui/`: shareable referee HUD UI package. It contains HUD business drawing plus the existing `UiRendererSrvc` render service.
- `Solution/System/DataHub/`: shared data definitions and blackboard variables.
- `Solution/tools/`: legacy/small utility files and earlier UI preview experiments.
- `ui-workspace/`: dedicated HTML UI drawing and visual-test workspace for referee UI prototypes.
  - `ui-workspace/index.html`: landing page for independent UI component previews.
  - `ui-workspace/wheel-leg.html`: wheel-leg state component preview.
  - `ui-workspace/capacitor-voltage.html`: optimized capacitor-voltage component preview.
  - `ui-workspace/team-logo-arcs.html`: `队徽.webp` arc-only approximation preview.
  - `ui-workspace/school-emblem.html`: RM school-emblem preview, centered above the capacitor arc with cyan former-main-color shapes.
  - `ui-workspace/auto-aim-icons.html`: four small geometry icons for visual auto-aim target modes.
  - `ui-workspace/bottom-holographic-dashboard.html`: bottom-center perspective edge line plus central dashboard, holographic instrument cluster, and four-switch icon deck prototype.
- `rm/`: old RM codebase used as reference. Treat as read-only reference unless explicitly asked.
- `Infantry-Chassis/`: chassis firmware target. CubeMX-generated code lives under `Core/`, `Drivers/`, and `Middlewares`.
- `Infantry-Gimbal/`: gimbal firmware target with the same generated-code layout.
- `build/<preset>/`: CMake out-of-tree build artifacts.

## Build Commands

- Configure chassis: `cmake --preset chassis-debug`
- Build chassis: `cmake --build build/chassis-debug`
- Configure gimbal: `cmake --preset gimbal-debug`
- Configure gimbal DR16: `cmake --preset gimbal-debug-dr16`
- Build both: `cmake --preset both-debug` then `cmake --build build/both-debug`

For shared `Solution/` changes, build both affected targets when practical. For chassis-only UI changes, at minimum build `build/chassis-debug`.

## Editing Constraints

- Do not hand-edit CubeMX-generated files under `Infantry-*/Core`, `Drivers`, or `Middlewares` unless the user explicitly asks for CubeMX/generated-code work.
- Do not edit `Solution/ThirdParty/*` directly unless updating a submodule pointer is the requested task.
- The worktree may be dirty. Do not revert unrelated user changes.
- Keep UI drawing changes in the application/drawing layer unless the user explicitly asks to change the renderer/send layer.
- Use existing `UiRendererSrvc` drawing APIs for firmware UI output. Avoid changing the package renderer files unless the task is specifically about renderer behavior.

## Current UI Architecture

- `UiMakerApp` should consume a unified UI input abstraction instead of directly scattering blackboard reads through drawing code.
- Reusable HUD drawing now lives in `Solution/ThirdParty/referee-hud-ui/include/referee-hud-ui.h` and `Solution/ThirdParty/referee-hud-ui/src/referee-hud-ui.cpp`.
- The renderer now lives in `Solution/ThirdParty/referee-hud-ui/include/ui-renderer-srvc.h` and `Solution/ThirdParty/referee-hud-ui/src/ui-renderer-srvc.cpp`. It is part of the shareable UI package and must keep its existing chain-call API and FreeRTOS queue implementation:
  - `UiRendererSrvc::draw(...).layer(...).color(...).width(...).start(...).asLine(...)`
  - `QueueHandle_t` plus `xQueueCreate/xQueueSend/xQueueReceive/xQueueReset`
  - `UiRendererSrvc::run()` performs the global `1/2/5/7` referee graphic batching.
- Do not replace `UiRendererSrvc` with a separate renderer abstraction unless the user explicitly asks for a platform-independent port.
- The current abstraction names are:
  - `RefereeHudInput`
  - `RefereeHudInputSource`
  - `RefereeHudUi`
  - `UiMakerInputSnapshot` / `UiMakerInputSource` remain compatibility aliases in `ui-maker-app.h`.
  - `UiMakerApp::setInputSource(...)`
- Firmware targets should consume this package with `add_subdirectory(Solution/ThirdParty/referee-hud-ui ...)` and `target_link_libraries(<target> PRIVATE referee_hud::ui)`, not by directly listing the UI package source files in the target. The CMake target is a source-carrying interface library so the renderer is compiled in the target's existing STM32/FreeRTOS/HAL context.
- The UI input snapshot should carry business state such as capacitor voltage, capacitor enabled/error state, reset requests, and leg-length state.
- A simulation input source is acceptable for visual/debug work. It should be easy to switch back to real blackboard input.
- The capacitor-voltage HTML prototype is now a separate top-center gauge. It only draws the arc track, ticks on the arc, dynamic mirrored arcs, and the numeric float value.

## Referee UI Coordinate And Mask Constraints

- The referee UI business canvas is `1920 x 1080`.
- Business coordinate origin is bottom-left.
- HTML previews must map business `y` to canvas `height - y`.
- `ui-workspace/RoboMaster 2025 机甲大师高校联盟赛选手端界面说明手册-学生版（20250219）.png` is the current reference mask.
- Gray areas in that PNG represent client UI occlusion zones. Custom UI controls must avoid those gray occlusion areas.
- UI previews should expose the occlusion mask visually so placement can be checked before porting to firmware.
- The custom UI is overlaid directly on the gimbal camera feed. The HTML preview only treats the RoboMaster client gray mask regions as forbidden; it no longer draws or checks an extra central vision forbidden rectangle.
- Placement code should check/avoid the client mask forbidden regions instead of merely drawing a warning overlay.
- Referee primitives do not support alpha/opacity. Real UI drawings and hardware-faithful HTML previews must use only `UiColor` enum colors (`Main`, `Yellow`, `Green`, `Orange`, `Purple`, `Pink`, `Cyan`, `Black`, `White`, `Del`). Do not simulate depth with `rgba`, glow, or fake dim colors; use geometry, line width, layer, and update state instead.

## Wheel-Leg Robot UI Understanding

- The wheel-leg robot is not a simple two-wheel chassis with two straight swing arms.
- It is a parallel-structure two-leg robot, visually similar to a pair of dog hind legs.
- For a side-view leg-length widget, draw one representative leg unless the operator needs left/right phase distinction. The current main-instrument prototype uses two legs with different colors.
- For dual-leg displays, use `Cyan` for the left leg and left body/instrument border, and `Orange` for the right leg and right border. Do not use `Main` for leg identity because `Main` follows red/blue camp and blue-side `Main` is too close to `Cyan`.
- The leg-length widget should separate static body graphics from moving leg graphics.
- Static body target: draw enough fixed body geometry to make the chassis, fixed hip/body mount, and center-of-mass reference clear. Static body graphics are sent on init/reset, so do not sacrifice readability to minimize local primitive count.
- The moving leg target uses a small dynamic primitive set:
  - upper link as one thick line,
  - lower link as one thick line,
  - wheel circle,
  - knee joint circle,
  - ankle/end joint circle.
- Do not make a local widget "fit" a `1/2/5/7` packet by adding meaningless primitives. The renderer batches the global queue; packet planning belongs to the overall UI scheduling layer, not to each local control.
- For firmware, this widget should update on leg-state changes rather than being resent periodically every UI tick.
- Keep the widget compact and aligned with the legacy `Solution/tools/referee-ui-preview.html` small-icon style. The current HTML prototype is on the right side, horizontally mirrored from the previous left-folding direction, and scaled to `0.6` of the previous prototype size, using line widths around `2..4`, joint radius around `4`, and wheel radius around `12`; reserve `25/30`-width strokes for major instruments only.
- In side view, the support wheel must stay under the body center of mass. Enforce `bodyCenterX == hipX == wheelX`.
- Physical link lengths are fixed. Do not fake leg length by scaling links. `L0/L1/L2` should change the effective hip-to-wheel distance by changing joint angles.
- The preview uses fixed upper/lower link lengths and computes the knee by two-link inverse kinematics from the target hip-to-wheel distance. The thigh/shin ratio is `210:250`; the compact UI drawing represents this as `105:125` before widget scaling. This keeps link lengths constant while making the visible leg length change clear.
- Do not add an in-canvas state rail/slider for leg length. The leg posture itself should communicate short/normal/long.
- Short static tick marks beside the three possible wheel heights are acceptable as wheel-position references; do not turn them into a separate slider/gauge.
- Prefer static geometric icons and pose changes over character text. If text is unavoidable, use English/ASCII only and send it as a separate character frame; do not mix character payloads with normal graphics.
- Current `ui-workspace/index.html` leg widget intentionally generates no character command and labels local primitive groups as static-vs-moving, not as final TX packets.
- Firmware `UiMakerApp` now contains the wheel-leg widget:
  - static body/tick graphics are emitted with `GraphicOption::Add` once after UI reset/init,
  - moving leg graphics are emitted with `GraphicOption::Add` the first time and `GraphicOption::Update` only after that,
  - the default simulation input cycles thigh angle and hip-to-wheel distance periodically; left and right legs use a `180 deg` phase offset,
  - real input mode reads `GimbalToChassisComm::msg.legLength`.
- Firmware `UiMakerApp` capacitor-voltage UI uses the optimized arc-only design:
  - static graphics are only the two arc-track segments and eight threshold ticks,
  - dynamic graphics are the left/right voltage arcs plus the numeric float value,
  - old capacitor icon/frame/fill-line graphics are removed from firmware drawing.
  - current capacitor geometry restores the original `rm/RM_Chassis/bsp/1_Handler/Bsp-UI-Handler.c` capacitor-voltage arc size: static arcs at `(960,1000)` with semi-axes `800/220`, ranges `140..170` and `190..220`; dynamic voltage arcs at `(960,980)` with the same semi-axes and a `30 deg` max sweep; numeric voltage at `(915,790)` rounded to three significant digits before drawing.
- `ui-workspace/team-logo-arcs.html` approximates `队徽.webp` using 19 referee Arc primitives, under the 20-graphic budget. The arcs are fitted against the original white mask so the filled region stays inside the logo strokes instead of covering the blue background. It can render with the reference image hidden via `?reference=0`; visual-check outputs both reference-overlaid and arc-only screenshots.
- `ui-workspace/school-emblem.html` ports `rm/RM_Chassis/bsp/1_Handler/Bsp-UserIntf-Handler .c::campInstructor(...)` as a school emblem. The hardware `UiMakerApp` draws the same 1x static 19-primitive emblem at `(960,900)`, centered above the capacitor arc, on UI group `3`; original `c01..c10` main-color shapes are cyan and `c11..c19` remain white.
- `ui-workspace/auto-aim-icons.html` previews four visual auto-aim mode icons: enemy vehicle, five-blade energy mechanism mode A, five-blade energy mechanism mode B, and outpost. The icons are small, text-free, and drawn from simple geometry that maps directly to referee UI primitives. The vehicle mode currently uses an abstract lock icon instead of a literal car. The current layout preserves the original left-side `x` positions and only moves the group vertically below the capacitor arc, using one unified abstract HUD border with only two short lower corner brackets, with 34 primitives total: CAR 8, ENE-A 7, ENE-B 8, POST 7, frame lines 4. The capacitor arc occupies roughly `x=446..1474, y=760..832`; the icon centers are around `x=230..600, y=694..718` and the frame sits around `y=666..700`. Per-icon `anchorX/Y` values compensate glyph visual center differences.
- Firmware `UiMakerApp` now contains the auto-aim icon group. The frame is static on UI group `4`, and the four icons are added once before later updates. `aimMode` uses the command semantic order `0=vehicle`, `1=outpost`, `2=energy A`, `3=energy B`; the display order remains vehicle, energy A, energy B, outpost to match the HTML layout. On mode changes, only the previous and current icons are updated instead of resending all 30 icon primitives.
- `ui-workspace/bottom-holographic-dashboard.html` is an HTML-only exploration for a bottom HUD component. It now uses left/right forward-slanted support arms, two projected switch icons per side, and a central polygon main-instrument frame containing the dual-leg state display. It is not yet ported to hardware.
- Current state labels:
  - `L0`: low/crouch posture
  - `L1`: normal posture
  - `L2`: high/extended posture

## UI Workspace

- Main prototype: `ui-workspace/index.html`
- Visual check script: `ui-workspace/visual-check.ps1`
- Visual check output: `build/ui-workspace-check/desktop.png` and `build/ui-workspace-check/mobile.png`
- The workspace is pure static HTML/CSS/JS and can be opened directly in a browser.
- Use `visual-check.ps1` to render screenshots with the local Playwright Chromium cache.

## Current Known State

- `Solution/Application/ui-maker-app.cpp` and `.h` contain ongoing UI abstraction and capacitor-voltage display work.
- `ui-workspace/` is the dedicated place for new UI design and visual testing.
- `rm/` contains old UI code references, including capacitor-voltage UI logic.
- There are existing unrelated dirty files in the worktree; do not revert them without explicit user instruction.
