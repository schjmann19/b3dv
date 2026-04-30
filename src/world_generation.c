#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

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

    // Improved smoothstep (Perlin's version for smoother curves)
    // Regular smoothstep: 3t^2 - 2t^3
    // Improved smoothstep: 6t^5 - 15t^4 + 10t^3 (makes terrain much smoother)
    float u = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
    float v = zf * zf * zf * (zf * (zf * 6.0f - 15.0f) + 10.0f);

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
    // Base terrain with multiple scales for much smoother transitions
    float height = 0.0f;

    // Very large scale features (continental features) - creates smooth rolling hills
    height += fbm_noise(x * 0.0008f, z * 0.0008f, 5, seed) * 20.0f;

    // Large scale features (mountains/valleys) with reduced amplitude for smoother blend
    height += fbm_noise(x * 0.002f, z * 0.002f, 6, seed + 1) * 16.0f;

    // Medium scale features (hills) - smoothly blended
    height += fbm_noise(x * 0.008f, z * 0.008f, 5, seed + 2) * 10.0f;

    // Medium-small scale features for land relief
    height += fbm_noise(x * 0.025f, z * 0.025f, 4, seed + 50) * 6.0f;

    // Small scale features (terrain detail) - only subtle variation to avoid 1-block stubs
    height += fbm_noise(x * 0.06f, z * 0.06f, 3, seed + 100) * 1.0f;

    // Base level at y=100
    height += 100.0f;

    // Clamp to reasonable heights
    if (height < 85.0f) height = 85.0f;
    if (height > 120.0f) height = 120.0f;

    return height;
}

// Smooth terrain height to reduce 1-block stubs while preserving land relief
static int smooth_terrain_height(int x, int z, uint64_t seed)
{
    // Sample this position and its 8 neighbors
    float heights[9];
    int idx = 0;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            heights[idx] = terrain_height_seeded((float)(x + dx), (float)(z + dz), seed);
            idx++;
        }
    }

    // Center is heights[4]
    float center = heights[4];

    // Count how many neighbors are significantly different (more than 2 blocks away)
    int isolated_count = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4) {
            if (fabsf(heights[i] - center) > 2.0f) {
                isolated_count++;
            }
        }
    }

    // If this position is isolated (8 neighbors different), smooth it toward their average
    if (isolated_count == 8) {
        float neighbor_sum = 0.0f;
        for (int i = 0; i < 9; i++) {
            if (i != 4) {
                neighbor_sum += heights[i];
            }
        }
        float neighbor_avg = neighbor_sum / 8.0f;
        center = center * 0.4f + neighbor_avg * 0.6f;  // Blend 40/60 toward neighbors
    }

    return (int)(center);
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

    // Improved smoothstep (Perlin's version for smoother cave surfaces)
    float u = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
    float v = yf * yf * yf * (yf * (yf * 6.0f - 15.0f) + 10.0f);
    float w = zf * zf * zf * (zf * (zf * 6.0f - 15.0f) + 10.0f);

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

    // Preallocate large chunk cache upfront to avoid realloc during gameplay
    // This prevents pointer invalidation when worker thread is accessing chunks
    world->chunk_cache.chunk_capacity = 4096;  // Pre-allocate space for 4096 chunks (very large)
    world->chunk_cache.chunks = (Chunk*)malloc(sizeof(Chunk) * world->chunk_cache.chunk_capacity);
    world->chunk_cache.chunk_count = 0;
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

    // Initialize worker thread system
    pthread_mutex_init(&world->cache_mutex, NULL);  // Initialize cache mutex before worker starts
    worker_init(world);

    return world;
}

