# Iterator GDI Usage Guide

`iterator_gdi.exe` is a native Win32/GDI dynamical-systems workbench. It splits the original single-page iterator into separate synchronized graph windows so that bifurcation structure, orbit behavior, cycle roots, and fixed-point export work can be inspected together.

The application is useful in two phases:

1. Academic discovery: explore how simple interval maps organize fixed points, period doubling, chaos, and stable attractors.
2. Practical application: preserve stable cycles and export deterministic fixed-point logistic parameters for use in other tools.

## Run And Build

Run:

```cmd
iterator_gdi.exe
```

Build from a Visual Studio tools command prompt:

```cmd
cd /d path\to\iterator_gdi
iterator_gdi.cmd
```

The build script uses `clang -O3 -march=native` and links the Win32 GDI/User libraries.

## Window Model

The program opens four border-resizable popup windows:

- Bifurcation
- Cobweb
- Quadratic Bridge
- Cycle Lab

All windows share the same application state: family, parameter, orbit seed, bifurcation view, toggles, and Cycle Lab search period. Changing a parameter in one window redraws the other windows.

Right-click any window for options. Left-click drag follows that window's current drag mode.

Hover inside a graph plot to inspect it without changing state. Bifurcation reports parameter, `x`, detected period, and Lyapunov value. Cobweb reports `x_n` and `f(x_n)`. Quadratic Bridge reports complex `c` and the inferred real-axis logistic `r` when the hovered real coordinate lies on the logistic bridge.

## Keyboard Shortcuts

- `Left` / `Right`: step the shared family parameter through the current bifurcation view.
- `Ctrl+Left` / `Ctrl+Right`: fine parameter step.
- `Shift+Left` / `Shift+Right`: coarse parameter step.
- `Page Down` / `Page Up`: coarse parameter step down or up.
- `+` / `-`: increase or decrease the Cycle Lab search period.
- `Space`: choose a new `x0`.
- `Enter`: scan and refine cycles for the current Cycle Lab period.
- `L`: toggle the Lyapunov overlay.
- `P`: toggle period spectrum coloring.
- `Backspace`: reset the bifurcation view.
- `F5`: rebuild the Quadratic Bridge period map.
- `T`: tile graph windows on the active monitor.
- `A`: show all graph windows.
- `Tab` / `Shift+Tab`: move focus to the next or previous graph window.
- `Ctrl+1` / `Ctrl+2` / `Ctrl+3`: choose logistic, tent, or sine family.
- `Ctrl+R`: reset all shared parameters.

## Academic Discovery Workflow

### 1. Choose A Map Family

Right-click any window and choose a family:

- Logistic: `x[n+1] = r*x[n]*(1 - x[n])`, `r in [0, 4]`
- Tent: `x[n+1] = mu*min(x[n], 1 - x[n])`, `mu in [0, 2]`
- Sine: `x[n+1] = a*sin(pi*x[n])`, `a in [0, 1]`

The logistic and sine maps are smooth unimodal maps with period-doubling structure. The tent map gives a useful piecewise-linear contrast.

### 2. Read The Bifurcation Window

The Bifurcation window plots long-run orbit points:

- Horizontal axis: current family parameter
- Vertical axis: state value `x`
- Marker: current shared parameter

Right-click options:

- `Drag sets parameter`: left-drag horizontally to scan the family parameter.
- `Drag aspect box-zooms 2D`: left-drag a 2D zoom rectangle. The selected box preserves the current graph aspect ratio and updates both parameter and vertical `x` ranges.
- `Lyapunov overlay`: draw a Lyapunov curve over the bifurcation display.
- `Period spectrum; chaos black`: color each parameter column by detected period; chaotic or undetected period columns are black.
- `Recent bifurcation views`: restore previously visited 2D zoom rectangles.

Use this window first to locate regions worth investigating. Period windows inside chaotic regions are especially important because they often lead to stable attractors.

Move the mouse over the plot for a crosshair readout. The displayed period and Lyapunov value are recalculated at the hovered parameter, so the plot can be sampled without moving the shared marker.

### 3. Inspect Orbit Motion With Cobweb

The Cobweb window shows how one seed evolves under the current map:

- The curve is `f(x)`.
- The diagonal is `y = x`.
- The staircase shows iteration.

Right-click options:

- `Drag sets x0`: drag to change the orbit seed.
- `New x0`: choose a random seed.
- `Show transient`: include or hide transient motion before the attracting cycle.

Use this to see whether nearby points collapse into a stable cycle or wander chaotically.

Move the mouse over the plot to read `x_n` and the corresponding `f(x_n)` value on the curve.

### 4. Use The Quadratic Bridge

The Quadratic Bridge window shows the Mandelbrot relation for the logistic map:

```text
c = r/2 - r^2/4
```

