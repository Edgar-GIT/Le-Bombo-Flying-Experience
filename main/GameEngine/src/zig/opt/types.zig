const std = @import("std");

pub const PackedRuntimeState = packed struct {
    combo_level: u2 = 0,
    bomb_count: u3 = 3,
    nuke_used: bool = false,
    nuke_rain_active: bool = false,
    overlay_blocks_fx: bool = false,
    reserved: u8 = 0,
};

//converte hits seguidos para tier de combo compacto
pub fn comboFromHits(hits: u8) u2 {
    if (hits >= 30) return 3;
    if (hits >= 20) return 2;
    if (hits >= 10) return 1;
    return 0;
}

//compacta alpha float [0..1] para 1 byte
pub fn alphaToByte(alpha: f32) u8 {
    const clamped = std.math.clamp(alpha, 0.0, 1.0);
    return @as(u8, @intFromFloat(clamped * 255.0));
}

//compacta dt em milissegundos para 16 bits
pub fn secondsToMillis(dt_seconds: f32) u16 {
    const clamped = std.math.clamp(dt_seconds * 1000.0, 0.0, 65535.0);
    return @as(u16, @intFromFloat(clamped));
}