// Free world memory and all chunks
void world_free(World* world)
{
    if (world) {
        // Shutdown worker thread first
        worker_shutdown(world);

        // Clean up all remaining chunks
        for (int i = 0; i < world->chunk_cache.chunk_count; i++) {
            Chunk* chunk = &world->chunk_cache.chunks[i];
            chunk_free_visible_blocks(chunk);  // Free any cached mesh data
            // NOTE: Don't destroy mutexes - they're part of preallocated array memory
            // They'll be reused when chunks are respawned or cleaned up
        }

        // Free chunk cache array and cache mutex
        if (world->chunk_cache.chunks) {
            free(world->chunk_cache.chunks);
        }
        pthread_mutex_destroy(&world->cache_mutex);  // Destroy cache access mutex

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
        fprintf(stderr, "[ERROR] Chunk cache overflow! count=%d, capacity=%d. Preallocated buffer was too small!\n",
                world->chunk_cache.chunk_count, world->chunk_cache.chunk_capacity);
        return NULL;  // Fail instead of reallocating - we should have preallocated enough
    }

    // Create new chunk
    Chunk* new_chunk = &world->chunk_cache.chunks[world->chunk_cache.chunk_count++];
    new_chunk->chunk_x = chunk_x;
    new_chunk->chunk_y = chunk_y;
    new_chunk->chunk_z = chunk_z;
    new_chunk->loaded = false;
    new_chunk->generated = false;
    new_chunk->modified = false;  // Not modified when first created
    new_chunk->needs_relighting = true;  // New chunks need lighting calculation
    new_chunk->meshed = false;
    new_chunk->pending_save = false;
    new_chunk->pending_unload = false;
    new_chunk->in_use_count = 0;
    // Initialize double-buffered lighting (avoid tearing during worker updates)
    new_chunk->active_light_buffer = 0;
    pthread_mutex_init(&new_chunk->light_swap_mutex, NULL);

    // Initialize double-buffered visible blocks cache
    new_chunk->visible_blocks[0] = NULL;
    new_chunk->visible_blocks[1] = NULL;
    new_chunk->visible_count[0] = 0;
    new_chunk->visible_count[1] = 0;
    new_chunk->visible_capacity[0] = 0;
    new_chunk->visible_capacity[1] = 0;
    new_chunk->active_mesh = 0;  // Start with buffer 0
    pthread_mutex_init(&new_chunk->mesh_swap_mutex, NULL);  // Mutex for atomic mesh swaps
    pthread_mutex_init(&new_chunk->mutex, NULL);  // Initialize chunk mutex

    // Initialize blocks to air and lighting to 0
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                new_chunk->blocks[y][z][x].type = BLOCK_AIR;
                // Initialize both lighting buffers to prevent garbage data
                new_chunk->skylight[0][y][z][x] = 0;
                new_chunk->skylight[1][y][z][x] = 0;
                new_chunk->blocklight[0][y][z][x] = 0;
                new_chunk->blocklight[1][y][z][x] = 0;
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
        new_chunk->needs_relighting = true;  // Recalculate lighting - arrays were not saved to disk
        printf("[chunk_load] Loaded chunk from %s\n", filepath);
        // Queue for lighting and meshing
        worker_queue_chunk(world, new_chunk);
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

            // Get height at this position using improved noise function with smoothing
            // Use world seed directly to maintain terrain continuity across chunks
            int terrain_height_blocks = smooth_terrain_height(world_x, world_z, seed);

            // Generate vertical column
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int world_y = chunk->chunk_y * CHUNK_HEIGHT + y;
                BlockType block_type = BLOCK_AIR;

                // Bedrock layer at y=0
                if (world_y == 0) {
                    block_type = BLOCK_BEDROCK;
                }
                // Depth limit: no blocks below bedrock
                else if (world_y < 0) {
                    block_type = BLOCK_AIR;
                }
                // Fill blocks below terrain height
                else if (world_y < terrain_height_blocks) {
                    // Underground cave systems - use 3D noise for connected caverns
                    // Tiered cave generation for natural progression
                    bool is_cave = false;

                    // Big caves from y=15 to y=40
                    if (world_y >= 15 && world_y < 40) {
                        float cave_val = 0.0f;
                        float cave_amp = 1.0f;

                        // Layer 1: Large cave chambers
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.03f,
                            (float)world_y * 0.03f,
                            (float)world_z * 0.03f,
                            seed + 5000
                        );
                        cave_amp *= 0.7f;

                        // Layer 2: Medium cave corridors
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.1f,
                            (float)world_y * 0.1f,
                            (float)world_z * 0.1f,
                            seed + 5001
                        );
                        cave_amp *= 0.6f;

                        // Layer 3: Small cave tunnels
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.25f,
                            (float)world_y * 0.25f,
                            (float)world_z * 0.25f,
                            seed + 5002
                        );

                        cave_val /= 2.3f;

                        // Lower threshold for bigger, more connected caves
                        is_cave = cave_val < 0.42f;
                    }
                    // Smaller caves from y=40 to y=85
                    else if (world_y >= 40 && world_y < 85) {
                        float cave_val = 0.0f;
                        float cave_amp = 1.0f;

                        // Layer 1: Smaller chambers
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.05f,
                            (float)world_y * 0.05f,
                            (float)world_z * 0.05f,
                            seed + 5010
                        );
                        cave_amp *= 0.7f;

                        // Layer 2: Smaller corridors
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.15f,
                            (float)world_y * 0.15f,
                            (float)world_z * 0.15f,
                            seed + 5011
                        );
                        cave_amp *= 0.6f;

                        // Layer 3: Fine tunnels
                        cave_val += cave_amp * cave_noise_3d(
                            (float)world_x * 0.35f,
                            (float)world_y * 0.35f,
                            (float)world_z * 0.35f,
                            seed + 5012
                        );

                        cave_val /= 2.3f;

                        // Higher threshold for smaller, more fragmented caves
                        is_cave = cave_val < 0.45f;
                    }

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

    // CRITICAL: Lock cache while accessing/modifying chunk cache
    pthread_mutex_lock(&world->cache_mutex);

    // Get or create chunk
    Chunk* chunk = world_load_or_create_chunk(world, chunk_x, chunk_y, chunk_z);
    if (chunk) {
        // Lock chunk while modifying blocks and invalidating cache
        pthread_mutex_lock(&chunk->mutex);

        // Get old block to check if lighting is affected
        BlockType old_block = world_chunk_get_block(chunk, local_x, local_y, local_z);
        BlockProperties old_props = get_block_properties(old_block);
        BlockProperties new_props = get_block_properties(type);

        // Check if this block change affects light propagation
        // Light is affected if: emission changed, opacity changed, or air<->solid transition
        bool affects_light = (old_props.emission != new_props.emission) ||
                            (old_props.opacity != new_props.opacity) ||
                            (old_block == BLOCK_AIR) != (type == BLOCK_AIR);

        world_chunk_set_block(chunk, local_x, local_y, local_z, type);

        // Mark chunk as needing relighting if block change affects light
        if (affects_light) {
            chunk->needs_relighting = true;
        }
        // Always mark meshed=false so worker knows to skip the expensive mesh rebuild
        // (we're doing it immediately below)
        chunk->meshed = false;

        pthread_mutex_unlock(&chunk->mutex);
        pthread_mutex_unlock(&world->cache_mutex);

        // INSTANT MESH UPDATE: Rebuild visible blocks immediately on main thread
        // This gives instant visual feedback for block changes without waiting for worker
        // The visible block list doesn't depend on lighting (lighting calculated at render time)
        // chunk_cache_visible_blocks calls world_get_block which locks world->cache_mutex,
        // so we call it WITHOUT holding any locks
        chunk_cache_visible_blocks(chunk, world);

        // Update mesh flag to mark it as done (worker will skip mesh rebuild and only do lighting)
        pthread_mutex_lock(&chunk->mutex);
        chunk->meshed = true;  // Mark mesh as already rebuilt
        pthread_mutex_unlock(&chunk->mutex);

        // Queue worker for lighting recalculation (mesh already updated)
        worker_queue_chunk(world, chunk);

        // Also update neighbor chunk meshes immediately to avoid temporary holes at chunk boundaries.
        // Only do this for neighbors that are adjacent to the modified block (i.e. block is on a chunk edge).
        // This avoids unnecessary work when editing blocks away from chunk boundaries.
        const int neighbor_offsets[6][3] = {
            { 1, 0, 0 }, {-1, 0, 0},
            { 0, 1, 0 }, { 0,-1, 0},
            { 0, 0, 1 }, { 0, 0,-1}
        };

        for (int ni = 0; ni < 6; ni++) {
            // Only rebuild neighbor chunk if the changed block lies on the shared face
            if ((ni == 0 && local_x != CHUNK_WIDTH - 1) || (ni == 1 && local_x != 0) ||
                (ni == 2 && local_y != CHUNK_HEIGHT - 1) || (ni == 3 && local_y != 0) ||
                (ni == 4 && local_z != CHUNK_DEPTH - 1) || (ni == 5 && local_z != 0)) {
                continue;
            }

            int32_t nx = chunk_x + neighbor_offsets[ni][0];
            int32_t ny = chunk_y + neighbor_offsets[ni][1];
            int32_t nz = chunk_z + neighbor_offsets[ni][2];

            Chunk* neighbor = world_get_chunk(world, nx, ny, nz);
            if (!neighbor || !neighbor->loaded || !neighbor->generated) continue;

            // Invalidate neighbor mesh and rebuild it immediately to prevent flicker
            pthread_mutex_lock(&neighbor->mutex);
            neighbor->meshed = false;
            pthread_mutex_unlock(&neighbor->mutex);

            chunk_cache_visible_blocks(neighbor, world);

            pthread_mutex_lock(&neighbor->mutex);
            neighbor->meshed = true;
            pthread_mutex_unlock(&neighbor->mutex);

            // Recalculate lighting for the neighbor chunk as well
            worker_queue_chunk(world, neighbor);
        }
    } else {
        pthread_mutex_unlock(&world->cache_mutex);
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

    // CRITICAL: Lock cache before accessing chunk to prevent concurrent unloading
    pthread_mutex_lock(&world->cache_mutex);
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        pthread_mutex_unlock(&world->cache_mutex);
        return BLOCK_AIR;  // Unloaded chunks are treated as air
    }

    BlockType result = world_chunk_get_block(chunk, local_x, local_y, local_z);
    pthread_mutex_unlock(&world->cache_mutex);
    return result;
}

