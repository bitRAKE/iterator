# Iterator GDI

Native Win32/GDI dynamical-systems workbench for exploring logistic, tent, and sine maps. The app opens separate synchronized graph windows for bifurcation, cobweb, quadratic bridge, and cycle analysis.

## Build

Open a Visual Studio tools command prompt with `clang` available, then run:

```cmd
iterator_gdi.cmd
```

The script builds `iterator_gdi.exe` from the source files in this directory using `-O3 -march=native`.

## Run

```cmd
iterator_gdi.exe
```

Right-click any graph window for options. Left-click drag follows the active mode for that graph. Hover over bifurcation, cobweb, or quadratic bridge plots for a live crosshair numeric readout.

## Documentation

See [docs/usage.md](docs/usage.md).

## Source Sharing

The source set is the C files, build script, README, and documentation. Runtime data and generated exports are local artifacts unless deliberately shared.

## Generated Local Files

The app creates local runtime state and exports beside the executable:

- `state/*.mru`: session state, recent views, recent cycle periods, stable-cycle records
- `exports/*`: fixed-point logistic `R` export files
- `*.exe`, `*.obj`, `*.pdb`, `*.ilk`: build outputs

Share these files only when you specifically want to include a personal discovery set or compiled binary.
