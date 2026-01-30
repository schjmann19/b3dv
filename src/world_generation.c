#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "world.h"
#include "vec_math.h"
#include "raylib.h"

// Simple fast terrain height - no expensive noise, just sinusoidal waves
static float terrain_height(float x, float z)
{
    // Use simple sin/cos for very fast terrain generation
    float h1 = sinf(x * 0.1f) * cosf(z * 0.1f) * 8.0f;
    float h2 = sinf(x * 0.05f) * cosf(z * 0.05f) * 6.0f;
    return h1 + h2 + 10.0f;  // Base height of 10
}

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
    new_chunk->generated = false;

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
        new_chunk->generated = true;  // Loaded chunks are already complete
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

    // Generate terrain with mounds and valleys using noise
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            // Calculate world position
            int world_x = chunk->chunk_x * CHUNK_WIDTH + x;
            int world_z = chunk->chunk_z * CHUNK_DEPTH + z;

            // Get height at this position using noise function
            float height = terrain_height((float)world_x, (float)world_z);
            int terrain_height_blocks = (int)height + 5;  // Base offset of 5

            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int world_y = chunk->chunk_y * CHUNK_HEIGHT + y;
                // Fill blocks below terrain height
                if (world_y < terrain_height_blocks) {
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
    if (chunk && !chunk->generated) {
        world_generate_chunk(chunk);
        chunk->loaded = true;
        chunk->generated = true;
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

    printf("[chunk_update] Player at (%.1f, %.1f, %.1f) → chunk (%d, %d, %d)\n",
           player_pos.x, player_pos.y, player_pos.z, player_chunk_x, player_chunk_y, player_chunk_z);

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
                    // Generate this chunk procedurally
                    world_generate_chunk(chunk);
                    chunk->loaded = true;
                    chunk->generated = true;
                    printf("[chunk_update] Generated chunk (%d, %d, %d)\n", cx, cy, cz);
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
