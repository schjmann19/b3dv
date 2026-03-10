#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "world.h"
#include "raylib.h"

// ============================================================================
// IMPROVED SEEDED RANDOM NUMBER GENERATION & NOISE FUNCTIONS
// ============================================================================

// Seeded random number generator (xorshift64*)
static uint64_t seed_state = 1;

uint64_t hash_seed(uint64_t x, uint64_t y, uint64_t seed)
{
    x = x ^ seed;
    y = y ^ (seed >> 32);

    uint64_t h = x ^ y;
    h ^= (h >> 33);
    h *= 0xff51afd7ed558ccdUL;
    h ^= (h >> 33);

    return h;
}

// Improved value noise that returns smoother gradients
static float noise_value(float x, float z, uint64_t seed)
{
    int xi = (int)floorf(x);
    int zi = (int)floorf(z);

    uint64_t hash = hash_seed(xi, zi, seed);
    // Map hash to range [-1, 1] for better noise
    return 2.0f * ((float)(hash % 1000) / 1000.0f) - 1.0f;
}

// Improved Perlin-like noise with interpolation
static float perlin_noise(float x, float z, uint64_t seed)
{
    int xi = (int)floorf(x);
    int zi = (int)floorf(z);
    float xf = x - xi;
    float zf = z - zi;

    // Smoothstep interpolation
    float u = xf * xf * (3.0f - 2.0f * xf);
    float v = zf * zf * (3.0f - 2.0f * zf);

    // Get corner values
    float n00 = noise_value((float)xi, (float)zi, seed);
    float n10 = noise_value((float)(xi + 1), (float)zi, seed);
    float n01 = noise_value((float)xi, (float)(zi + 1), seed);
    float n11 = noise_value((float)(xi + 1), (float)(zi + 1), seed);

    // Interpolate
    float nx0 = n00 * (1.0f - u) + n10 * u;
    float nx1 = n01 * (1.0f - u) + n11 * u;
    float result = nx0 * (1.0f - v) + nx1 * v;

    return result;
}

// Fractional Brownian Motion - multiple octaves for complex terrain
static float fbm_noise(float x, float z, int octaves, uint64_t seed)
{
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * perlin_noise(x * frequency, z * frequency, seed + i);
        max_value += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / max_value;
}

// Generate terrain height at a world position using seeded noise
static float terrain_height_seeded(float x, float z, uint64_t seed)
{
    // Base terrain with multiple scales for smoother transitions
    float height = 0.0f;

    // Large scale features (mountains/valleys) with extra octave
    height += fbm_noise(x * 0.002f, z * 0.002f, 6, seed) * 28.0f;

    // Medium scale features (hills)
    height += fbm_noise(x * 0.01f, z * 0.01f, 5, seed) * 14.0f;

    // Medium-small scale features
    height += fbm_noise(x * 0.04f, z * 0.04f, 4, seed + 50) * 8.0f;

    // Small scale features (terrain detail)
    height += fbm_noise(x * 0.12f, z * 0.12f, 3, seed + 100) * 4.0f;

    // Base level with offset
    height += 15.0f;

    // Clamp to reasonable heights
    if (height < 3.0f) height = 3.0f;
    if (height > 32.0f) height = 32.0f;

    return height;
}

// Ridge-based noise for cave systems (simple 2D representation)
static float ridge_noise(float x, float z, uint64_t seed)
{
    float value = fbm_noise(x * 0.02f, z * 0.02f, 2, seed + 1000);
    float ridge = 1.0f - fabsf(2.0f * value - 1.0f);
    return ridge;
}

