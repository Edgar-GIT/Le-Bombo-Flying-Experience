# Le Bombo Flying Experience

Originally this project started as a GPU benchmark, then evolved into a chaotic 3D arcade game inspired by Terry Davis' Flight Simulator.

You fly over a procedural city, pick different vehicles, and maximize destruction with machine-gun fire, bombs, and a nuke event.

## Screenshots

### Main Menu
![Main Menu](caps/p1.png)

### Vehicle Select
![Vehicle Select](caps/p2.png)

### Gameplay
![Gameplay](caps/p3.png)

### Game Engine - Main Editor
![Game Engine Main Editor](caps/p4.png)

### Game Engine - Color + Inspector
![Game Engine Scene and Inspector](caps/p5.png)

### Game Engine - 2d mode and Block Workflow
![Game Engine Color and Blocks](caps/p6.png)

### Game Engine - Preview Mode
![Game Engine Preview and Tools](caps/p7.png)

## Tech Stack

- Languages: C (C99), Zig, C++
- C (C99): gameplay loop, rendering logic, attacks, UI, assets
- Zig: optimized build pipeline + optimization bridge/runtime helpers
- C++: Game Engine editor architecture and UI layer
- Graphics/Audio: Raylib (`raylib`, `raymath`, `rlgl`)
- Rendering backend (via Raylib): OpenGL

## Game Features

- 3D flight gameplay with multiple vehicles (Helicopter, UFO, Airplane, Drone, Hawk, Jet F-16)
- Procedural city generation
- Combat systems: machine-gun, laser, bombs, nuke
- HUD overlays, score-based events, menu flow
- Custom vehicle loading from `.vehicle` files exported by the Game Engine
- Extra helper tooling for faster workflow

## Clone The Repository

```bash
git clone https://github.com/Edgar-GIT/Le-Bombo-Flying-Experience.git
cd Le-Bombo-Flying-Experience
```

## Tools Folder

The `main/tools/` folder contains helper programs and platform separation:

- `main/tools/src/build_tool.c`: automated build system for game and/or vehicle previewer
- `main/tools/src/vehicle_previewer.c`: lightweight vehicle preview (left/right arrows + vehicle name)
- `main/tools/linux`, `main/tools/macos`, `main/tools/windows`: platform-specific build binaries

### Quick Start (Recommended)

From the project root:

```bash
# Build game and previewer (Linux example)
./main/tools/linux/build_tool --all

# Run the game
./main/LBFE
```

Build options:

- `--all` - build game + previewer
- `--game` - build game only
- `--preview` - build previewer only
- `--run-preview` - build and run previewer
- `--no-zig` - disable Zig path and use fallback C compiler path
- `--help` - show help

Generated binaries:

- `main/LBFE` (or `main/LBFE.exe` on Windows)
- `main/build/<platform>/vehicle_previewer`

## Build And Run (Game)

### Automated Build

From the project root:

```bash
./main/tools/linux/build_tool --all
./main/tools/macos/build_tool --all
.\main\tools\windows\build_tool.exe --all
```

Then run:

```bash
./main/LBFE
```

Build behavior:

- `build_tool` tries Zig first (`zig run main/GameEngine/src/zig/main.zig -- ...`)
- if Zig is unavailable/fails, it falls back to the C compiler path
- use `--no-zig` to force C-only path

### Manual Build

Assets are loaded with relative paths (`img/...` and `music/...`), so keep executable output in `main/`.

#### Zig path (recommended)

```bash
zig run main/GameEngine/src/zig/main.zig -- --all
./main/LBFE
```

#### C fallback (Linux)

```bash
gcc -std=c99 -O2 \
    main/src/main.c main/src/game.c main/src/ui.c main/src/obj.c \
    main/src/atacks.c main/src/screens.c main/src/config.c main/src/custom_vehicle.c \
    -o main/LBFE \
    -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

./main/LBFE
```

## Game Engine

A separate `main/GameEngine/` workspace is being built to let players create custom vehicles.

### Folder Structure

- `main/GameEngine/src` - engine/editor source code
- `main/GameEngine/projects` - editable work files (drafts/in-progress)
- `main/GameEngine/pieces` - user-created reusable piece templates (`.piece`)
- `main/GameEngine/builds` - exported/shareable final builds
- `main/GameEngine/schemes` - format/version rules for validation
- `main/GameEngine/cache` - temporary preview/cache files

### Current Capabilities (Implemented)

- Dedicated editor window with dark UI
- Top toolbar (`New`, `Open`, `Save`, `Export`, `Settings`) and editor mode controls
- VS Code style workspace tabs (`pj1`, `piece 1`, ...) with switching between project and piece workspaces
- Left tool tabs:
  - `SCN`: scene manager
  - `BLK`: block/primitive palette (click or drag-drop to viewport)
  - `CLR`: color editing panel
  - `PCS`: make your own piece workflow and piece browser
  - `LSR`: laser shotpoint authoring panel