// Get block, treating unloaded chunks as STONE (for lighting calculations)
// This prevents false positives where light penetrates through unloaded chunks
BlockType world_get_block_or_solid(World* world, int x, int y, int z)
{
    // Calculate chunk coordinates
    int32_t chunk_x = x < 0 ? (x - CHUNK_WIDTH + 1) / CHUNK_WIDTH : x / CHUNK_WIDTH;
    int32_t chunk_y = y < 0 ? (y - CHUNK_HEIGHT + 1) / CHUNK_HEIGHT : y / CHUNK_HEIGHT;
    int32_t chunk_z = z < 0 ? (z - CHUNK_DEPTH + 1) / CHUNK_DEPTH : z / CHUNK_DEPTH;

    // Calculate position within chunk
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_y = y - (chunk_y * CHUNK_HEIGHT);
    int local_z = z - (chunk_z * CHUNK_DEPTH);

    // Lock cache before accessing chunk
    pthread_mutex_lock(&world->cache_mutex);
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        pthread_mutex_unlock(&world->cache_mutex);
        return BLOCK_STONE;  // Unloaded chunks are treated as solid (prevents false light propagation)
    }

    BlockType result = world_chunk_get_block(chunk, local_x, local_y, local_z);
    pthread_mutex_unlock(&world->cache_mutex);
    return result;
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
        case BLOCK_GLOWSTONE:
            return (Color){255, 255, 200, 255};  // Bright warm white
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
        worker_queue_chunk(world, chunk);  // Queue for worker to calculate lighting and mesh
        chunk->modified = false;  // Freshly generated chunk is not modified
    }
}

