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

- Language: C (C99)
- Graphics/Audio: Raylib (`raylib`, `raymath`, `rlgl`)
- Rendering API (via Raylib): OpenGL

## What The Game Includes

- 3D flight gameplay with multiple vehicles (Hellicopter, Plane and f16 Jet)
- Procedural city generation with animated buildings/clouds
- Combat systems: machine-gun, laser, standard bombs, nuclear strike
- Particle-heavy explosions, shockwaves, smoke trails, debris rain
- HUD, overlays, score-based special events, and menu flow

## Main Techniques Used

- Quaternion-based aircraft orientation (yaw/pitch/roll)
- Modular C architecture (`src/game.c`, `src/obj.c`, `src/atacks.c`, `src/ui.c`, etc.)
- Real-time particle and effects systems
- World looping/repositioning for an "infinite" play area
- State-based screens (main menu, vehicle select, gameplay)

## Clone The Repository

```bash
git clone https://github.com/<Edgar-GIT>/Le-Bombo-Flying-Experience.git
cd Le-Bombo-Flying-Experience
```

## Future Plans

This project is intended to keep growing over time.

- A dedicated 3D engine will be created on top of the current project
- People will be able to build and use their own custom vehicles
- It will include an optional mode to run as a stress tester and benchmark

## Build And Run

Important:
- Build and run from inside `main/`
- Assets are loaded with relative paths (`img/...` and `music/...`)

### Linux (GCC + system Raylib)

```bash
cd main
gcc -std=c99 -O2 \
    src/main.c src/game.c src/ui.c src/obj.c src/atacks.c src/screens.c src/config.c \
    -o LBFE \
    -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
./LBFE
```

### macOS (Clang + Homebrew Raylib)

```bash
cd main
clang -std=c99 -O2 \
    src/main.c src/game.c src/ui.c src/obj.c src/atacks.c src/screens.c src/config.c \
    -o LBFE \
    -I/opt/homebrew/include -L/opt/homebrew/lib -lraylib \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo
./LBFE
```

### Windows (MinGW/MSYS2)

```bash
cd main
gcc -std=c99 -O2 \
    src/main.c src/game.c src/ui.c src/obj.c src/atacks.c src/screens.c src/config.c \
    -o LBFE.exe \
    -I<raylib_include_path> -L<raylib_lib_path> \
    -lraylib -lopengl32 -lgdi32 -lwinmm
./LBFE.exe
```
