const std = @import("std");
const builtin = @import("builtin");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();
    var temp_arena = std.heap.ArenaAllocator.init(allocator);
    defer temp_arena.deinit();
    const temp_allocator = temp_arena.allocator();

    const root = try findProjectRoot(allocator);
    defer allocator.free(root);
    try std.posix.chdir(root);

    try std.fs.cwd().makePath("main/GameEngine/src");

    const suffix = if (builtin.os.tag == .windows) ".exe" else "";
    const output = try std.fmt.allocPrint(
        allocator,
        "main/GameEngine/src/GameEngine{s}",
        .{suffix},
    );
    defer allocator.free(output);

    var args = std.array_list.Managed([]const u8).init(allocator);
    defer args.deinit();

    try args.appendSlice(&.{ "zig", "c++" });
    try appendOptimizationFlags(&args);
    // Compile all Game Engine modules into a single optimized executable.
    try args.appendSlice(&.{
        "main/GameEngine/src/main/main.cpp",
        "main/GameEngine/src/main/config.cpp",
        "main/GameEngine/src/main/gui_manager.cpp",
        "main/GameEngine/src/main/gui_workflow.cpp",
        "main/GameEngine/src/main/gui_shotpoints.cpp",
        "main/GameEngine/src/main/main_gui.cpp",
        "main/GameEngine/src/main/vehicle_export.cpp",
        "main/GameEngine/src/main/vehicle_import.cpp",
        "main/GameEngine/src/main/persistence.cpp",
        "main/GameEngine/src/main/persistence_io.cpp",
        "main/GameEngine/src/main/persistence_picker.cpp",
        "main/GameEngine/src/main/workspace_tabs.cpp",
        "main/GameEngine/src/main/piece_library.cpp",
        "-I",
        "main/GameEngine/src/include",
        "-o",
        output,
    });
    try appendPlatformRaylibFlags(temp_allocator, &args);
    try appendPlatformLinkerOptimizationFlags(&args);

    try runCommand(allocator, args.items);
    _ = stripBinary(allocator, output) catch {};

    std.debug.print("[engine-build] done -> {s}\n", .{output});
}

fn findProjectRoot(allocator: std.mem.Allocator) ![]u8 {
    var current = try std.fs.cwd().realpathAlloc(allocator, ".");
    errdefer allocator.free(current);

    while (true) {
        const marker = try std.fmt.allocPrint(allocator, "{s}/main/src/main.c", .{current});
        defer allocator.free(marker);

        if (std.fs.openFileAbsolute(marker, .{})) |file| {
            file.close();
            return current;
        } else |_| {}

        const parent = std.fs.path.dirname(current) orelse return error.ProjectRootNotFound;
        if (std.mem.eql(u8, parent, current)) return error.ProjectRootNotFound;

        const next = try allocator.dupe(u8, parent);
        allocator.free(current);
        current = next;
    }
}

fn appendOptimizationFlags(args: *std.array_list.Managed([]const u8)) !void {
    try args.appendSlice(&.{
        "-std=c++17",
        "-O3",
        "-DNDEBUG",
        "-flto",
        "-ffast-math",
        "-fstrict-aliasing",
        "-fomit-frame-pointer",
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-math-errno",
        "-fno-trapping-math",
        "-fno-semantic-interposition",
    });
}

fn appendPlatformRaylibFlags(allocator: std.mem.Allocator, args: *std.array_list.Managed([]const u8)) !void {
    switch (builtin.os.tag) {
        .windows => {
            try appendWindowsRaylibPaths(allocator, args);
            try args.appendSlice(&.{
                "-lraylib",
                "-lglfw3dll",
                "-lopengl32",
                "-lgdi32",
                "-lwinmm",
            });
        },
        .macos => try args.appendSlice(&.{
            "-I/opt/homebrew/include",
            "-L/opt/homebrew/lib",
            "-lraylib",
            "-framework",
            "OpenGL",
            "-framework",
            "Cocoa",
            "-framework",
            "IOKit",
            "-framework",
            "CoreAudio",
            "-framework",
            "CoreVideo",
        }),
        else => try args.appendSlice(&.{
            "-lraylib",
            "-lGL",
            "-lm",
            "-lpthread",
            "-ldl",
            "-lrt",
            "-lX11",
        }),
    }
}

