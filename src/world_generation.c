#include "world.h"
#include "vec_math.h"
#include "raylib.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Create and allocate a new infinite world with chunk system
World* world_create(void)
{
    World* world = (World*)malloc(sizeof(World));

    world->chunk_cache.chunks = NULL;
    world->chunk_cache.chunk_count = 0;
    world->chunk_cache.chunk_capacity = 0;
    world->last_loaded_chunk_x = INT32_MAX;
    world->last_loaded_chunk_z = INT32_MAX;
    strncpy(world->world_name, "default", sizeof(world->world_name) - 1);
    world->world_name[sizeof(world->world_name) - 1] = '\0';

    return world;
}

// Free world memory and all chunks
void world_free(World* world)
{
    if (world) {
        if (world->chunk_cache.chunks) {
            free(world->chunk_cache.chunks);
        }
        free(world);
    }
}

// Find or create a chunk in the cache
Chunk* world_get_chunk(World* world, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z)
{
    if (!world) return NULL;

    // Search for existing chunk
    for (int i = 0; i < world->chunk_cache.chunk_count; i++) {
        Chunk* c = &world->chunk_cache.chunks[i];
        if (c->chunk_x == chunk_x && c->chunk_y == chunk_y && c->chunk_z == chunk_z) {
            return c;
        }
    }

    return NULL;
}

// Load or create a chunk
Chunk* world_load_or_create_chunk(World* world, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z)
{
    // Try to find existing chunk
    Chunk* existing = world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (existing) {
        return existing;
    }

    // Expand chunk cache if needed
    if (world->chunk_cache.chunk_count >= world->chunk_cache.chunk_capacity) {
        int new_capacity = (world->chunk_cache.chunk_capacity == 0) ? 16 : world->chunk_cache.chunk_capacity * 2;
        Chunk* new_chunks = (Chunk*)realloc(world->chunk_cache.chunks, new_capacity * sizeof(Chunk));
        if (!new_chunks) return NULL;

        world->chunk_cache.chunks = new_chunks;
        world->chunk_cache.chunk_capacity = new_capacity;
    }

    // Create new chunk
    Chunk* new_chunk = &world->chunk_cache.chunks[world->chunk_cache.chunk_count++];
    new_chunk->chunk_x = chunk_x;
    new_chunk->chunk_y = chunk_y;
    new_chunk->chunk_z = chunk_z;
    new_chunk->loaded = false;

    // Initialize blocks to air
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                new_chunk->blocks[y][z][x].type = BLOCK_AIR;
            }
        }
    }

    // Try to load from disk
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "./worlds/%s_chunks/chunk_%d_%d_%d.chunk",
             world->world_name, chunk_x, chunk_y, chunk_z);

    FILE* file = fopen(filepath, "rb");
    if (file) {
        // Load chunk data from disk
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int x = 0; x < CHUNK_WIDTH; x++) {
                    BlockType block_type;
                    if (fread(&block_type, sizeof(BlockType), 1, file) == 1) {
                        new_chunk->blocks[y][z][x].type = block_type;
                    }
                }
            }
        }
        fclose(file);
        new_chunk->loaded = true;
        printf("[chunk_load] Loaded chunk from %s\n", filepath);
    } else {
        // Chunk doesn't exist on disk - don't auto-generate, return empty chunk
        // The caller (world_load) will handle generation if needed
        printf("[chunk_load] Chunk not found: %s (will stay as air)\n", filepath);
    }

    return new_chunk;
}

// Generate a chunk procedurally
void world_generate_chunk(Chunk* chunk)
{
    if (!chunk) return;

    // Simple flat terrain - just create ground at y=0-5
    // This is much faster than complex procedural generation
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int world_y = chunk->chunk_y * CHUNK_HEIGHT + y;
                // Create a flat ground plane at y=0-5
                if (world_y >= 0 && world_y < 5) {
                    chunk->blocks[y][z][x].type = BLOCK_STONE;
                } else {
                    chunk->blocks[y][z][x].type = BLOCK_AIR;
                }
            }
        }
    }
}

