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

    const sources = &.{
        "src/main.c",
        "src/world_generation.c",
        "src/worker.c",
        "src/player.c",
        "src/vec_math.c",
        "src/rendering.c",
        "src/utils.c",
        "src/menu.c",
        "src/clouds.c",
    };

    exe.addCSourceFiles(.{
        .root = b.path("."),
        .files = sources,
        .flags = getCFlags(optimize),
    });

    exe.addIncludePath(b.path("src"));

    exe.linkLibC();
    exe.linkSystemLibrary("raylib");

    switch (target.result.os.tag) {
        .linux => {
            exe.linkSystemLibrary("GL");
            exe.linkSystemLibrary("X11");
            exe.linkSystemLibrary("pthread");
            exe.linkSystemLibrary("dl");
            exe.linkSystemLibrary("rt");
        },
        .macos => {
            exe.linkFramework("OpenGL");
            exe.linkFramework("Cocoa");
            exe.linkFramework("IOKit");
            exe.linkFramework("CoreVideo");
        },
        else => {},
    }

    exe.linkSystemLibrary("m");

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
