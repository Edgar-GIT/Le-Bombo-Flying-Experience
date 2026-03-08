#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#define PATH_SEP "\\"
#define PATH_MAX 260
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <sys/param.h>
#define MKDIR(path) mkdir(path, 0755)
#define PATH_SEP "/"
#endif

static const char *get_platform(void) {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

static int ensure_dir(const char *path) {
    if (MKDIR(path) == 0) return 0;
    return 0;
}

static int run_cmd(const char *cmd) {
    printf("[build_tool] %s\n", cmd);
    return system(cmd);
}

static int has_zig(void) {
#if defined(_WIN32)
    int rc = system("zig version >NUL 2>&1");
#else
    int rc = system("zig version >/dev/null 2>&1");
#endif
    return rc == 0;
}

static int run_zig_pipeline(const char *mode) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "zig run main/GameEngine/src/zig/main.zig -- %s",
             mode);
    return run_cmd(cmd);
}

static int build_game(const char *platform) {
    char cmd[4096];
    char out_file[512];

    // Output goes directly to main/ so relative paths work
#if defined(_WIN32)
    snprintf(out_file, sizeof(out_file), "main%sLBFE.exe", PATH_SEP);
    snprintf(cmd, sizeof(cmd),
             "gcc -std=c99 -O2 "
             "main/src/main.c main/src/game.c main/src/ui.c main/src/obj.c "
             "main/src/atacks.c main/src/screens.c main/src/config.c main/src/custom_vehicle.c "
             "-o \"%s\" "
             "-I<raylib_include_path> -L<raylib_lib_path> "
             "-lraylib -lopengl32 -lgdi32 -lwinmm",
             out_file);
#elif defined(__APPLE__)
    snprintf(out_file, sizeof(out_file), "main%sLBFE", PATH_SEP);
    snprintf(cmd, sizeof(cmd),
             "clang -std=c99 -O2 "
             "main/src/main.c main/src/game.c main/src/ui.c main/src/obj.c "
             "main/src/atacks.c main/src/screens.c main/src/config.c main/src/custom_vehicle.c "
             "-o \"%s\" "
             "-I/opt/homebrew/include -L/opt/homebrew/lib -lraylib "
             "-framework OpenGL -framework Cocoa -framework IOKit "
             "-framework CoreAudio -framework CoreVideo",
             out_file);
#else
    snprintf(out_file, sizeof(out_file), "main%sLBFE", PATH_SEP);
    snprintf(cmd, sizeof(cmd),
             "gcc -std=c99 -O2 "
             "main/src/main.c main/src/game.c main/src/ui.c main/src/obj.c "
             "main/src/atacks.c main/src/screens.c main/src/config.c main/src/custom_vehicle.c "
             "-o \"%s\" "
             "-lraylib -lGL -lm -lpthread -ldl -lrt -lX11",
             out_file);
#endif

    return run_cmd(cmd);
}

static int build_previewer(const char *platform) {
    char cmd[4096];
    char out_dir[512];
    char out_file[512];

    snprintf(out_dir, sizeof(out_dir), "main%sbuild%s%s", PATH_SEP, PATH_SEP, platform);
    ensure_dir("main");
    ensure_dir("main/build");
    ensure_dir(out_dir);

#if defined(_WIN32)
    snprintf(out_file, sizeof(out_file), "%s%svehicle_previewer.exe", out_dir, PATH_SEP);
    snprintf(cmd, sizeof(cmd),
             "gcc -std=c99 -O2 "
             "main/tools/src/vehicle_previewer.c main/src/obj.c main/src/config.c main/src/custom_vehicle.c "
             "-I main/src -o \"%s\" "
             "-I<raylib_include_path> -L<raylib_lib_path> "
             "-lraylib -lopengl32 -lgdi32 -lwinmm",
             out_file);
#elif defined(__APPLE__)
    snprintf(out_file, sizeof(out_file), "%s%svehicle_previewer", out_dir, PATH_SEP);
    snprintf(cmd, sizeof(cmd),
             "clang -std=c99 -O2 "
             "main/tools/src/vehicle_previewer.c main/src/obj.c main/src/config.c main/src/custom_vehicle.c "
             "-I main/src -o \"%s\" "
             "-I/opt/homebrew/include -L/opt/homebrew/lib -lraylib "
             "-framework OpenGL -framework Cocoa -framework IOKit "
             "-framework CoreAudio -framework CoreVideo",
             out_file);
#else
    snprintf(out_file, sizeof(out_file), "%s%svehicle_previewer", out_dir, PATH_SEP);
    snprintf(cmd, sizeof(cmd),
             "gcc -std=c99 -O2 "
             "main/tools/src/vehicle_previewer.c main/src/obj.c main/src/config.c main/src/custom_vehicle.c "
             "-I main/src -o \"%s\" "
             "-lraylib -lGL -lm -lpthread -ldl -lrt -lX11",
             out_file);
#endif

    return run_cmd(cmd);
}