// Set block at world position
void world_set_block(World* world, int x, int y, int z, BlockType type)
{
    // Calculate chunk coordinates
    int32_t chunk_x = x < 0 ? (x - CHUNK_WIDTH + 1) / CHUNK_WIDTH : x / CHUNK_WIDTH;
    int32_t chunk_y = y < 0 ? (y - CHUNK_HEIGHT + 1) / CHUNK_HEIGHT : y / CHUNK_HEIGHT;
    int32_t chunk_z = z < 0 ? (z - CHUNK_DEPTH + 1) / CHUNK_DEPTH : z / CHUNK_DEPTH;

    // Calculate position within chunk
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_y = y - (chunk_y * CHUNK_HEIGHT);
    int local_z = z - (chunk_z * CHUNK_DEPTH);

    // Get or create chunk
    Chunk* chunk = world_load_or_create_chunk(world, chunk_x, chunk_y, chunk_z);
    if (chunk) {
        world_chunk_set_block(chunk, local_x, local_y, local_z, type);
    }
}

// Get block at world position
BlockType world_get_block(World* world, int x, int y, int z)
{
    // Calculate chunk coordinates
    int32_t chunk_x = x < 0 ? (x - CHUNK_WIDTH + 1) / CHUNK_WIDTH : x / CHUNK_WIDTH;
    int32_t chunk_y = y < 0 ? (y - CHUNK_HEIGHT + 1) / CHUNK_HEIGHT : y / CHUNK_HEIGHT;
    int32_t chunk_z = z < 0 ? (z - CHUNK_DEPTH + 1) / CHUNK_DEPTH : z / CHUNK_DEPTH;

    // Calculate position within chunk
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_y = y - (chunk_y * CHUNK_HEIGHT);
    int local_z = z - (chunk_z * CHUNK_DEPTH);

    // Get chunk
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        return BLOCK_AIR;  // Unloaded chunks are treated as air
    }

    return world_chunk_get_block(chunk, local_x, local_y, local_z);
}

// Set block within a chunk
void world_chunk_set_block(Chunk* chunk, int x, int y, int z, BlockType type)
{
    if (x >= 0 && x < CHUNK_WIDTH &&
        y >= 0 && y < CHUNK_HEIGHT &&
        z >= 0 && z < CHUNK_DEPTH) {
        chunk->blocks[y][z][x].type = type;
    }
}

// Get block within a chunk
BlockType world_chunk_get_block(Chunk* chunk, int x, int y, int z)
{
    if (x >= 0 && x < CHUNK_WIDTH &&
        y >= 0 && y < CHUNK_HEIGHT &&
        z >= 0 && z < CHUNK_DEPTH) {
        return chunk->blocks[y][z][x].type;
    }
    return BLOCK_AIR;
}

// Get color for block type
Color world_get_block_color(BlockType type)
{
    switch (type) {
        case BLOCK_STONE:
            return (Color){128, 128, 128, 255};  // Grey
        case BLOCK_AIR:
        default:
            return (Color){0, 0, 0, 0};  // Transparent
    }
}

// Generate a simple prism world - now just generates starting area with chunks
void world_generate_prism(World* world)
{
    // Generate only the chunk at origin for player spawn
    Chunk* chunk = world_load_or_create_chunk(world, 0, 0, 0);
    if (chunk && !chunk->loaded) {
        world_generate_chunk(chunk);
        chunk->loaded = true;
    }
}

