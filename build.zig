const std = @import("std");
const builtin = @import("builtin");

pub fn build(b: *std.Build) void {
    const setup_script = createSetupCommand(b);

    const game_script = b.addSystemCommand(&.{
        "zig",
        "run",
        "main/GameEngine/src/zig/main.zig",
        "--",
        "--game",
    });
    game_script.step.dependOn(&setup_script.step);
    const preview_script = b.addSystemCommand(&.{
        "zig",
        "run",
        "main/GameEngine/src/zig/main.zig",
        "--",
        "--preview",
    });
    preview_script.step.dependOn(&setup_script.step);
    const all_script = b.addSystemCommand(&.{
        "zig",
        "run",
        "main/GameEngine/src/zig/main.zig",
        "--",
        "--all",
    });
    all_script.step.dependOn(&setup_script.step);
    const engine_script = b.addSystemCommand(&.{
        "zig",
        "run",
        "main/GameEngine/src/zig/engine_build.zig",
    });
    engine_script.step.dependOn(&setup_script.step);

    const setup_step = b.step("setup", "Install/check dependencies and configure paths");
    setup_step.dependOn(&setup_script.step);

    const game_step = b.step("game", "Build the main game binary (LBFE)");
    game_step.dependOn(&game_script.step);

    const preview_step = b.step("preview", "Build the vehicle previewer");
    preview_step.dependOn(&preview_script.step);

    const all_step = b.step("all", "Build the main game and the previewer");
    all_step.dependOn(&all_script.step);

    const engine_step = b.step("engine", "Build the Game Engine editor");
    engine_step.dependOn(&engine_script.step);

    const run_game_cmd = b.addSystemCommand(&.{gameBinaryPath()});
    run_game_cmd.step.dependOn(&game_script.step);
    const run_game_step = b.step("run", "Build and run LBFE");
    run_game_step.dependOn(&run_game_cmd.step);

    const run_engine_cmd = b.addSystemCommand(&.{engineBinaryPath()});
    run_engine_cmd.step.dependOn(&engine_script.step);
    const run_engine_step = b.step("run-engine", "Build and run the Game Engine");
    run_engine_step.dependOn(&run_engine_cmd.step);

    const run_preview_cmd = b.addSystemCommand(&.{previewBinaryPath()});
    run_preview_cmd.step.dependOn(&preview_script.step);
    const run_preview_step = b.step("run-preview", "Build and run the vehicle previewer");
    run_preview_step.dependOn(&run_preview_cmd.step);

    b.default_step.dependOn(&all_script.step);
}

fn createSetupCommand(b: *std.Build) *std.Build.Step.Run {
    return switch (builtin.os.tag) {
        .windows => b.addSystemCommand(&.{
            "powershell",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            "scripts/setup_windows.ps1",
        }),
        else => b.addSystemCommand(&.{
            "sh",
            "scripts/setup.sh",
        }),
    };
}

fn gameBinaryPath() []const u8 {
    return if (builtin.os.tag == .windows) "main/LBFE.exe" else "./main/LBFE";
}

fn engineBinaryPath() []const u8 {
    return if (builtin.os.tag == .windows)
        "main/GameEngine/src/GameEngine.exe"
    else
        "./main/GameEngine/src/GameEngine";
}

fn previewBinaryPath() []const u8 {
    return switch (builtin.os.tag) {
        .windows => "main/build/windows/vehicle_previewer.exe",
        .macos => "./main/build/macos/vehicle_previewer",
        else => "./main/build/linux/vehicle_previewer",
    };
}