int main(int argc, char **argv) {
    // Change to project root directory automatically
#ifdef _WIN32
    char exe[MAX_PATH];
    GetModuleFileName(NULL, exe, MAX_PATH);
    char *dir = exe;
    for (int i = 0; i < 3; i++) {
        char *sep = strrchr(dir, '\\');
        if (sep) *sep = '\0';
    }
    _chdir(dir);
#else
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';
        char exe_copy[PATH_MAX];
        strncpy(exe_copy, exe, sizeof(exe_copy) - 1);
        char *dir = dirname(exe_copy);     // .../main/tools/linux
        dir = dirname(dir);                // .../main/tools
        dir = dirname(dir);                // .../main
        dir = dirname(dir);                // project root
        if (chdir(dir) != 0) {
            fprintf(stderr, "Warning: failed to change to project root\n");
        }
    }
#endif

    const char *platform = get_platform();
    int build_game_flag = 1;
    int build_preview_flag = 1;
    int run_preview = 0;
    int no_zig = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game") == 0) {
            build_preview_flag = 0;
        } else if (strcmp(argv[i], "--preview") == 0) {
            build_game_flag = 0;
        } else if (strcmp(argv[i], "--all") == 0) {
            build_game_flag = 1;
            build_preview_flag = 1;
        } else if (strcmp(argv[i], "--run-preview") == 0) {
            run_preview = 1;
            build_game_flag = 0;
            build_preview_flag = 1;
        } else if (strcmp(argv[i], "--no-zig") == 0) {
            no_zig = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: build_tool [--all|--game|--preview|--run-preview|--no-zig]\n");
            return 0;
        }
    }

    int rc = 0;
    int zig_ok = 0;
    if (!no_zig && has_zig()) {
        const char *zig_mode = "--all";
        if (build_game_flag && !build_preview_flag) zig_mode = "--game";
        if (!build_game_flag && build_preview_flag) zig_mode = "--preview";
        rc = run_zig_pipeline(zig_mode);
        if (rc == 0) {
            zig_ok = 1;
            printf("[build_tool] Zig pipeline complete.\n");
        } else {
            printf("[build_tool] Zig pipeline failed, falling back to C compiler build.\n");
        }
    } else if (!no_zig) {
        printf("[build_tool] Zig not found, using C compiler build.\n");
    }

    if (build_game_flag) {
        if (!zig_ok) {
            rc = build_game(platform);
            if (rc != 0) return rc;
        }
    }
    if (build_preview_flag) {
        if (!zig_ok) {
            rc = build_previewer(platform);
            if (rc != 0) return rc;
        }
    }

    if (run_preview) {
#if defined(_WIN32)
        rc = run_cmd("main\\build\\windows\\vehicle_previewer.exe");
#elif defined(__APPLE__)
        rc = run_cmd("./main/build/macos/vehicle_previewer");
#else
        rc = run_cmd("./main/build/linux/vehicle_previewer");
#endif
    }

    printf("[build_tool] Done for platform: %s\n", platform);
    return rc;
}