// Update loaded chunks based on player position
void world_update_chunks(World* world, Vector3 player_pos)
{
    if (!world) return;

    // Calculate player's chunk coordinates
    int32_t player_chunk_x = (int32_t)floorf(player_pos.x / CHUNK_WIDTH);
    int32_t player_chunk_y = (int32_t)floorf(player_pos.y / CHUNK_HEIGHT);
    int32_t player_chunk_z = (int32_t)floorf(player_pos.z / CHUNK_DEPTH);

    // Only update if player moved to a different chunk
    if (player_chunk_x == world->last_loaded_chunk_x &&
        player_chunk_y == world->last_loaded_chunk_y &&
        player_chunk_z == world->last_loaded_chunk_z) {
        return;
    }

    world->last_loaded_chunk_x = player_chunk_x;
    world->last_loaded_chunk_y = player_chunk_y;
    world->last_loaded_chunk_z = player_chunk_z;

    // Load chunks within load distance
    int load_dist = CHUNK_LOAD_DISTANCE;
    for (int cx = player_chunk_x - load_dist; cx <= player_chunk_x + load_dist; cx++) {
        for (int cy = player_chunk_y - 1; cy <= player_chunk_y + 1; cy++) {
            for (int cz = player_chunk_z - load_dist; cz <= player_chunk_z + load_dist; cz++) {
                Chunk* chunk = world_load_or_create_chunk(world, cx, cy, cz);
                if (chunk && !chunk->loaded) {
                    // Generate this chunk procedurally if it wasn't loaded from disk
                    world_generate_chunk(chunk);
                    chunk->loaded = true;
                }
            }
        }
    }

    // Unload chunks that are too far away
    int unload_dist = CHUNK_LOAD_DISTANCE * 2;
    int i = 0;
    while (i < world->chunk_cache.chunk_count) {
        Chunk* chunk = &world->chunk_cache.chunks[i];
        int dx = chunk->chunk_x - player_chunk_x;
        int dy = chunk->chunk_y - player_chunk_y;
        int dz = chunk->chunk_z - player_chunk_z;

        // Check if chunk is beyond unload distance
        if (dx*dx + dz*dz > unload_dist*unload_dist || dy > unload_dist || dy < -unload_dist) {
            // Remove chunk from cache (swap with last)
            if (i < world->chunk_cache.chunk_count - 1) {
                world->chunk_cache.chunks[i] = world->chunk_cache.chunks[world->chunk_cache.chunk_count - 1];
            }
            world->chunk_cache.chunk_count--;
        } else {
            i++;
        }
    }
}

// Create a player
Player* player_create(float x, float y, float z)
{
    Player* player = (Player*)malloc(sizeof(Player));
    player->position = (Vector3){ x, y, z };
    player->velocity = (Vector3){ 0, 0, 0 };
    player->on_ground = false;
    player->jump_used = false;
    return player;
}

// Free player memory
void player_free(Player* player)
{
    if (player) {
        free(player);
    }
}

