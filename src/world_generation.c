#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "world.h"
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

    // Initialize texture cache
    world->textures.textures_loaded = false;
    world->textures.grass_texture = (Texture2D){0};
    world->textures.dirt_texture = (Texture2D){0};
    world->textures.stone_texture = (Texture2D){0};

    strncpy(world->world_name, "default", sizeof(world->world_name) - 1);
    world->world_name[sizeof(world->world_name) - 1] = '\0';

    return world;
}

// Free world memory and all chunks
void world_free(World* world)
{
    if (world) {
        // Don't unload textures - they're shared across all worlds
        // and will be unloaded when the application closes
        free(world);
    }
}

// Load textures for blocks from ./assets/textures/blocks/
void world_load_textures(World* world)
{
    if (!world || world->textures.textures_loaded) return;

    // Try to load grass texture
    world->textures.grass_texture = LoadTexture("./assets/textures/blocks/grass.png");
    printf("[textures] grass texture id: %d\n", world->textures.grass_texture.id);

    // Try to load dirt texture
    world->textures.dirt_texture = LoadTexture("./assets/textures/blocks/dirt.png");
    printf("[textures] dirt texture id: %d\n", world->textures.dirt_texture.id);

    // Try to load stone texture
    world->textures.stone_texture = LoadTexture("./assets/textures/blocks/stone.png");
    printf("[textures] stone texture id: %d\n", world->textures.stone_texture.id);

    // Try to load sand texture
    world->textures.sand_texture = LoadTexture("./assets/textures/blocks/sand.png");
    printf("[textures] sand texture id: %d\n", world->textures.sand_texture.id);

    // Try to load wood texture
    world->textures.wood_texture = LoadTexture("./assets/textures/blocks/wood.png");
    printf("[textures] wood texture id: %d\n", world->textures.wood_texture.id);

    // Try to load bedrock texture
    world->textures.bedrock_texture = LoadTexture("./assets/textures/blocks/stone.png");  // Using stone texture as fallback
    printf("[textures] bedrock texture id: %d\n", world->textures.bedrock_texture.id);

    world->textures.textures_loaded = true;
    printf("[textures] Block textures loaded\n");
}

// Unload block textures
void world_unload_textures(World* world)
{
    if (!world || !world->textures.textures_loaded) return;

    UnloadTexture(world->textures.grass_texture);
    UnloadTexture(world->textures.dirt_texture);
    UnloadTexture(world->textures.stone_texture);
    UnloadTexture(world->textures.sand_texture);
    UnloadTexture(world->textures.wood_texture);
    UnloadTexture(world->textures.bedrock_texture);

    world->textures.textures_loaded = false;
}

