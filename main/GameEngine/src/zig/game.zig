const opt_types = @import("opt/types.zig");
const fx_gate = @import("opt/fx_gate.zig");
const cloud_lod = @import("opt/cloud_lod.zig");

pub const RuntimeState = opt_types.PackedRuntimeState;
pub const FxKind = fx_gate.FxKind;
pub const PackedFxEvent = fx_gate.PackedFxEvent;
pub const CloudSphere = cloud_lod.CloudSphere;

pub const GameRuntime = struct {
    frame_index: u32 = 0,
    dt_ms: u16 = 16,
    overlay_alpha_u8: u8 = 0,
    state: RuntimeState = .{},
    fx_flush_budget: u8 = 2,

    //atualiza estado compacto do frame com dados minimos
    pub fn beginFrame(self: *GameRuntime, dt_seconds: f32, overlay_alpha: f32) void {
        self.frame_index +%= 1;
        self.dt_ms = opt_types.secondsToMillis(dt_seconds);
        self.overlay_alpha_u8 = opt_types.alphaToByte(overlay_alpha);
        self.state.overlay_blocks_fx = fx_gate.shouldBlockOverlay(self.overlay_alpha_u8);
    }

    //indica quando updates/desenhos pesados podem correr
    pub fn canRunHeavyFx(self: *const GameRuntime) bool {
        return !self.state.overlay_blocks_fx;
    }

    //define budget de flush de efeitos para evitar spikes
    pub fn currentFlushBudget(self: *const GameRuntime) u8 {
        if (self.state.overlay_blocks_fx) return 0;
        return self.fx_flush_budget;
    }
};

test "combo compact state fits in 2 bytes" {
    const std = @import("std");
    try std.testing.expectEqual(@as(usize, 2), @sizeOf(RuntimeState));
}
