const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.createModule(.{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "b3dv",
        .root_module = mod,
    });

    exe.use_llvm = true;

    const sources = &.{ "src/main.c", "src/world_generation.c", "src/worker.c", "src/player.c", "src/vec_math.c", "src/rendering.c", "src/utils.c", "src/menu.c", "src/clouds.c", "src/console.c", "src/aux.c", "src/neutrino_detect.c", "common_utils/src/args.c", "common_utils/src/strings.c" };

    mod.addCSourceFiles(.{
        .files = sources,
        .flags = getCFlags(optimize),
    });

    mod.addIncludePath(b.path("src"));

    mod.linkSystemLibrary("raylib", .{});

    switch (target.result.os.tag) {
        .linux => {
            mod.linkSystemLibrary("GL", .{});
            mod.linkSystemLibrary("X11", .{});
            mod.linkSystemLibrary("pthread", .{});
            mod.linkSystemLibrary("dl", .{});
            mod.linkSystemLibrary("rt", .{});
        },
        .macos => {
            mod.linkFramework("OpenGL", .{});
            mod.linkFramework("Cocoa", .{});
            mod.linkFramework("IOKit", .{});
            mod.linkFramework("CoreVideo", .{});
        },
        else => {},
    }

    mod.linkSystemLibrary("m", .{});
    mod.linkSystemLibrary("z", .{});

    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    if (b.args) |args| run.addArgs(args);

    b.step("run", "Run b3dv").dependOn(&run.step);
}

fn getCFlags(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => &.{
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-O0",
            "-g",
            "-DDEBUG",
        },
        .ReleaseSafe => &.{
            "-std=c11",
            "-O2",
        },
        .ReleaseFast, .ReleaseSmall => &.{
            "-std=c11",
            "-O3",
            "-ffast-math",
        },
    };
}
