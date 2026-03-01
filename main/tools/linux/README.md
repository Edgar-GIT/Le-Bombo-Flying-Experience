# Linux Tools Output

This folder stores Linux executables for:
- `build_tool`
- `vehicle_previewer`

Build locally from repo root:

```bash
gcc -std=c99 -O2 main/tools/src/build_tool.c -o main/tools/linux/build_tool
gcc -std=c99 -O2 main/tools/src/vehicle_previewer.c main/src/obj.c main/src/config.c \
    -I main/src -o main/tools/linux/vehicle_previewer \
    -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```
