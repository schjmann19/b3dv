const std = @import("std");

extern fn b3dv_main(argc: c_int, argv: [*][*:0]u8) c_int;

pub fn main(init: std.process.Init.Minimal) void {
    const argc: c_int = @intCast(init.args.vector.len);
    const argv: [*][*:0]u8 = @ptrCast(@constCast(init.args.vector.ptr));
    _ = b3dv_main(argc, argv);
}
