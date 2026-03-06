const std = @import("std");

pub const FxKind = enum(u2) {
    explosion = 0,
    nuke = 1,
    smoke = 2,
    trail = 3,
};

pub const PackedFxEvent = packed struct {
    kind: FxKind,
    x_q: i20,
    y_q: i20,
    z_q: i20,
};

pub const OverlayBlockAlphaU8: u8 = 140;
const QuantScale: f32 = 10.0;

//bloqueia fx quando o overlay esta realmente a tapar o jogo
pub fn shouldBlockOverlay(overlay_alpha_u8: u8) bool {
    return overlay_alpha_u8 >= OverlayBlockAlphaU8;
}

//compacta posiçao float para inteiro de 20 bits com escala fixa
fn quantizePos(value: f32) i20 {
    const scaled = value * QuantScale;
    const clamped = std.math.clamp(scaled, -524287.0, 524287.0);
    const as_i32 = @as(i32, @intFromFloat(clamped));
    return @as(i20, @intCast(as_i32));
}

//descompacta posiçao de volta para float
fn dequantizePos(value: i20) f32 {
    return @as(f32, @floatFromInt(value)) / QuantScale;
}

//cria evento compacto para fila de fx diferidos
pub fn makeEvent(kind: FxKind, x: f32, y: f32, z: f32) PackedFxEvent {
    return .{
        .kind = kind,
        .x_q = quantizePos(x),
        .y_q = quantizePos(y),
        .z_q = quantizePos(z),
    };
}

//expande posiçao compactada para uso no renderer/fisica
pub fn unpackPosition(event: PackedFxEvent) [3]f32 {
    return .{
        dequantizePos(event.x_q),
        dequantizePos(event.y_q),
        dequantizePos(event.z_q),
    };
}

pub fn FxRing(comptime capacity: comptime_int) type {
    return struct {
        const Self = @This();

        events: [capacity]PackedFxEvent = undefined,
        head: u16 = 0,
        tail: u16 = 0,
        count: u16 = 0,

        //guarda evento novo e falha apenas se a fila encher
        pub fn push(self: *Self, event: PackedFxEvent) bool {
            if (@as(usize, self.count) >= capacity) return false;

            self.events[@as(usize, self.tail)] = event;
            self.tail = @as(u16, @intCast((@as(usize, self.tail) + 1) % capacity));
            self.count += 1;
            return true;
        }

        //retira evento mais antigo da fila
        pub fn pop(self: *Self) ?PackedFxEvent {
            if (self.count == 0) return null;

            const event = self.events[@as(usize, self.head)];
            self.head = @as(u16, @intCast((@as(usize, self.head) + 1) % capacity));
            self.count -= 1;
            return event;
        }

        //limpa a fila sem realocar memoria
        pub fn clear(self: *Self) void {
            self.head = 0;
            self.tail = 0;
            self.count = 0;
        }
    };
}