// Update loaded chunks based on player position and camera direction
void world_update_chunks(World* world, Vector3 player_pos, Vector3 camera_forward, float render_distance_blocks)
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

    // CRITICAL: Lock cache mutex while loading/creating chunks to prevent races with unload
    pthread_mutex_lock(&world->cache_mutex);

    // Load chunks within load distance, prioritizing forward direction
    // Compute chunk load distance from desired render distance in blocks
    int load_dist = (int)ceilf(render_distance_blocks / (float)CHUNK_WIDTH);
    if (load_dist < 1) load_dist = 1;
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
                    if (!chunk->generated) {
                        // Generate this chunk procedurally
                        world_generate_chunk(chunk, world->seed);
                        chunk->generated = true;
                    }
                    // Mark as loaded again (this chunk was previously unloaded)
                    chunk->loaded = true;
                    chunk->pending_unload = false;
                    // Mark lighting/mesh dirty so the worker will rebuild.
                    chunk->needs_relighting = true;
                    chunk->meshed = false;
                    // NOTE: Don't queue yet - we'll do it after releasing the lock to avoid holding lock too long
                }
            }
        }
    }

    pthread_mutex_unlock(&world->cache_mutex);

    // Queue newly generated chunks for lighting/meshing after releasing cache_mutex
    pthread_mutex_lock(&world->cache_mutex);
    for (int cx = player_chunk_x - load_dist; cx <= player_chunk_x + load_dist; cx++) {
        for (int cy = player_chunk_y - 1; cy <= player_chunk_y + 1; cy++) {
            for (int cz = player_chunk_z - load_dist; cz <= player_chunk_z + load_dist; cz++) {
                Chunk* chunk = world_get_chunk(world, cx, cy, cz);
                if (chunk && chunk->generated && chunk->loaded && chunk->needs_relighting) {
                    // Queue for worker to calculate lighting and mesh
                    worker_queue_chunk(world, chunk);
                }
            }
        }
    }
    pthread_mutex_unlock(&world->cache_mutex);

    // Note: We no longer flush the worker queue here to avoid stalling the main thread.
    // Chunks that are in-use by the worker (in_use_count > 0) will not be unloaded until
    // their jobs complete.

    // CRITICAL: Lock cache mutex while modifying chunk array
    pthread_mutex_lock(&world->cache_mutex);

    // Unload chunks that are too far away or behind the player
    int unload_dist = load_dist + 1;
    int i = 0;

    // Throttle unloads to avoid stuttering when crossing chunk boundaries.
    // We only unload a small number of chunks per frame, spreading work across frames.
    const int max_unloads_per_frame = 1;
    int unloads_this_frame = 0;

    while (i < world->chunk_cache.chunk_count) {
        // If we already unloaded enough chunks this frame, stop here.
        if (unloads_this_frame >= max_unloads_per_frame) {
            break;
        }

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
            // Avoid unloading while a worker is still processing this chunk
            if (__atomic_load_n(&chunk->in_use_count, __ATOMIC_ACQUIRE) > 0) {
                i++;
                continue;
            }

            // If modified, queue async save and mark for unload after save completes.
            // This prevents main-thread stalls due to disk I/O during unloading.
            if (chunk->modified && !chunk->pending_save) {
                chunk->pending_unload = true;
                // We can mark the chunk as unloaded for rendering purposes while we save it.
                // This keeps it in memory until save completes, but removes it from active rendering.
                chunk->loaded = false;
                worker_queue_chunk_save(world, chunk);
            }

            // If chunk is not pending save, we can unload it immediately
            if (!chunk->pending_save) {
                // Clean up chunk resources
                chunk_free_visible_blocks(chunk);  // Free mesh

                // Remove chunk from cache (swap with last)
                if (i < world->chunk_cache.chunk_count - 1) {
                    world->chunk_cache.chunks[i] = world->chunk_cache.chunks[world->chunk_cache.chunk_count - 1];
                }
                world->chunk_cache.chunk_count--;
                unloads_this_frame++;
            } else {
                // Skip this chunk for now; it will be removed once the save completes
                i++;
            }
        } else {
            i++;
        }
    }

    pthread_mutex_unlock(&world->cache_mutex);
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

    // CRITICAL: Flush the worker queue before clearing chunks
    // This prevents the worker thread from accessing chunks we're about to reset
    worker_flush_queue(world);

    // Clear existing chunks first
    // NOTE: Don't destroy mutexes - worker thread is still running and might use them
    // Just clear out the data
    if (world->chunk_cache.chunks) {
        for (int i = 0; i < world->chunk_cache.chunk_count; i++) {
            chunk_free_visible_blocks(&world->chunk_cache.chunks[i]);
            // Don't destroy mutexes - they'll be reused when new chunks are loaded
        }
    }

    // Reset the chunk count (keeps pre-allocated memory)
    world->chunk_cache.chunk_count = 0;

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
    // Only generate minimal spawn area to avoid startup lag
    int spawn_dist = 1;  // Only load immediate area around spawn
    bool any_chunk_loaded = false;

    for (int cx = -spawn_dist; cx <= spawn_dist; cx++) {
        for (int cy = -1; cy <= 1; cy++) {
            for (int cz = -spawn_dist; cz <= spawn_dist; cz++) {
                Chunk* chunk = world_load_or_create_chunk(world, cx, cy, cz);
                if (chunk && chunk->loaded) {
                    any_chunk_loaded = true;
                    // Loaded chunks still need lighting and meshing
                    if (chunk->needs_relighting || !chunk->meshed) {
                        worker_queue_chunk(world, chunk);
                    }
                } else if (chunk && !chunk->generated) {
                    // Only generate if not yet generated
                    world_generate_chunk(chunk, world->seed);
                    chunk->loaded = true;
                    chunk->generated = true;
                    worker_queue_chunk(world, chunk);  // Queue for worker to calculate lighting and mesh
                    chunk->modified = false;
                }
            }
        }
    }

    return true;
}

