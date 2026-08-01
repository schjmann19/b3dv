const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const client_mod = b.createModule(.{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    const server_mod = b.createModule(.{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    const client_exe = b.addExecutable(.{
        .name = "b3dv-client",
        .root_module = client_mod,
    });
    b.step("b3dv-client", "Build b3dv client").dependOn(&client_exe.step);

    const server_exe = b.addExecutable(.{
        .name = "b3dv-server",
        .root_module = server_mod,
    });
    b.step("b3dv-server", "Build b3dv server").dependOn(&server_exe.step);

    const client_sources = &.{
        "src/client/main.c",
        "src/client/aux.c",
        "src/client/clouds.c",
        "src/client/menu.c",
        "src/client/rendering.c",
        "src/client/neutrino_detect.c",
        "src/common/world_generation.c",
        "src/common/worker.c",
        "src/common/player.c",
        "src/common/game_server.c",
        "src/common/console.c",
        "src/common/vec_math.c",
        "src/common/utils.c",
        "common_utils/src/args.c",
        "common_utils/src/strings.c",
    };

    const server_sources = &.{
        "src/server/main.c",
        "src/common/world_generation.c",
        "src/common/worker.c",
        "src/common/player.c",
        "src/common/game_server.c",
        "src/common/console.c",
        "src/common/vec_math.c",
        "src/common/utils.c",
        "common_utils/src/args.c",
        "common_utils/src/strings.c",
    };

    client_mod.addCSourceFiles(.{
        .files = client_sources,
        .flags = getCFlags(optimize, true),
    });
    server_mod.addCSourceFiles(.{
        .files = server_sources,
        .flags = getCFlags(optimize, false),
    });

    client_mod.addIncludePath(b.path("src"));
    server_mod.addIncludePath(b.path("src"));

    const client_link_libs = [_][]const u8{ "raylib", "m", "z" };
    const server_link_libs = [_][]const u8{ "m", "z" };
    for (client_link_libs) |lib| {
        client_mod.linkSystemLibrary(lib, .{});
    }
    for (server_link_libs) |lib| {
        server_mod.linkSystemLibrary(lib, .{});
    }

    switch (target.result.os.tag) {
        .linux => {
            const client_libs = [_][]const u8{ "GL", "X11", "pthread", "dl", "rt" };
            const server_libs = [_][]const u8{ "pthread", "dl", "rt" };
            for (client_libs) |lib| {
                client_mod.linkSystemLibrary(lib, .{});
            }
            for (server_libs) |lib| {
                server_mod.linkSystemLibrary(lib, .{});
            }
        },
        .macos => {
            const frameworks = [_][]const u8{ "OpenGL", "Cocoa", "IOKit", "CoreVideo" };
            for (frameworks) |framework| {
                client_mod.linkFramework(framework, .{});
            }
        },
        else => {},
    }

    b.installArtifact(client_exe);
    b.installArtifact(server_exe);

    const client_run = b.addRunArtifact(client_exe);
    if (b.args) |args| client_run.addArgs(args);
    b.step("run-client", "Run b3dv-client").dependOn(&client_run.step);

    const server_run = b.addRunArtifact(server_exe);
    if (b.args) |args| server_run.addArgs(args);
    b.step("run-server", "Run b3dv-server").dependOn(&server_run.step);
}

const ClientBuildDefine = "-DCLIENT_BUILD";
const ServerBuildDefine = "-DSERVER_BUILD";

const cflags_debug_client = [_][]const u8{
    "-std=c11",
    "-D_POSIX_C_SOURCE=200112L",
    "-D_GNU_SOURCE",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-O0",
    "-g",
    "-DDEBUG",
    ClientBuildDefine,
};

const cflags_debug_server = [_][]const u8{
    "-std=c11",
    "-D_POSIX_C_SOURCE=200112L",
    "-D_GNU_SOURCE",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-O0",
    "-g",
    "-DDEBUG",
    ServerBuildDefine,
};

const cflags_release_safe_client = [_][]const u8{
    "-std=c11",
    "-D_POSIX_C_SOURCE=200112L",
    "-D_GNU_SOURCE",
    "-O2",
    ClientBuildDefine,
};

const cflags_release_safe_server = [_][]const u8{
    "-std=c11",
    "-D_POSIX_C_SOURCE=200112L",
    "-D_GNU_SOURCE",
    "-O2",
    ServerBuildDefine,
};

const cflags_release_fast_client = [_][]const u8{
    "-std=c11",
    "-D_POSIX_C_SOURCE=200112L",
    "-D_GNU_SOURCE",
    "-O2",
    ClientBuildDefine,
};

const cflags_release_fast_server = [_][]const u8{
    "-std=c11",
    "-D_POSIX_C_SOURCE=200112L",
    "-D_GNU_SOURCE",
    "-O2",
    ServerBuildDefine,
};

fn getCFlags(optimize: std.builtin.OptimizeMode, is_client: bool) []const []const u8 {
    return switch (optimize) {
        .Debug => if (is_client) &cflags_debug_client else &cflags_debug_server,
        .ReleaseSafe => if (is_client) &cflags_release_safe_client else &cflags_release_safe_server,
        .ReleaseFast, .ReleaseSmall => if (is_client) &cflags_release_fast_client else &cflags_release_fast_server,
    };
}
