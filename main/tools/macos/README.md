# macOS Tools Output

This folder stores macOS executables for:
- `build_tool`
- `vehicle_previewer`

Build locally from repo root:

```bash
clang -std=c99 -O2 main/tools/src/build_tool.c -o main/tools/macos/build_tool
clang -std=c99 -O2 main/tools/src/vehicle_previewer.c main/src/obj.c main/src/config.c \
    -I main/src -o main/tools/macos/vehicle_previewer \
    -I/opt/homebrew/include -L/opt/homebrew/lib -lraylib \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo
```