// Get skylight level at a specific world position
// Returns 0 if in a solid block or unloaded area
// OPTIMIZED: Fast path for unloaded chunks (returns 0 immediately)
uint8_t world_get_skylight(World* world, int x, int y, int z)
{
    // Out of bounds?
    if (y < WORLD_Y_MIN || y > WORLD_Y_MAX) return 0;

    // Calculate chunk coordinates
    int32_t chunk_x = x < 0 ? (x - CHUNK_WIDTH + 1) / CHUNK_WIDTH : x / CHUNK_WIDTH;
    int32_t chunk_y = y < 0 ? (y - CHUNK_HEIGHT + 1) / CHUNK_HEIGHT : y / CHUNK_HEIGHT;
    int32_t chunk_z = z < 0 ? (z - CHUNK_DEPTH + 1) / CHUNK_DEPTH : z / CHUNK_DEPTH;

    // Calculate position within chunk
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_y = y - (chunk_y * CHUNK_HEIGHT);
    int local_z = z - (chunk_z * CHUNK_DEPTH);

    // Bounds check first (fast)
    if (local_x < 0 || local_x >= CHUNK_WIDTH || local_y < 0 || local_y >= CHUNK_HEIGHT || local_z < 0 || local_z >= CHUNK_DEPTH) {
        return 0;  // Out of chunk bounds
    }

    // Get chunk (linear search - but we return early for out-of-bounds)
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        return 0;  // Unloaded chunks have no skylight
    }

    int active = __atomic_load_n(&chunk->active_light_buffer, __ATOMIC_ACQUIRE);
    return chunk->skylight[active][local_y][local_z][local_x];
}

// Get blocklight level at a specific world position
// Returns 0 if in solid block or unloaded area
uint8_t world_get_blocklight(World* world, int x, int y, int z)
{
    // Out of bounds?
    if (y < WORLD_Y_MIN || y > WORLD_Y_MAX) return 0;

    // Calculate chunk coordinates
    int32_t chunk_x = x < 0 ? (x - CHUNK_WIDTH + 1) / CHUNK_WIDTH : x / CHUNK_WIDTH;
    int32_t chunk_y = y < 0 ? (y - CHUNK_HEIGHT + 1) / CHUNK_HEIGHT : y / CHUNK_HEIGHT;
    int32_t chunk_z = z < 0 ? (z - CHUNK_DEPTH + 1) / CHUNK_DEPTH : z / CHUNK_DEPTH;

    // Calculate position within chunk
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_y = y - (chunk_y * CHUNK_HEIGHT);
    int local_z = z - (chunk_z * CHUNK_DEPTH);

    // Bounds check first (fast)
    if (local_x < 0 || local_x >= CHUNK_WIDTH || local_y < 0 || local_y >= CHUNK_HEIGHT || local_z < 0 || local_z >= CHUNK_DEPTH) {
        return 0;  // Out of chunk bounds
    }

    // Get chunk
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        return 0;  // Unloaded chunks have no blocklight
    }

    int active = __atomic_load_n(&chunk->active_light_buffer, __ATOMIC_ACQUIRE);
    return chunk->blocklight[active][local_y][local_z][local_x];
}

// Simple queue for BFS light propagation (static to avoid allocation per chunk)
#define LIGHT_QUEUE_SIZE 4096
typedef struct {
    int x, y, z;
    uint8_t light;
} LightQueueEntry;

// Calculate blocklight levels for a chunk using flood-fill from light-emitting blocks
// Uses BFS propagation like Minecraft: light spreads from emitters (glowstone)
// Note: uses double-buffering so render thread never reads partially-updated lighting data.
void calculate_chunk_blocklight(Chunk* chunk, World* world, int target_buffer)
{
    if (!chunk) return;

    uint8_t (*blocklight_buf)[CHUNK_DEPTH][CHUNK_WIDTH] = chunk->blocklight[target_buffer];

    // Initialize blocklight to 0 in the target buffer
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                blocklight_buf[y][z][x] = 0;
            }
        }
    }

    // BFS queue for light propagation
    static LightQueueEntry queue[LIGHT_QUEUE_SIZE];
    int queue_head = 0, queue_tail = 0;

    // Find all light-emitting blocks and add to queue
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                BlockProperties props = get_block_properties(chunk->blocks[y][z][x].type);
                if (props.emission > 0) {
                    blocklight_buf[y][z][x] = props.emission;
                    // Add to queue
                    if (queue_tail < LIGHT_QUEUE_SIZE) {
                        queue[queue_tail].x = x;
                        queue[queue_tail].y = y;
                        queue[queue_tail].z = z;
                        queue[queue_tail].light = props.emission;
                        queue_tail++;
                    }
                }
            }
        }
    }

    // BFS flood-fill: propagate light to neighbors
    while (queue_head < queue_tail) {
        LightQueueEntry entry = queue[queue_head++];
        int x = entry.x, y = entry.y, z = entry.z;
        uint8_t current_light = entry.light;

        // Skip if light would be absorbed to 0
        if (current_light <= 1) continue;

        // Propagate to 6 neighbors (±X, ±Y, ±Z)
        int neighbors[6][3] = {
            {x+1, y, z}, {x-1, y, z},  // ±X
            {x, y+1, z}, {x, y-1, z},  // ±Y
            {x, y, z+1}, {x, y, z-1}   // ±Z
        };

        for (int i = 0; i < 6; i++) {
            int nx = neighbors[i][0];
            int ny = neighbors[i][1];
            int nz = neighbors[i][2];

            // Bounds check
            if (nx < 0 || nx >= CHUNK_WIDTH || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= CHUNK_DEPTH) {
                continue;  // Cross-chunk lighting not handled in this pass
            }

            BlockProperties neighbor_props = get_block_properties(chunk->blocks[ny][nz][nx].type);
            uint8_t new_light = current_light - 1;  // Light reduces by 1 for each block distance

            // Check if we should update this neighbor
            if (neighbor_props.opacity < 15) {  // Only propagate through non-opaque blocks
                if (new_light > blocklight_buf[ny][nz][nx]) {
                    blocklight_buf[ny][nz][nx] = new_light;
                    // Add to queue if there's still light to propagate
                    if (queue_tail < LIGHT_QUEUE_SIZE && new_light > 1) {
                        queue[queue_tail].x = nx;
                        queue[queue_tail].y = ny;
                        queue[queue_tail].z = nz;
                        queue[queue_tail].light = new_light;
                        queue_tail++;
                    }
                }
            }
        }
    }

    // Note: the caller (worker thread) swaps the active lighting buffer after
    // both skylight + blocklight are computed to avoid intermediate states.
}

