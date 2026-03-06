# Le Bombo Flying Experience

Originally this project started as a GPU benchmark, then evolved into a chaotic 3D arcade game inspired by Terry Davis' Flight Simulator.

You fly around a colorful procedural city, choose different vehicles, and cause maximum destruction with machine-gun fire, bombs, and a nuke event.

## Screenshots

### Main Menu
![Main Menu](caps/p1.png)

### Vehicle Select
![Vehicle Select](caps/p2.png)

### Gameplay
![Gameplay](caps/p3.png)

## Tech Stack

- Languages: C (C99), Zig, C++
- C (C99): main gameplay, rendering logic, attacks, UI, assets
- Zig: optimized build pipeline + runtime optimization bridge linked into `LBFE`
- C++: GameEngine base classes/editor architecture (in progress)
- Graphics/Audio: Raylib (`raylib`, `raymath`, `rlgl`)
- Rendering API (via Raylib): OpenGL

## What The Game Includes

- 3D flight gameplay with multiple vehicles (Hellicopter, Alien ship, Plane, Drone, Stealth bomber and f16 Jet)
- Procedural city generation with animated buildings/clouds
- Combat systems: machine-gun, laser, standard bombs, nuclear strike
- Particle-heavy explosions, shockwaves, smoke trails, debris rain
- HUD, overlays, score-based special events, and menu flow
- A Game Engine that allows the user to create their own vehicles

## Clone The Repository

```bash
git clone https://github.com/Edgar-GIT/Le-Bombo-Flying-Experience.git
cd Le-Bombo-Flying-Experience
```

## Future Plans

This project is intended to keep growing over time.

- A dedicated 3D engine will be created on top of the current project
- People will be able to build and use their own custom vehicles
- It will include an optional mode to run as a stress tester and benchmark

## Tools Folder

The `tools/` folder contains helper programs and platform separation:

- `tools/src/build_tool.c`: automated build system that compiles the game and/or vehicle previewer
- `tools/src/vehicle_previewer.c`: lightweight vehicle preview (left/right arrows + vehicle name)
- `tools/linux`, `tools/macos`, `tools/windows`: platform-specific build binaries

### Quick Start (Recommended)

From the project root:

```bash
# Build game and previewer
./main/tools/linux/build_tool --all

# Run the game
./main/LBFE
```

Build options:
- `--all` - Build both game and previewer
- `--game` - Build game only
- `--preview` - Build vehicle previewer only
- `--run-preview` - Build and run previewer
- `--no-zig` - Force C compiler path (disable Zig pipeline)
- `--help` - Show help

Generated binaries:
- `main/LBFE` (or `main/LBFE.exe` on Windows)
- `main/build/<platform>/vehicle_previewer`

The game executable is placed directly in `main/` so relative paths to assets work correctly.

Hot-reload note:
- A running `.exe`/binary cannot update C code changes in true runtime hot-reload mode by default.
- Recompile and relaunch using `build_tool` for a faster development cycle.

## Build And Run

### Automated Build (Recommended)

From the project root:

```bash
./main/tools/linux/build_tool --all    # Linux
./main/tools/macos/build_tool --all    # macOS
.\main\tools\windows\build_tool.exe --all  # Windows
```

Then run:
```bash
./main/LBFE
```

Build behavior:
- `build_tool` tries the Zig pipeline first (`zig run main/GameEngine/src/zig/main.zig -- ...`)
- If Zig is missing/fails, it automatically falls back to C compiler builds
- Use `--no-zig` to force the fallback C-only path
- The Zig game build links the Zig runtime bridge and compiles with `-DLBFE_USE_ZIG_RUNTIME`

### Manual Compilation

Assets are loaded with relative paths (`img/...` and `music/...`), so the executable must be in the `main/` directory.

**Zig Pipeline (manual, recommended for full optimized path)**

```bash
mkdir -p /tmp/zig-cache /tmp/zig-local
ZIG_GLOBAL_CACHE_DIR=/tmp/zig-cache ZIG_LOCAL_CACHE_DIR=/tmp/zig-local \
zig run main/GameEngine/src/zig/main.zig -- --all

./main/LBFE
```

Useful manual Zig modes:
- `--all` - Build game and previewer
- `--game` - Build game only
- `--preview` - Build previewer only

**C Fallback (manual, no Zig runtime bridge)**

**Linux (GCC + system Raylib)**

```bash
gcc -std=c99 -O2 \
    main/src/main.c main/src/game.c main/src/ui.c main/src/obj.c \
    main/src/atacks.c main/src/screens.c main/src/config.c \
    -o main/LBFE \
    -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

./main/LBFE
```

**macOS (Clang + Homebrew Raylib)**

```bash
clang -std=c99 -O2 \
    main/src/main.c main/src/game.c main/src/ui.c main/src/obj.c \
    main/src/atacks.c main/src/screens.c main/src/config.c \
    -o main/LBFE \
    -I/opt/homebrew/include -L/opt/homebrew/lib -lraylib \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo

./main/LBFE
```

**Windows (MinGW/MSYS2)**

```bash
gcc -std=c99 -O2 \
    main/src/main.c main/src/game.c main/src/ui.c main/src/obj.c \
    main/src/atacks.c main/src/screens.c main/src/config.c \
    -o main/LBFE.exe \
    -I<raylib_include_path> -L<raylib_lib_path> \
    -lraylib -lopengl32 -lgdi32 -lwinmm

./main/LBFE.exe
```

For Windows, if using MSYS2:
```
-I/mingw64/include -L/mingw64/lib
```

## Game Engine (In Progress)

A separate `GameEngine/` workspace is being built to let players create custom vehicles with their own shapes, colors, and behavior.

Current structure:

- `GameEngine/src` - engine and editor source code
- `GameEngine/projects` - editable work files (drafts/in-progress)
- `GameEngine/builds` - exported/shareable final builds
- `GameEngine/schemes` - format/version rules used to validate vehicle files
- `GameEngine/cache` - temporary preview/cache files generated by the engine

Goal:

- Create a vehicle in the editor
- Preview it immediately
- Save/export it
- Load it in-game from the builder pipeline
