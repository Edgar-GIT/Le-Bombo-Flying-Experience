const std = @import("std");
const builtin = @import("builtin");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

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
    try args.appendSlice(&.{
        "main/GameEngine/src/main/main.cpp",
        "main/GameEngine/src/main/config.cpp",
        "main/GameEngine/src/main/gui.cpp",
        "-I",
        "main/GameEngine/src/include",
        "-o",
        output,
    });
    try appendPlatformRaylibFlags(&args);
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