// 3D cave noise for generating connected cave systems with proper interpolation
static float cave_noise_3d(float x, float y, float z, uint64_t seed)
{
    int xi = (int)floorf(x);
    int yi = (int)floorf(y);
    int zi = (int)floorf(z);

    float xf = x - xi;
    float yf = y - yi;
    float zf = z - zi;

    // Smoothstep interpolation for smooth cave transitions
    float u = xf * xf * (3.0f - 2.0f * xf);
    float v = yf * yf * (3.0f - 2.0f * yf);
    float w = zf * zf * (3.0f - 2.0f * zf);

    // Get 8 corner values (cube corners)
    float c000 = (float)(hash_seed(xi, yi, zi + seed) % 1000) / 1000.0f;
    float c100 = (float)(hash_seed(xi + 1, yi, zi + seed) % 1000) / 1000.0f;
    float c010 = (float)(hash_seed(xi, yi + 1, zi + seed) % 1000) / 1000.0f;
    float c110 = (float)(hash_seed(xi + 1, yi + 1, zi + seed) % 1000) / 1000.0f;
    float c001 = (float)(hash_seed(xi, yi, zi + 1 + seed) % 1000) / 1000.0f;
    float c101 = (float)(hash_seed(xi + 1, yi, zi + 1 + seed) % 1000) / 1000.0f;
    float c011 = (float)(hash_seed(xi, yi + 1, zi + 1 + seed) % 1000) / 1000.0f;
    float c111 = (float)(hash_seed(xi + 1, yi + 1, zi + 1 + seed) % 1000) / 1000.0f;

    // Interpolate along x
    float c00 = c000 * (1.0f - u) + c100 * u;
    float c10 = c010 * (1.0f - u) + c110 * u;
    float c01 = c001 * (1.0f - u) + c101 * u;
    float c11 = c011 * (1.0f - u) + c111 * u;

    // Interpolate along y
    float c0 = c00 * (1.0f - v) + c10 * v;
    float c1 = c01 * (1.0f - v) + c11 * v;

    // Interpolate along z
    float result = c0 * (1.0f - w) + c1 * w;

    return result;
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

    // Initialize seed to a random value if not set later
    world->seed = (uint64_t)time(NULL);

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
    new_chunk->modified = false;  // Not modified when first created

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
    snprintf(filepath, sizeof(filepath), "./worlds/%s/chunks/chunk_%d_%d_%d.chunk",
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
        new_chunk->modified = false;  // Not modified when loaded from disk
        printf("[chunk_load] Loaded chunk from %s\n", filepath);
    } else {
        // Chunk doesn't exist on disk - don't auto-generate, return empty chunk
        // The caller (world_load) will handle generation if needed
        printf("[chunk_load] Chunk not found: %s (will stay as air)\n", filepath);
    }

    return new_chunk;
}

// Generate a chunk procedurally with improved terrain
void world_generate_chunk(Chunk* chunk, uint64_t seed)
{
    if (!chunk) return;

    // Generate terrain with improved noise and features
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            // Calculate world position
            int world_x = chunk->chunk_x * CHUNK_WIDTH + x;
            int world_z = chunk->chunk_z * CHUNK_DEPTH + z;

            // Get height at this position using improved noise function
            // Use world seed directly to maintain terrain continuity across chunks
            float height = terrain_height_seeded((float)world_x, (float)world_z, seed);
            int terrain_height_blocks = (int)(height + 0.5f);

            // Generate vertical column
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int world_y = chunk->chunk_y * CHUNK_HEIGHT + y;
                BlockType block_type = BLOCK_AIR;

                // Bedrock layer at y=-20
                if (world_y == -20) {
                    block_type = BLOCK_BEDROCK;
                }
                // Depth limit: no blocks below bedrock
                else if (world_y < -20) {
                    block_type = BLOCK_AIR;
                }
                // Fill blocks below terrain height
                else if (world_y < terrain_height_blocks) {
                    // Underground cave systems - use 3D noise for connected caverns
                    // Caves appear deeper underground (Y < terrain - 4) and above bedrock
                    if (world_y < terrain_height_blocks - 4 && world_y > -18) {
                        // 3D cave noise for proper connected caverns with FBM
                        // Use multiple scales for natural-looking caves
                        float cave_val = 0.0f;
                        float cave_amp = 1.0f;
                        float cave_freq = 1.0f;

                        // Layer 1: Large cave chambers
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.05f,
                            (float)world_y * 0.05f,
                            (float)world_z * 0.05f,
                            seed + 5000
                        );
                        cave_amp *= 0.6f;

                        // Layer 2: Medium cave corridors
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.15f,
                            (float)world_y * 0.15f,
                            (float)world_z * 0.15f,
                            seed + 5001
                        );
                        cave_amp *= 0.5f;

                        // Layer 3: Small cave tunnels
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.4f,
                            (float)world_y * 0.4f,
                            (float)world_z * 0.4f,
                            seed + 5002
                        );

                        cave_val /= 2.1f;  // Normalize

                        // Create cave systems with threshold - higher=less caves
                        bool is_cave = cave_val < 0.35f;

                        if (is_cave) {
                            block_type = BLOCK_AIR;
                        }
                        // Top surface block is grass
                        else if (world_y == terrain_height_blocks - 1) {
                            block_type = BLOCK_GRASS;
                        }
                        // Few blocks below grass are dirt
                        else if (world_y > terrain_height_blocks - 4 && world_y < terrain_height_blocks - 1) {
                            block_type = BLOCK_DIRT;
                        }
                        // Deep dirt layer
                        else if (world_y > terrain_height_blocks - 7) {
                            block_type = BLOCK_DIRT;
                        }
                        // Everything else is stone
                        else {
                            block_type = BLOCK_STONE;
                        }
                    } else {
                        // Top surface block is grass
                        if (world_y == terrain_height_blocks - 1) {
                            block_type = BLOCK_GRASS;
                        }
                        // Few blocks below grass are dirt
                        else if (world_y > terrain_height_blocks - 4 && world_y < terrain_height_blocks - 1) {
                            block_type = BLOCK_DIRT;
                        }
                        // Deep dirt layer
                        else if (world_y > terrain_height_blocks - 7) {
                            block_type = BLOCK_DIRT;
                        }
                        // Everything else is stone
                        else {
                            block_type = BLOCK_STONE;
                        }
                    }
                } else {
                    block_type = BLOCK_AIR;
                }

                chunk->blocks[y][z][x].type = block_type;
            }
        }
    }

    chunk->generated = true;
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
        chunk->modified = true;  // Mark chunk as modified when block changes
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
        world_generate_chunk(chunk, world->seed);
        chunk->loaded = true;
        chunk->generated = true;
        chunk->modified = false;  // Freshly generated chunk is not modified
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
                    world_generate_chunk(chunk, world->seed);
                    chunk->loaded = true;
                    chunk->generated = true;
                    chunk->modified = false;  // Freshly generated chunk is not modified
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
            // Save chunk to disk before unloading if it was modified
            if (chunk->modified) {
                world_save_chunk(chunk, world->world_name);
                printf("[chunk_unload] Saved modified chunk %d,%d,%d\n", chunk->chunk_x, chunk->chunk_y, chunk->chunk_z);
                chunk->modified = false;  // Mark as saved
            }

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