// Handle player movement input
void player_move_input(Player* player, Vector3 forward, Vector3 right, float dt)
{
    (void)dt;  // unused

    // Extract horizontal (XZ) component of right vector and normalize it.
    // Then derive a stable horizontal forward from world up and right to avoid issues when
    // the camera is looking straight up/down (forward XZ component near zero).
    Vector3 right_horiz = (Vector3){ right.x, 0, right.z };
    float right_len = sqrtf(right_horiz.x * right_horiz.x + right_horiz.y * right_horiz.y + right_horiz.z * right_horiz.z);
    if (right_len < 1e-6f) {
        // Fallback: if right is degenerate, use world X axis
        right_horiz = (Vector3){ 1.0f, 0.0f, 0.0f };
    } else {
        right_horiz.x /= right_len;
        right_horiz.y /= right_len;
        right_horiz.z /= right_len;
    }

    // Derive forward_horiz as cross(world_up, right_horiz) which gives a horizontal forward
    // consistent with yaw even when the camera is looking vertically.
    Vector3 forward_horiz = vec3_cross((Vector3){0, 1, 0}, right_horiz);
    forward_horiz = vec3_normalize(forward_horiz);

    Vector3 move = { 0, 0, 0 };

    if (IsKeyDown(KEY_W)) {
        move = vec3_add(move, vec3_scale(forward_horiz, PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_S)) {
        move = vec3_add(move, vec3_scale(forward_horiz, -PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_D)) {
        move = vec3_add(move, vec3_scale(right_horiz, PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_A)) {
        move = vec3_add(move, vec3_scale(right_horiz, -PLAYER_SPEED));
    }

    // Normalize movement to prevent diagonal speedup
    float move_len = sqrtf(move.x * move.x + move.z * move.z);
    if (move_len > PLAYER_SPEED) {
        float scale = PLAYER_SPEED / move_len;
        move.x *= scale;
        move.z *= scale;
    }

    if (IsKeyDown(KEY_SPACE)) {
        if (player->on_ground && !player->jump_used) {
            player->velocity.y = JUMP_FORCE;
            player->on_ground = false;
            player->jump_used = true;
        }
    } else {
        player->jump_used = false;
    }

    player->velocity.x = move.x;
    player->velocity.z = move.z;
}

// Check collision with a sphere
bool world_check_collision_sphere(World* world, Vector3 pos, float radius)
{
    int min_x = (int)floorf(pos.x - radius - 1.0f);
    int max_x = (int)ceilf(pos.x + radius + 1.0f);
    int min_y = (int)floorf(pos.y - radius - 1.0f);
    int max_y = (int)ceilf(pos.y + radius + 1.0f);
    int min_z = (int)floorf(pos.z - radius - 1.0f);
    int max_z = (int)ceilf(pos.z + radius + 1.0f);

    for (int y = min_y; y <= max_y; y++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                BlockType block = world_get_block(world, x, y, z);
                if (block != BLOCK_AIR) {
                    float block_min_x = (float)x;
                    float block_max_x = (float)(x + 1);
                    float block_min_y = (float)y;
                    float block_max_y = (float)(y + 1);
                    float block_min_z = (float)z;
                    float block_max_z = (float)(z + 1);

                    float closest_x = (pos.x < block_min_x) ? block_min_x : (pos.x > block_max_x) ? block_max_x : pos.x;
                    float closest_y = (pos.y < block_min_y) ? block_min_y : (pos.y > block_max_y) ? block_max_y : pos.y;
                    float closest_z = (pos.z < block_min_z) ? block_min_z : (pos.z > block_max_z) ? block_max_z : pos.z;

                    float dx = pos.x - closest_x;
                    float dy = pos.y - closest_y;
                    float dz = pos.z - closest_z;
                    float dist_sq = dx*dx + dy*dy + dz*dz;

                    if (dist_sq < radius * radius) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Check collision for a capsule (pill shape) - checks multiple points along vertical height
bool world_check_collision_capsule(World* world, Vector3 center_pos, float height, float radius)
{
    // Sample collision at 5 points along the player's height
    int num_samples = 5;
    float sample_spacing = height / (float)(num_samples - 1);

    for (int i = 0; i < num_samples; i++) {
        float offset_y = -height / 2.0f + i * sample_spacing;
        Vector3 sample_pos = center_pos;
        sample_pos.y += offset_y;

        if (world_check_collision_sphere(world, sample_pos, radius)) {
            return true;
        }
    }

    return false;
}

// Update player physics
void player_update(Player* player, World* world, float dt)
{
    player->velocity.y -= GRAVITY * dt;

    if (player->velocity.y < -50.0f) {
        player->velocity.y = -50.0f;
    }

    Vector3 new_pos = (Vector3){
        player->position.x + player->velocity.x * dt,
        player->position.y + player->velocity.y * dt,
        player->position.z + player->velocity.z * dt
    };

    // Use capsule collision (0.7 blocks wide = 0.35 radius)
    if (!world_check_collision_capsule(world, new_pos, PLAYER_HEIGHT, 0.35f)) {
        player->position = new_pos;
        player->on_ground = false;
    } else {
        Vector3 slide_pos = player->position;

        Vector3 test_x = (Vector3){
            player->position.x + player->velocity.x * dt,
            player->position.y,
            player->position.z
        };
        if (!world_check_collision_capsule(world, test_x, PLAYER_HEIGHT, 0.35f)) {
            slide_pos.x = test_x.x;
        }

        Vector3 test_y = (Vector3){
            slide_pos.x,
            player->position.y + player->velocity.y * dt,
            player->position.z
        };
        if (!world_check_collision_capsule(world, test_y, PLAYER_HEIGHT, 0.35f)) {
            slide_pos.y = test_y.y;
        } else {
            if (player->velocity.y < 0) {
                player->on_ground = true;
                player->jump_used = false;
            }
            player->velocity.y = 0;
        }

        Vector3 test_z = (Vector3){
            slide_pos.x,
            slide_pos.y,
            player->position.z + player->velocity.z * dt
        };
        if (!world_check_collision_capsule(world, test_z, PLAYER_HEIGHT, 0.35f)) {
            slide_pos.z = test_z.z;
        }

        player->position = slide_pos;

        Vector3 below = (Vector3){ player->position.x, player->position.y - 0.1f, player->position.z };
        if (world_check_collision_capsule(world, below, PLAYER_HEIGHT, 0.35f)) {
            player->on_ground = true;
            player->velocity.y = 0;
            player->jump_used = false;
        }
    }
}

// Initialize worlds folder if it doesn't exist
void world_system_init(void)
{
    #ifdef _WIN32
        system("if not exist worlds mkdir worlds");
        system("if not exist worlds\\default_chunks mkdir worlds\\default_chunks");
    #else
        system("mkdir -p ./worlds/default_chunks");
    #endif
}

// Save world to files (chunks)
bool world_save(World* world, const char* world_name)
{
    if (!world || !world_name) return false;

    world_system_init();

    // Save each loaded chunk
    for (int i = 0; i < world->chunk_cache.chunk_count; i++) {
        Chunk* chunk = &world->chunk_cache.chunks[i];

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "./worlds/%s_chunks/chunk_%d_%d_%d.chunk",
                 world_name, chunk->chunk_x, chunk->chunk_y, chunk->chunk_z);

        #ifdef _WIN32
            // Create directory if needed
            char mkdir_cmd[512];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "if not exist worlds\\%s_chunks mkdir worlds\\%s_chunks", world_name, world_name);
            system(mkdir_cmd);
        #else
            char mkdir_cmd[512];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p ./worlds/%s_chunks", world_name);
            system(mkdir_cmd);
        #endif

        FILE* file = fopen(filepath, "wb");
        if (!file) continue;

        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int x = 0; x < CHUNK_WIDTH; x++) {
                    BlockType block_type = chunk->blocks[y][z][x].type;
                    fwrite(&block_type, sizeof(BlockType), 1, file);
                }
            }
        }

        fclose(file);
    }

    return true;
}

// Load world from files (chunks)
bool world_load(World* world, const char* world_name)
{
    if (!world || !world_name) return false;

    printf("[world_load] Loading world '%s'...\n", world_name);
    printf("[world_load] Clearing chunk cache...\n");

    // Clear existing chunks first
    if (world->chunk_cache.chunks) {
        free(world->chunk_cache.chunks);
        world->chunk_cache.chunks = NULL;
        world->chunk_cache.chunk_count = 0;
        world->chunk_cache.chunk_capacity = 0;
    }

    // Set the world name so chunk loading uses the correct directory
    strncpy(world->world_name, world_name, sizeof(world->world_name) - 1);
    world->world_name[sizeof(world->world_name) - 1] = '\0';
    printf("[world_load] World name set to: '%s'\n", world->world_name);

    // Reset chunk loading tracker
    world->last_loaded_chunk_x = INT32_MAX;
    world->last_loaded_chunk_y = INT32_MAX;
    world->last_loaded_chunk_z = INT32_MAX;

    // Try to load initial chunks from disk
    // Start from origin chunk and nearby chunks
    int load_dist = CHUNK_LOAD_DISTANCE;
    bool any_chunk_loaded = false;

    printf("[world_load] Attempting to load chunks around origin with distance %d...\n", load_dist);

    for (int cx = -load_dist; cx <= load_dist; cx++) {
        for (int cy = -1; cy <= 1; cy++) {
            for (int cz = -load_dist; cz <= load_dist; cz++) {
                Chunk* chunk = world_load_or_create_chunk(world, cx, cy, cz);
                if (chunk && chunk->loaded) {
                    any_chunk_loaded = true;
                    printf("[world_load] Loaded chunk at (%d, %d, %d)\n", cx, cy, cz);
                }
            }
        }
    }

    printf("[world_load] Chunks loaded from disk: %s\n", any_chunk_loaded ? "yes" : "no");

    // If no chunks exist on disk, generate the starting area
    if (!any_chunk_loaded) {
        printf("[world_load] No saved chunks found for world '%s'. Generating new world...\n", world_name);
        world_generate_prism(world);
    } else {
        printf("[world_load] Successfully loaded world '%s'. Total chunks: %d\n", world_name, world->chunk_cache.chunk_count);
    }

    return true;
}