- 2D/3D viewport modes
- Scene object selection + highlight and rectangle multi-select (`Select` mode)
- Selection drag-copy from viewport to another workspace tab
- Inspector panel (rename, visibility, anchored/free, position/rotation/scale)
- Layer system with visibility toggles and active-layer assignment
- Basic hierarchy support with parent assignment per object
- Gizmo modes: `Move`, `Rotate`, `Scale`
- Delete flow with confirmation popup (`Backspace/Delete`, `X` in scene manager, `X` in inspector)
- Live preview mode with dedicated controls
- Blast preview mode (`Prev Blasts`) and per-shotpoint preview
- Shotpoint placement mode with isolated object view
- Event console in status panel with timestamped logs (create/rename/delete/preview)
- Settings for camera and editor sensitivities
- Project persistence with `scene.json` + `autosave.json`
- Piece persistence with `.piece` templates auto-loaded from `main/GameEngine/pieces`
- Export to `.vehicle` in `main/GameEngine/builds`

### Editor Controls (Current)

- `W`, `E`, `R`: switch gizmo mode (`Move`, `Rotate`, `Scale`)
- `Select` toolbar button: drag rectangle multi-select in viewport
- `Backspace` / `Delete`: request delete for selected block (with confirmation)
- `LMB` on handles: manipulate selected block
- `RMB` drag: orbit camera
- `Shift + RMB` or `MMB`: pan camera
- Mouse wheel: zoom
- `Preview` button: open preview window

#### Preview Controls

- `WASD`: move camera
- Arrow keys: rotate view
- `RMB` drag: free look
- `MMB` drag: pan
- Mouse wheel: zoom
- `Shift`: faster movement
- `Esc` or `Close`: exit preview

### How To Use The Game Engine (Current Workflow)

1. Open the editor.
2. Go to `BLK` and add primitives (click or drag into viewport).
3. Select blocks from viewport or scene manager.
4. Use `SCN` to manage hierarchy/layers and set hide/show.
5. Use `Move/Rotate/Scale` gizmo modes to shape the vehicle.
6. Use `Select` mode for rectangle multi-select and drag selected blocks into another tab to copy.
7. Use `CLR` to adjust colors.
8. Go to `LSR` to create shotpoints and bind them to blocks.
9. Use `PCS` or `+ Piece` to create `.piece` templates from selection.
10. Open piece tabs, edit them, save them, and spawn them back into project tabs.
11. Use `Define Placement` to place shotpoints directly on a block.
12. Use `Preview Shot` or `Prev Blasts` to validate laser origin and style.
13. Fine-tune values in the inspector and shotpoint panel.
14. Click `Export` to generate `main/GameEngine/builds/<project>.vehicle`.
15. Run the main game and pick the custom vehicle in the vehicle menu.

### Planned Features (Not Implemented Yet)

- Advanced custom weapon behavior beyond shotpoint forward-fire
- Full material and texture pipeline for custom assets
- Validation tooling using schemes during import/export
- Extended build sharing pipeline with richer metadata
- Optional stress-test/benchmark mode integrated with generated vehicles

## Build And Run (Game Engine)

### Recommended (Zig builder)

From project root:

```bash
zig run main/GameEngine/src/zig/engine_build.zig
./main/GameEngine/src/GameEngine
```

From inside `main/GameEngine/src/zig`:

```bash
zig run engine_build.zig
../GameEngine
```

Generated binary:

- `main/GameEngine/src/GameEngine` (Linux/macOS)
- `main/GameEngine/src/GameEngine.exe` (Windows)

### Manual Compile (Linux)

```bash
g++ -std=c++17 -O3 \
    main/GameEngine/src/main/main.cpp \
    main/GameEngine/src/main/config.cpp \
    main/GameEngine/src/main/gui_manager.cpp \
    main/GameEngine/src/main/gui_workflow.cpp \
    main/GameEngine/src/main/gui_shotpoints.cpp \
    main/GameEngine/src/main/main_gui.cpp \
    main/GameEngine/src/main/vehicle_export.cpp \
    main/GameEngine/src/main/persistence.cpp \
    main/GameEngine/src/main/persistence_io.cpp \
    main/GameEngine/src/main/persistence_picker.cpp \
    main/GameEngine/src/main/workspace_tabs.cpp \
    main/GameEngine/src/main/piece_library.cpp \
    -I main/GameEngine/src/include \
    -o main/GameEngine/src/GameEngine \
    -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

## Custom Vehicle Runtime Pipeline

1. Build and run the Game Engine.
2. Create or open a project, build your vehicle, add shotpoints in `LSR`.
3. Click `Export`.
4. The engine writes `*.vehicle` files to `main/GameEngine/builds/`.
5. Start the main game (`./main/LBFE`).
6. On startup, the game scans that builds folder, loads the newest `.vehicle`, and exposes it in vehicle selection.

## Future Direction

This project is intended to keep growing over time.

- Expand gameplay systems and vehicles
- Evolve the Game Engine into a complete creator workflow
- Keep optimization work in Zig/C++ while preserving gameplay quality
- Maintain optional benchmark/stress-test paths