// Get texture for a block type
Texture2D world_get_block_texture(World* world, BlockType type)
{
    if (!world || !world->textures.textures_loaded) {
        // Return invalid texture if not loaded
        return (Texture2D){0};
    }

    switch (type) {
        case BLOCK_GRASS:
            return world->textures.grass_texture;
        case BLOCK_DIRT:
            return world->textures.dirt_texture;
        case BLOCK_STONE:
            return world->textures.stone_texture;
        case BLOCK_SAND:
            return world->textures.sand_texture;
        case BLOCK_WOOD:
            return world->textures.wood_texture;
        case BLOCK_BEDROCK:
            return world->textures.bedrock_texture;
        case BLOCK_AIR:
        default:
            return (Texture2D){0};
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
                // Depth limit: bedrock layer at y=-20, no blocks below
                if (world_y < -20) {
                    chunk->blocks[y][z][x].type = BLOCK_AIR;
                }
                // Bedrock layer at y=-20
                else if (world_y == -20) {
                    chunk->blocks[y][z][x].type = BLOCK_BEDROCK;
                }
                // Fill blocks below terrain height with appropriate type
                else if (world_y < terrain_height_blocks) {
                    // Top block is grass
                    if (world_y == terrain_height_blocks - 1) {
                        chunk->blocks[y][z][x].type = BLOCK_GRASS;
                    }
                    // 3 blocks below grass are dirt
                    else if (world_y > terrain_height_blocks - 5 && world_y < terrain_height_blocks - 1) {
                        chunk->blocks[y][z][x].type = BLOCK_DIRT;
                    }
                    // Everything else is stone
                    else {
                        chunk->blocks[y][z][x].type = BLOCK_STONE;
                    }
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
        case BLOCK_GRASS:
            return (Color){34, 139, 34, 255};  // Forest Green
        case BLOCK_DIRT:
            return (Color){139, 69, 19, 255};  // Saddle Brown
        case BLOCK_STONE:
            return (Color){128, 128, 128, 255};  // Grey
        case BLOCK_SAND:
            return (Color){238, 214, 175, 255};  // Sandy Brown
        case BLOCK_WOOD:
            return (Color){101, 67, 33, 255};  // Dark Brown
        case BLOCK_BEDROCK:
            return (Color){64, 64, 64, 255};  // Dark Grey (Bedrock)
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

// Update loaded chunks based on player position and camera direction
void world_update_chunks(World* world, Vector3 player_pos, Vector3 camera_forward)
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

    // Load chunks within load distance, prioritizing forward direction
    int load_dist = CHUNK_LOAD_DISTANCE;
    for (int cx = player_chunk_x - load_dist; cx <= player_chunk_x + load_dist; cx++) {
        for (int cy = player_chunk_y - 1; cy <= player_chunk_y + 1; cy++) {
            for (int cz = player_chunk_z - load_dist; cz <= player_chunk_z + load_dist; cz++) {
                // Calculate chunk center relative to player
                float chunk_center_x = cx * CHUNK_WIDTH + CHUNK_WIDTH / 2.0f;
                float chunk_center_y = cy * CHUNK_HEIGHT + CHUNK_HEIGHT / 2.0f;
                float chunk_center_z = cz * CHUNK_DEPTH + CHUNK_DEPTH / 2.0f;

                // Direction from player to chunk
                float to_chunk_x = chunk_center_x - player_pos.x;
                float to_chunk_y = chunk_center_y - player_pos.y;
                float to_chunk_z = chunk_center_z - player_pos.z;

                // Dot product with camera forward (if negative, chunk is behind player)
                float dot = to_chunk_x * camera_forward.x + to_chunk_y * camera_forward.y + to_chunk_z * camera_forward.z;

                // Skip chunks behind the player (with a small margin to avoid harsh cutoff)
                // Allow a small cone behind (dot product threshold of -0.3)
                if (dot < -0.3f * (load_dist + 1) * CHUNK_WIDTH) {
                    continue;
                }

                Chunk* chunk = world_load_or_create_chunk(world, cx, cy, cz);
                if (chunk && !chunk->loaded) {
                    // Generate this chunk procedurally
                    world_generate_chunk(chunk);
                    chunk->loaded = true;
                    chunk->generated = true;
                }
            }
        }
    }

    // Unload chunks that are too far away or behind the player
    int unload_dist = CHUNK_LOAD_DISTANCE + 1;
    int i = 0;
    while (i < world->chunk_cache.chunk_count) {
        Chunk* chunk = &world->chunk_cache.chunks[i];
        int dx = chunk->chunk_x - player_chunk_x;
        int dy = chunk->chunk_y - player_chunk_y;
        int dz = chunk->chunk_z - player_chunk_z;

        // Check if chunk is beyond unload distance or behind player
        float chunk_center_x = chunk->chunk_x * CHUNK_WIDTH + CHUNK_WIDTH / 2.0f;
        float chunk_center_y = chunk->chunk_y * CHUNK_HEIGHT + CHUNK_HEIGHT / 2.0f;
        float chunk_center_z = chunk->chunk_z * CHUNK_DEPTH + CHUNK_DEPTH / 2.0f;

        float to_chunk_x = chunk_center_x - player_pos.x;
        float to_chunk_y = chunk_center_y - player_pos.y;
        float to_chunk_z = chunk_center_z - player_pos.z;

        float dot = to_chunk_x * camera_forward.x + to_chunk_y * camera_forward.y + to_chunk_z * camera_forward.z;
        bool behind_player = dot < -0.3f * (unload_dist + 1) * CHUNK_WIDTH;
        bool too_far = dx*dx + dz*dz > unload_dist*unload_dist || dy > unload_dist || dy < -unload_dist;

        if (too_far || behind_player) {
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

    // Reset chunk loading tracker
    world->last_loaded_chunk_x = INT32_MAX;
    world->last_loaded_chunk_y = INT32_MAX;
    world->last_loaded_chunk_z = INT32_MAX;

    // Try to load initial chunks from disk
    // Start from origin chunk and nearby chunks
    int load_dist = CHUNK_LOAD_DISTANCE;
    bool any_chunk_loaded = false;

    for (int cx = -load_dist; cx <= load_dist; cx++) {
        for (int cy = -1; cy <= 1; cy++) {
            for (int cz = -load_dist; cz <= load_dist; cz++) {
                Chunk* chunk = world_load_or_create_chunk(world, cx, cy, cz);
                if (chunk && chunk->loaded) {
                    any_chunk_loaded = true;
                }
            }
        }
    }

    // If no chunks exist on disk, generate the starting area
    if (!any_chunk_loaded) {
        world_generate_prism(world);
    }

    return true;
}
