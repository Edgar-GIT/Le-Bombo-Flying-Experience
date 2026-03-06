const game_runtime = @import("game.zig");

var g_runtime = game_runtime.GameRuntime{};

//entrada por frame para atualizar estado zig compacto do runtime
pub export fn LBFE_ZigBeginFrame(dt_seconds: f32, overlay_alpha: f32) void {
    g_runtime.beginFrame(dt_seconds, overlay_alpha);
}

//informa ao lado C quando efeitos pesados devem ficar suspensos
pub export fn LBFE_ZigOverlayBlocksFx() u8 {
    return if (g_runtime.canRunHeavyFx()) 0 else 1;
}

//budget de flush de efeitos diferidos para evitar picos
pub export fn LBFE_ZigFxFlushBudget() u8 {
    return g_runtime.currentFlushBudget();
}

//reseta o runtime zig em reinicios/transicoes de estado
pub export fn LBFE_ZigResetRuntime() void {
    g_runtime = .{};
}
