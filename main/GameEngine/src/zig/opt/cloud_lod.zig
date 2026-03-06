pub const CloudSphere = struct {
    x: f32,
    y: f32,
    z: f32,
    radius: f32,
    shade: u8,
};

//gera nuvem leve com 2 esferas cinzentas coladas
pub fn buildGrayPair(center: [3]f32, base_radius: f32, shade: u8) [2]CloudSphere {
    return .{
        .{
            .x = center[0],
            .y = center[1],
            .z = center[2],
            .radius = base_radius,
            .shade = shade,
        },
        .{
            .x = center[0] + (base_radius * 0.62),
            .y = center[1] + (base_radius * 0.18),
            .z = center[2] + (base_radius * 0.22),
            .radius = base_radius * 0.68,
            .shade = shade,
        },
    };
}