// Calculate skylight levels for a chunk using BFS from the sky downward
// Algorithm:
// 1. Start from y=top of world, skylight = 15 (fully lit)
// 2. Propagate downward through air blocks
// 3. Opaque blocks block skylight completely
// 4. Result: caves have 0 skylight UNLESS they have direct line to sky
// Note: uses double-buffering so render thread never reads partially-updated lighting data.
void calculate_chunk_skylight(Chunk* chunk, World* world, int target_buffer)
{
    if (!chunk) return;

    uint8_t (*skylight_buf)[CHUNK_DEPTH][CHUNK_WIDTH] = chunk->skylight[target_buffer];

    // Initialize skylight to 0 (caves are dark by default)
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                skylight_buf[y][z][x] = 0;
            }
        }
    }

    // For each XZ column, trace from top downward
    for (int z = 0; z < CHUNK_DEPTH; z++) {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            int world_x = chunk->chunk_x * CHUNK_WIDTH + x;
            int world_z = chunk->chunk_z * CHUNK_DEPTH + z;

            // Start from top of world, bounded by WORLD_Y_MAX (no light above world limits)
            uint8_t current_light = 15;

            // Scan downward from world top to world bottom - enforces hard Y limits
            // This prevents unloaded chunks above from leaking light into caves
            for (int world_y = WORLD_Y_MAX; world_y >= WORLD_Y_MIN; world_y--) {
                BlockType block = world_get_block(world, world_x, world_y, world_z);

                if (block == BLOCK_AIR) {
                    // Air transmits skylight fully
                    if (world_y >= chunk->chunk_y * CHUNK_HEIGHT &&
                        world_y < chunk->chunk_y * CHUNK_HEIGHT + CHUNK_HEIGHT) {
                        int local_y = world_y - (chunk->chunk_y * CHUNK_HEIGHT);
                        skylight_buf[local_y][z][x] = current_light;
                    }
                } else {
                    // Solid block blocks skylight and all blocks below until exposed to air again
                    current_light = 0;  // No more skylight below this block
                }
            }
        }
    }

    // Second pass: propagate skylight horizontally into cavities from exposed air
    // This uses BFS to spread light into caves that open to the sky
    static LightQueueEntry queue[LIGHT_QUEUE_SIZE];
    int queue_head = 0, queue_tail = 0;

    // Find all air blocks with skylight and queue them
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                if (chunk->blocks[y][z][x].type == BLOCK_AIR && skylight_buf[y][z][x] > 0) {
                    if (queue_tail < LIGHT_QUEUE_SIZE) {
                        queue[queue_tail].x = x;
                        queue[queue_tail].y = y;
                        queue[queue_tail].z = z;
                        queue[queue_tail].light = skylight_buf[y][z][x];
                        queue_tail++;
                    }
                }
            }
        }
    }

    // BFS: propagate skylight horizontally to adjacent air blocks
    while (queue_head < queue_tail) {
        LightQueueEntry entry = queue[queue_head++];
        int x = entry.x, y = entry.y, z = entry.z;
        uint8_t current_light = entry.light;

        // Skip if light would be absorbed to 0
        if (current_light <= 1) continue;

        uint8_t new_light = current_light - 1;

        // Check 4 horizontal neighbors and 2 vertical (all 6 directions)
        int neighbors[6][3] = {
            {x+1, y, z}, {x-1, y, z},  // ±X
            {x, y+1, z}, {x, y-1, z},  // ±Y
            {x, y, z+1}, {x, y, z-1}   // ±Z
        };

        for (int i = 0; i < 6; i++) {
            int nx = neighbors[i][0];
            int ny = neighbors[i][1];
            int nz = neighbors[i][2];

            // Bounds check
            if (nx < 0 || nx >= CHUNK_WIDTH || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= CHUNK_DEPTH) {
                continue;  // Cross-chunk not handled in this pass
            }

            if (chunk->blocks[ny][nz][nx].type == BLOCK_AIR) {
                if (new_light > skylight_buf[ny][nz][nx]) {
                    skylight_buf[ny][nz][nx] = new_light;
                    if (queue_tail < LIGHT_QUEUE_SIZE && new_light > 1) {
                        queue[queue_tail].x = nx;
                        queue[queue_tail].y = ny;
                        queue[queue_tail].z = nz;
                        queue[queue_tail].light = new_light;
                        queue_tail++;
                    }
                }
            }
        }
    }

    // Note: the caller (worker thread) swaps the active lighting buffer after
    // both skylight + blocklight are computed to avoid intermediate states.
}