// Save a single chunk to disk
bool world_save_chunk(Chunk* chunk, const char* world_name)
{
    if (!chunk || !world_name) return false;

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "./worlds/%s/chunks/chunk_%d_%d_%d.chunk",
             world_name, chunk->chunk_x, chunk->chunk_y, chunk->chunk_z);

    FILE* file = fopen(filepath, "wb");
    if (!file) return false;

    bool success = true;
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                BlockType block_type = chunk->blocks[y][z][x].type;
                if (fwrite(&block_type, sizeof(BlockType), 1, file) != 1) {
                    success = false;
                    break;
                }
            }
            if (!success) break;
        }
        if (!success) break;
    }

    fclose(file);
    return success;
}

// Initialize worlds folder if it doesn't exist
void world_system_init(void)
{
    #ifdef _WIN32
        system("if not exist worlds mkdir worlds");
    #else
        system("mkdir -p ./worlds");
    #endif
}

// Save world to files (chunks)
bool world_save(World* world, const char* world_name)
{
    if (!world || !world_name) return false;

    world_system_init();

    // Create world metadata file
    char world_dir[512];
    snprintf(world_dir, sizeof(world_dir), "./worlds/%s", world_name);

    #ifdef _WIN32
        char mkdir_cmd[512];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "if not exist \"worlds\\%s\" mkdir \"worlds\\%s\"", world_name, world_name);
        system(mkdir_cmd);
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "if not exist \"worlds\\%s\\chunks\" mkdir \"worlds\\%s\\chunks\"", world_name, world_name);
        system(mkdir_cmd);
    #else
        char mkdir_cmd[512];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"./worlds/%s/chunks\"", world_name);
        system(mkdir_cmd);
    #endif

    // Write world metadata file
    char metadata_path[512];
    snprintf(metadata_path, sizeof(metadata_path), "./worlds/%s/world.txt", world_name);

    FILE* metadata_file = fopen(metadata_path, "w");
    if (metadata_file) {
        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

        fprintf(metadata_file, "name=%s\n", world_name);
        fprintf(metadata_file, "seed=%lu\n", world->seed);
        fprintf(metadata_file, "last_saved=%s\n", time_str);
        fprintf(metadata_file, "chunk_count=%d\n", world->chunk_cache.chunk_count);
        fprintf(metadata_file, "player_x=%.6f\n", world->last_player_position.x);
        fprintf(metadata_file, "player_y=%.6f\n", world->last_player_position.y);
        fprintf(metadata_file, "player_z=%.6f\n", world->last_player_position.z);
        fclose(metadata_file);
    }

    // Save each loaded chunk
    for (int i = 0; i < world->chunk_cache.chunk_count; i++) {
        Chunk* chunk = &world->chunk_cache.chunks[i];

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "./worlds/%s/chunks/chunk_%d_%d_%d.chunk",
                 world_name, chunk->chunk_x, chunk->chunk_y, chunk->chunk_z);

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
        chunk->modified = false;  // Mark chunk as saved
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

    // Load player position from metadata if it exists
    world->last_player_position = (Vector3){8.0f, 20.0f, 8.0f};  // Default position
    char metadata_path[512];
    snprintf(metadata_path, sizeof(metadata_path), "./worlds/%s/world.txt", world_name);
    FILE* metadata_file = fopen(metadata_path, "r");
    if (metadata_file) {
        char line[256];
        while (fgets(line, sizeof(line), metadata_file)) {
            float x, y, z;
            uint64_t seed_val;
            if (sscanf(line, "player_x=%f", &x) == 1) {
                world->last_player_position.x = x;
            } else if (sscanf(line, "player_y=%f", &y) == 1) {
                world->last_player_position.y = y;
            } else if (sscanf(line, "player_z=%f", &z) == 1) {
                world->last_player_position.z = z;
            } else if (sscanf(line, "seed=%lu", &seed_val) == 1) {
                world->seed = seed_val;
            }
        }
        fclose(metadata_file);
    }

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
