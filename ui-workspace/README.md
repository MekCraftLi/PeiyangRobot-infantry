# UI Workspace

This workspace is for referee UI drawing and visual testing.

## Current Constraints

- Canvas is `1920 x 1080`; business coordinates use a bottom-left origin.
- Gray mask regions from the RoboMaster client manual are treated as forbidden placement areas.
- Referee primitives have no alpha/opacity field. Actual UI drawings must use only `UiColor` enum colors (`Main`, `Yellow`, `Green`, `Orange`, `Purple`, `Pink`, `Cyan`, `Black`, `White`, `Del`); depth must be expressed by geometry, line width, layer, and update state instead of transparent or fake dim colors.
- UI components are kept in independent HTML files so each widget can be designed and visually checked in isolation.
- `队徽.webp` can be previewed as a referee-UI arc outline in `team-logo-arcs.html`; the current outline uses 19 Arc primitives, below the 20-graphic budget, fitted to the original white mask to avoid filling the blue background.
- `school-emblem.html` ports the `rm` camp-indicator drawing as a school emblem. The former `UI_Color_Main` shapes are cyan, the internal details remain white, and the emblem is centered above the capacitor arc.
- The integrated auto-aim status uses one compact energy-mechanism icon above the screen center. `aimMode` is a two-state value: `0=off`, `1=on`. Activation changes the five armor modules, sweep arc, arrow, and top status line; the outer reference arcs, center hub, and static frame stay white.
- The hardware `UiMakerApp` ports this auto-aim group on UI group `4`: static frame lines are sent once, dynamic icon/status primitives are added after reset and then updated when `aimMode` or target status changes.
- The wheel-leg leg-length widget is a low-bandwidth side-view dual-leg icon.
- The body is static and should be readable: draw a faceted chassis outline, center reference, fixed hip/body mount, and other fixed structure details as needed.
- Each moving leg uses a small dynamic primitive set and no character command: upper link as one thick line, lower link as one thick line, wheel, knee joint, ankle/end joint. Left and right legs use different colors.
- Left-leg geometry and the left side of the body/instrument border use fixed `Cyan`; right-leg geometry and the right side of the body/instrument border use fixed `Orange`. Do not use `Main` for leg identity because it changes with red/blue camp and can be confused with `Cyan` on blue side.
- Do not force this local widget to fit a `1/2/5/7` packet. Firmware should send the static body once, update the moving leg on state changes, and let the overall UI scheduler/renderer batch the global queue.
- The current wheel-leg widget is placed on the right side of the screen and scaled to `0.6` of the previous prototype size.
- A leg pose is controlled by two variables: hip-to-wheel line angle against the body and hip-to-wheel distance. The HTML simulator drives both variables periodically; the left and right legs use a 180-degree phase offset.
- The displayed two-link leg uses a fixed thigh/shin ratio of `210:250`; the UI keeps the compact legacy size by drawing this as `105:125` before applying the widget scale.
- The right-side widget is horizontally mirrored from the previous left-folding version: body facing, knee fold, and tick marks flip together.
- The three wheel-height states have short static tick marks beside the wheel positions. These are reference ticks only, not a slider control.
- Dimensions still follow the legacy `Solution/tools/referee-ui-preview.html` small-icon style, but after scaling use line widths around `2..4`, joint radius around `4`, and wheel radius around `12`.
- In side view, the support wheel is positioned from the hip-to-wheel line angle and distance; the knee is solved from the fixed upper/lower link lengths.
- Body and hip/body mount stay fixed. Wheel positions come from angle/distance input, and knee points are solved from those positions so links and joints do not drift apart.
- Do not add a state rail/slider inside the canvas; leg length should be readable from the posture itself.
- Prefer static geometry/icons over text. If text is ever required, keep it ASCII and send it in a separate character frame.
- The capacitor-voltage widget restores the original `rm` capacitor arc dimensions while keeping the reduced arc/tick/value-only primitive set. It only draws the arc track, ticks on the arc, dynamic symmetric arcs, and the numeric float value. The visible voltage value is formatted as three significant digits.

## Files

- `index.html`: integrated 1920 x 1080 HUD preview combining the capacitor arc, school emblem, bottom switch deck, dual-leg indicator, and auto-aim mode status.
- `wheel-leg.html`: interactive 1920 x 1080 wheel-leg state UI canvas. The business coordinate origin is bottom-left.
- `capacitor-voltage.html`: interactive 1920 x 1080 capacitor-voltage UI canvas.
- `team-logo-arcs.html`: interactive team-logo arc approximation for `队徽.webp`.
- `school-emblem.html`: `rm` school-emblem preview with cyan outer arcs and white internal detail.
- `auto-aim-icons.html`: legacy auto-aim mode icon exploration.
- `rm-2024-switch-icons.html`: reconstructed RM 2024 switch icon preview for gyro, ramp, friction wheel, and capacitor switch, using the original `rm` UI primitive geometry from commit `3504a9b4`.
- `movement-speed.html`: HUD chart preview for current movement speed, velocity vector, components, and short trend.
- `holographic-helmet.html`: line-only holographic helmet HUD silhouette preview.
- `bottom-holographic-dashboard.html`: bottom HUD composition with open feature lines for the two-arm perspective frame, four switch widgets, and a central leg-state instrument. It avoids closed boxes where short brackets and edge marks are enough. The drawing uses solid referee UI colors only, without alpha transparency.
- `rm-july-2024.html`: legacy `rm` chassis referee UI reconstructed from commit `3504a9b4` on 2024-07-31.
- `visual-check.ps1`: renders desktop and mobile screenshots for the independent component pages with the local Playwright Chromium cache.

## Open

Open `index.html` or any independent component HTML directly in a browser.

## Visual Check

```powershell
.\ui-workspace\visual-check.ps1
```

Screenshots are written to `build/ui-workspace-check`.