// Pre-compute and cache all visible blocks in a chunk (blocks with exposed faces)
// This avoids the per-frame triple-nested loop and provides massive performance improvement
void chunk_cache_visible_blocks(Chunk* chunk, World* world)
{
    if (!chunk) return;

    // Build into a temporary array to avoid realloc while render thread might use the original
    int temp_capacity = 1024;  // Start with 1024 blocks
    int temp_count = 0;
    CachedVisibleBlock* temp_blocks = (CachedVisibleBlock*)malloc(sizeof(CachedVisibleBlock) * temp_capacity);

    // Iterate through all blocks in chunk
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                BlockType block = world_chunk_get_block(chunk, x, y, z);

                // Skip air blocks
                if (block == BLOCK_AIR) continue;

                // Get world coordinates
                int world_x = chunk->chunk_x * CHUNK_WIDTH + x;
                int world_y = chunk->chunk_y * CHUNK_HEIGHT + y;
                int world_z = chunk->chunk_z * CHUNK_DEPTH + z;

                // Check which faces are exposed (neighbor is air)
                // Only mark a face as exposed if the neighbor exists and is air
                // If neighbor chunk isn't loaded, don't mark face as exposed
                uint8_t exposed_faces = 0;

                // Check +X direction
                if (world_get_block(world, world_x + 1, world_y, world_z) == BLOCK_AIR) {
                    int32_t adj_chunk_x = (world_x + 1) < 0 ? ((world_x + 1) - CHUNK_WIDTH + 1) / CHUNK_WIDTH : (world_x + 1) / CHUNK_WIDTH;
                    Chunk* adj_chunk = world_get_chunk(world, adj_chunk_x, chunk->chunk_y, chunk->chunk_z);
                    if (adj_chunk && adj_chunk->loaded) exposed_faces |= (1 << 0);
                }
                // Check -X direction
                if (world_get_block(world, world_x - 1, world_y, world_z) == BLOCK_AIR) {
                    int32_t adj_chunk_x = (world_x - 1) < 0 ? ((world_x - 1) - CHUNK_WIDTH + 1) / CHUNK_WIDTH : (world_x - 1) / CHUNK_WIDTH;
                    Chunk* adj_chunk = world_get_chunk(world, adj_chunk_x, chunk->chunk_y, chunk->chunk_z);
                    if (adj_chunk && adj_chunk->loaded) exposed_faces |= (1 << 1);
                }
                // Check +Y direction
                if (world_get_block(world, world_x, world_y + 1, world_z) == BLOCK_AIR) {
                    int32_t adj_chunk_y = (world_y + 1) < 0 ? ((world_y + 1) - CHUNK_HEIGHT + 1) / CHUNK_HEIGHT : (world_y + 1) / CHUNK_HEIGHT;
                    Chunk* adj_chunk = world_get_chunk(world, chunk->chunk_x, adj_chunk_y, chunk->chunk_z);
                    if (adj_chunk && adj_chunk->loaded) exposed_faces |= (1 << 2);
                }
                // Check -Y direction
                if (world_get_block(world, world_x, world_y - 1, world_z) == BLOCK_AIR) {
                    int32_t adj_chunk_y = (world_y - 1) < 0 ? ((world_y - 1) - CHUNK_HEIGHT + 1) / CHUNK_HEIGHT : (world_y - 1) / CHUNK_HEIGHT;
                    Chunk* adj_chunk = world_get_chunk(world, chunk->chunk_x, adj_chunk_y, chunk->chunk_z);
                    if (adj_chunk && adj_chunk->loaded) exposed_faces |= (1 << 3);
                }
                // Check +Z direction
                if (world_get_block(world, world_x, world_y, world_z + 1) == BLOCK_AIR) {
                    int32_t adj_chunk_z = (world_z + 1) < 0 ? ((world_z + 1) - CHUNK_DEPTH + 1) / CHUNK_DEPTH : (world_z + 1) / CHUNK_DEPTH;
                    Chunk* adj_chunk = world_get_chunk(world, chunk->chunk_x, chunk->chunk_y, adj_chunk_z);
                    if (adj_chunk && adj_chunk->loaded) exposed_faces |= (1 << 4);
                }
                // Check -Z direction
                if (world_get_block(world, world_x, world_y, world_z - 1) == BLOCK_AIR) {
                    int32_t adj_chunk_z = (world_z - 1) < 0 ? ((world_z - 1) - CHUNK_DEPTH + 1) / CHUNK_DEPTH : (world_z - 1) / CHUNK_DEPTH;
                    Chunk* adj_chunk = world_get_chunk(world, chunk->chunk_x, chunk->chunk_y, adj_chunk_z);
                    if (adj_chunk && adj_chunk->loaded) exposed_faces |= (1 << 5);
                }

                if (exposed_faces != 0) {
                    // Grow temporary array if needed
                    if (temp_count >= temp_capacity) {
                        temp_capacity *= 2;
                        temp_blocks = (CachedVisibleBlock*)realloc(temp_blocks,
                                                                   sizeof(CachedVisibleBlock) * temp_capacity);
                    }

                    // Add to temporary array
                    temp_blocks[temp_count].x = x;
                    temp_blocks[temp_count].y = y;
                    temp_blocks[temp_count].z = z;
                    temp_blocks[temp_count].exposed_faces = exposed_faces;
                    temp_blocks[temp_count].light_level = 0;  // preserved for compatibility

                    // Compute per-face baked lighting (use max of skylight and blocklight at adjacent air block)
                    for (int face = 0; face < 6; face++) {
                        // default to zero light
                        temp_blocks[temp_count].face_light[face] = 0;
                    }

                    // +X
                    if (exposed_faces & (1 << 0)) {
                        int nx = world_x + 1;
                        int ny = world_y;
                        int nz = world_z;
                        uint8_t skyl = world_get_skylight(world, nx, ny, nz);
                        uint8_t blockl = world_get_blocklight(world, nx, ny, nz);
                        temp_blocks[temp_count].face_light[0] = (skyl > blockl) ? skyl : blockl;
                    }
                    // -X
                    if (exposed_faces & (1 << 1)) {
                        int nx = world_x - 1;
                        int ny = world_y;
                        int nz = world_z;
                        uint8_t skyl = world_get_skylight(world, nx, ny, nz);
                        uint8_t blockl = world_get_blocklight(world, nx, ny, nz);
                        temp_blocks[temp_count].face_light[1] = (skyl > blockl) ? skyl : blockl;
                    }
                    // +Y
                    if (exposed_faces & (1 << 2)) {
                        int nx = world_x;
                        int ny = world_y + 1;
                        int nz = world_z;
                        uint8_t skyl = world_get_skylight(world, nx, ny, nz);
                        uint8_t blockl = world_get_blocklight(world, nx, ny, nz);
                        temp_blocks[temp_count].face_light[2] = (skyl > blockl) ? skyl : blockl;
                    }
                    // -Y
                    if (exposed_faces & (1 << 3)) {
                        int nx = world_x;
                        int ny = world_y - 1;
                        int nz = world_z;
                        uint8_t skyl = world_get_skylight(world, nx, ny, nz);
                        uint8_t blockl = world_get_blocklight(world, nx, ny, nz);
                        temp_blocks[temp_count].face_light[3] = (skyl > blockl) ? skyl : blockl;
                    }
                    // +Z
                    if (exposed_faces & (1 << 4)) {
                        int nx = world_x;
                        int ny = world_y;
                        int nz = world_z + 1;
                        uint8_t skyl = world_get_skylight(world, nx, ny, nz);
                        uint8_t blockl = world_get_blocklight(world, nx, ny, nz);
                        temp_blocks[temp_count].face_light[4] = (skyl > blockl) ? skyl : blockl;
                    }
                    // -Z
                    if (exposed_faces & (1 << 5)) {
                        int nx = world_x;
                        int ny = world_y;
                        int nz = world_z - 1;
                        uint8_t skyl = world_get_skylight(world, nx, ny, nz);
                        uint8_t blockl = world_get_blocklight(world, nx, ny, nz);
                        temp_blocks[temp_count].face_light[5] = (skyl > blockl) ? skyl : blockl;
                    }

                    temp_count++;
                }
            }
        }
    }

    // ATOMIC SWAP: Now safely replace the old array with the new one
    // Using double-buffering: build into inactive buffer, then atomically swap active_mesh index
    // This ensures render thread always sees consistent data (no partial updates)

    // Lock mutex during swap to prevent render thread from reading between buffer updates
    // This ensures the render thread never sees an inconsistent state
    pthread_mutex_lock(&chunk->mesh_swap_mutex);

    int current_active = __atomic_load_n(&chunk->active_mesh, __ATOMIC_ACQUIRE);
    int inactive_buffer = 1 - current_active;  // Opposite of currently active buffer

    // Free old data in the inactive buffer if it exists
    if (chunk->visible_blocks[inactive_buffer] != NULL) {
        free(chunk->visible_blocks[inactive_buffer]);
    }

    // Store new mesh into inactive buffer
    chunk->visible_blocks[inactive_buffer] = temp_blocks;
    chunk->visible_count[inactive_buffer] = temp_count;
    chunk->visible_capacity[inactive_buffer] = temp_capacity;

    // ATOMIC SWAP: Use atomic operation with memory barrier
    // This ensures the updated mesh is fully visible before we flip the active buffer switch
    // Render thread will see the new mesh on the next inspection
    __atomic_store_n(&chunk->active_mesh, inactive_buffer, __ATOMIC_RELEASE);

    pthread_mutex_unlock(&chunk->mesh_swap_mutex);
}

// Free the visible blocks cache (both buffers)
void chunk_free_visible_blocks(Chunk* chunk)
{
    // Free both buffers
    for (int i = 0; i < 2; i++) {
        if (chunk->visible_blocks[i] != NULL) {
            free(chunk->visible_blocks[i]);
            chunk->visible_blocks[i] = NULL;
        }
        chunk->visible_count[i] = 0;
        chunk->visible_capacity[i] = 0;
    }
    // NOTE: Don't set meshed=false here - keep rendering old mesh while worker recalculates
    // This prevents flickering when blocks are placed/broken
    // Worker thread will recalculate and update visible_blocks while meshed stays true
}
