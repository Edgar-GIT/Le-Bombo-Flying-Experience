# Windows Tools Output

This folder stores Windows executables for:
- `build_tool.exe`
- `vehicle_previewer.exe`

Build locally from repo root (MSYS2/MinGW):

```bash
gcc -std=c99 -O2 main/tools/src/build_tool.c -o main/tools/windows/build_tool.exe
gcc -std=c99 -O2 main/tools/src/vehicle_previewer.c main/src/obj.c main/src/config.c \
    -I main/src -o main/tools/windows/vehicle_previewer.exe \
    -I<raylib_include_path> -L<raylib_lib_path> \
    -lraylib -lopengl32 -lgdi32 -lwinmm
```