fn appendWindowsRaylibPaths(allocator: std.mem.Allocator, args: *std.array_list.Managed([]const u8)) !void {
    const include_env = std.process.getEnvVarOwned(allocator, "RAYLIB_INCLUDE_DIR") catch null;
    const lib_env = std.process.getEnvVarOwned(allocator, "RAYLIB_LIB_DIR") catch null;

    if (include_env != null and lib_env != null) {
        try appendWindowsIncludeDirIfExists(allocator, args, include_env.?);
        try appendWindowsLibDirIfExists(allocator, args, lib_env.?);
        return;
    }

    const root_env = std.process.getEnvVarOwned(allocator, "RAYLIB_ROOT") catch null;
    if (root_env) |root| {
        const include_dir = try std.fmt.allocPrint(allocator, "{s}/include", .{root});
        const lib_dir = try std.fmt.allocPrint(allocator, "{s}/lib", .{root});
        try appendWindowsIncludeDirIfExists(allocator, args, include_dir);
        try appendWindowsLibDirIfExists(allocator, args, lib_dir);
        return;
    }

    try appendWindowsIncludeDirIfExists(allocator, args, "C:/msys64/ucrt64/include");
    try appendWindowsLibDirIfExists(allocator, args, "C:/msys64/ucrt64/lib");
}

fn appendWindowsLibDirIfExists(
    allocator: std.mem.Allocator,
    args: *std.array_list.Managed([]const u8),
    lib_dir: []const u8,
) !void {
    if (!dirExistsAbsolute(lib_dir)) return;
    const flag = try std.fmt.allocPrint(allocator, "-L{s}", .{lib_dir});
    try args.append(flag);
}

fn appendWindowsIncludeDirIfExists(
    allocator: std.mem.Allocator,
    args: *std.array_list.Managed([]const u8),
    include_dir: []const u8,
) !void {
    if (!dirExistsAbsolute(include_dir)) return;
    const flag = try std.fmt.allocPrint(allocator, "-I{s}", .{include_dir});
    try args.append(flag);
}

fn dirExistsAbsolute(path: []const u8) bool {
    var dir = std.fs.openDirAbsolute(path, .{}) catch return false;
    dir.close();
    return true;
}

fn appendPlatformLinkerOptimizationFlags(args: *std.array_list.Managed([]const u8)) !void {
    switch (builtin.os.tag) {
        .windows => try args.appendSlice(&.{
            "-Wl,--gc-sections",
        }),
        .macos => try args.appendSlice(&.{
            "-Wl,-dead_strip",
        }),
        else => try args.appendSlice(&.{
            "-Wl,--gc-sections",
            "-Wl,--as-needed",
        }),
    }
}

fn runCommand(allocator: std.mem.Allocator, argv: []const []const u8) !void {
    std.debug.print("[engine-build]", .{});
    for (argv) |part| {
        std.debug.print(" {s}", .{part});
    }
    std.debug.print("\n", .{});

    var child = std.process.Child.init(argv, allocator);
    child.stdin_behavior = .Ignore;
    child.stdout_behavior = .Inherit;
    child.stderr_behavior = .Inherit;

    const term = try child.spawnAndWait();
    switch (term) {
        .Exited => |code| if (code != 0) return error.CommandFailed,
        else => return error.CommandFailed,
    }
}

fn stripBinary(allocator: std.mem.Allocator, output: []const u8) !void {
    const stripped_out = try std.fmt.allocPrint(allocator, "{s}.stripped", .{output});
    defer allocator.free(stripped_out);

    const objcopy_cmd = [_][]const u8{
        "zig",
        "objcopy",
        "--strip-all",
        output,
        stripped_out,
    };
    if (runCommand(allocator, &objcopy_cmd)) |_| {
        std.fs.cwd().deleteFile(output) catch {};
        try std.fs.cwd().rename(stripped_out, output);
        return;
    } else |_| {
        std.fs.cwd().deleteFile(stripped_out) catch {};
    }

    const strip_cmd = [_][]const u8{ "strip", output };
    try runCommand(allocator, &strip_cmd);
}
