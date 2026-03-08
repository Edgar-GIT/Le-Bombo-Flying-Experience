const std = @import("std");
const builtin = @import("builtin");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var args_it = try std.process.argsWithAllocator(allocator);
    defer args_it.deinit();

    _ = args_it.next();

    var build_game = true;
    var build_preview = true;

    while (args_it.next()) |arg| {
        if (std.mem.eql(u8, arg, "--game")) {
            build_preview = false;
        } else if (std.mem.eql(u8, arg, "--preview")) {
            build_game = false;
        } else if (std.mem.eql(u8, arg, "--all")) {
            build_game = true;
            build_preview = true;
        } else if (std.mem.eql(u8, arg, "--help")) {
            printUsage();
            return;
        } else {
            std.debug.print("Unknown argument: {s}\n", .{arg});
            printUsage();
            return error.InvalidArgument;
        }
    }

    const root = try findProjectRoot(allocator);
    defer allocator.free(root);

    try std.posix.chdir(root);

    const platform = platformName();
    try std.fs.cwd().makePath("main/build");

    const platform_dir = try std.fmt.allocPrint(allocator, "main/build/{s}", .{platform});
    defer allocator.free(platform_dir);
    try std.fs.cwd().makePath(platform_dir);

    if (build_game) {
        try buildGame(allocator, platform);
    }
    if (build_preview) {
        try buildPreviewer(allocator, platform);
    }

    std.debug.print("[zig-opt] done ({s})\n", .{platform});
}

fn printUsage() void {
    std.debug.print(
        "Usage: zig run main/GameEngine/src/zig/main.zig -- [--all|--game|--preview]\n",
        .{},
    );
}

fn platformName() []const u8 {
    return switch (builtin.os.tag) {
        .windows => "windows",
        .macos => "macos",
        else => "linux",
    };
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
        if (std.mem.eql(u8, parent, current)) {
            return error.ProjectRootNotFound;
        }

        const next = try allocator.dupe(u8, parent);
        allocator.free(current);
        current = next;
    }
}

fn buildGame(allocator: std.mem.Allocator, _: []const u8) !void {
    const runtime_lib_name = if (builtin.os.tag == .windows)
        "lbfe_zig_runtime.lib"
    else
        "liblbfe_zig_runtime.a";
    const runtime_lib = try std.fmt.allocPrint(
        allocator,
        "main/build/{s}/{s}",
        .{ platformName(), runtime_lib_name },
    );
    defer allocator.free(runtime_lib);

    const emit_arg = try std.fmt.allocPrint(allocator, "-femit-bin={s}", .{runtime_lib});
    defer allocator.free(emit_arg);

    var runtime_args = std.array_list.Managed([]const u8).init(allocator);
    defer runtime_args.deinit();

    try runtime_args.appendSlice(&.{
        "zig",
        "build-lib",
        "main/GameEngine/src/zig/game_api.zig",
        "-O",
        "ReleaseFast",
        "-static",
        "-fstrip",
        "-fPIC",
    });
    try runtime_args.append(emit_arg);
    try runCommand(allocator, runtime_args.items);

    const suffix = if (builtin.os.tag == .windows) ".exe" else "";
    const output = try std.fmt.allocPrint(
        allocator,
        "main/LBFE{s}",
        .{suffix},
    );
    defer allocator.free(output);

    var args = std.array_list.Managed([]const u8).init(allocator);
    defer args.deinit();

    try args.appendSlice(&.{ "zig", "cc" });
    try appendOptimizationFlags(&args);
    try args.appendSlice(&.{ "-DLBFE_USE_ZIG_RUNTIME" });
    try args.appendSlice(&.{
        "main/src/main.c",
        "main/src/game.c",
        "main/src/ui.c",
        "main/src/obj.c",
        "main/src/atacks.c",
        "main/src/screens.c",
        "main/src/config.c",
        "main/src/custom_vehicle.c",
        runtime_lib,
        "-o",
        output,
    });
    try appendPlatformRaylibFlags(&args);

    try runCommand(allocator, args.items);
    _ = stripBinary(allocator, output) catch {};
}

fn buildPreviewer(allocator: std.mem.Allocator, platform: []const u8) !void {
    const suffix = if (builtin.os.tag == .windows) ".exe" else "";
    const output = try std.fmt.allocPrint(
        allocator,
        "main/build/{s}/vehicle_previewer{s}",
        .{ platform, suffix },
    );
    defer allocator.free(output);

    var args = std.array_list.Managed([]const u8).init(allocator);
    defer args.deinit();

    try args.appendSlice(&.{ "zig", "cc" });
    try appendOptimizationFlags(&args);
    try args.appendSlice(&.{
        "main/tools/src/vehicle_previewer.c",
        "main/src/obj.c",
        "main/src/config.c",
        "main/src/custom_vehicle.c",
        "-I",
        "main/src",
        "-o",
        output,
    });
    try appendPlatformRaylibFlags(&args);

    try runCommand(allocator, args.items);
    _ = stripBinary(allocator, output) catch {};
}

fn appendOptimizationFlags(args: *std.array_list.Managed([]const u8)) !void {
    try args.appendSlice(&.{
        "-std=c99",
        "-O3",
        "-DNDEBUG",
        "-flto",
        "-ffast-math",
        "-fstrict-aliasing",
        "-fomit-frame-pointer",
    });
}

fn appendPlatformRaylibFlags(args: *std.array_list.Managed([]const u8)) !void {
    switch (builtin.os.tag) {
        .windows => try args.appendSlice(&.{
            "-lraylib",
            "-lopengl32",
            "-lgdi32",
            "-lwinmm",
        }),
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

fn runCommand(allocator: std.mem.Allocator, argv: []const []const u8) !void {
    std.debug.print("[zig-opt]", .{});
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
        .Exited => |code| {
            if (code != 0) {
                return error.CommandFailed;
            }
        },
        else => return error.CommandFailed,
    }
}

fn stripBinary(allocator: std.mem.Allocator, output: []const u8) !void {
    const stripped_out = try std.fmt.allocPrint(allocator, "{s}.stripped", .{output});
    defer allocator.free(stripped_out);

    //tenta strip via zig objcopy para manter toolchain consistente
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

    //fallback para strip do sistema se objcopy nao estiver disponivel
    const strip_cmd = [_][]const u8{ "strip", output };
    try runCommand(allocator, &strip_cmd);
}