This bridge only has direct meaning for the logistic family. For tent and sine, the window can still sweep the current parameter, but there is no quadratic conjugacy marker.

Move the mouse over the bridge to read the hovered complex `c`. The real-axis logistic `r` is shown when the real coordinate falls in the bridge interval `[-2, 0.25]`.

### 5. Analyze Cycles In Cycle Lab

Cycle Lab finds primitive period-`p` cycles by scanning roots of:

```text
f^p(x) - x = 0
```

It filters lower-period repeats, refines roots, computes each cycle multiplier, and sorts by `|multiplier|`.

Right-click options:

- `Search period -` / `Search period +`: choose the period to scan.
- `Recent search periods`: restore recent family/period choices.
- `Scan + Newton refine`: find primitive cycles for the selected period.
- `Stable cycles found`: restore preserved stable cycles.
- `Estimate Feigenbaum delta`: estimate the smooth critical-orbit cascade for logistic and sine.

A cycle is stable when:

```text
|product f'(x_i)| < 1
```

Stable cycles attract nearby states. Unstable cycles can still be found, especially in chaotic regions, but they do not collect nearby states. The application displays both but only preserves stable cycles as durable attractor records.

## Persistent MRU State

The app uses feature-specific MRU files under:

```text
.\state
```

There is no INI file. Each stateful feature owns a separate support file:

- `session.mru`: family, parameter, toggles, view ranges, search period, window rectangles
- `bif_views.mru`: recent 2D bifurcation zoom rectangles
- `cycle_periods.mru`: recent Cycle Lab family/period searches
- `stable_cycles.mru`: preserved stable cycle detections

This mirrors the MRU pattern used in `mru_rtfedit`: each state category has its own identity and retention rule.

## Practical Application Workflow

The practical target is to collect small deterministic sets from stable cycles.

1. Select `Logistic family`.
2. Use Bifurcation to locate a period window.
3. Set the parameter by dragging or by restoring a recent bifurcation view.
4. In Cycle Lab, choose a period.
5. Run `Scan + Newton refine`.
6. Stable cycles are automatically preserved in `stable_cycles.mru`.
7. Export fixed-point logistic `R` tables for downstream code.

The fixed-point logistic kernel expects:

```text
R = r / 4 encoded as Q0.bits
```

The exporter converts preserved stable logistic parameters with:

```text
R_raw = floor((r / 4) * 2^bits)
```

and saturates to the selected Q0 mask.

## Export Stable Logistic R

Right-click Cycle Lab and choose:

```text
Export stable logistic R
```

Available bit depths:

- `Q0.8 R table`
- `Q0.16 R table`
- `Q0.32 R table`
- `Q0.52 R table`

Exports are written under:

```text
.\exports
```

Each export creates:

- `stable_logistic_R_q0_<bits>.csv`
- `stable_logistic_R_q0_<bits>.inc`

The CSV preserves metadata:

- bit depth
- raw `R` decimal
- raw `R` hexadecimal
- source `r`
- period
- multiplier
- orbit points

The `.inc` file emits an assembly-friendly table:

- `db` for Q0.8
- `dw` for Q0.16
- `dd` for Q0.32
- `dq` for Q0.52

Q0.52 is the highest export mode currently offered because the app stores parameters as `double`; exporting Q0.64 would imply precision the source value does not reliably carry.

## Interpreting Stable Cycles As Practical Buckets

A stable period-`p` cycle has two behaviors:

```text
basin collapse: arbitrary nearby x -> one cycle phase
cycle motion:   phase n -> (n + 1) mod p
```

This makes a stable cycle resemble a modulus operator in another domain. The map first collapses many starting states into a small attractor set, then advances through that set cyclically.

Unstable cycles are still mathematically important. They reveal structure inside chaotic regions, but they are not preserved for deterministic bucket export because nearby states do not collapse onto them.

## Suggested Discovery Pattern

For academic exploration:

1. Start with logistic `r` near `3.5`.
2. Turn on `Period spectrum; chaos black`.
3. Use 2D box zoom around a colored period window.
4. Toggle `Lyapunov overlay` to see stability boundaries.
5. Scan matching periods in Cycle Lab.
6. Compare the cobweb path before and after changing `x0`.

For practical collection:

1. Scan candidate periods in known stable windows.
2. Let `stable_cycles.mru` accumulate only stable detections.
3. Export Q0.16 or Q0.32 first for compact deterministic tables.
4. Use Q0.52 when the downstream tool needs high-resolution parameters.

## Caveats

- Cycle scans find primitive roots of `f^p(x) - x`, not only attractors.
- Chaotic regions may contain many unstable cycles.
- Stable preservation requires `|multiplier| < 1`.
- The display list has a finite in-memory cap, but stable cycles are promoted before display truncation.
- Export is currently logistic-only because the fixed-point kernel encodes `R = r/4`.
