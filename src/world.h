#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include "utils.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Forward declare Player so World can reference the active player without including player.h
struct Player;

// Block types
typedef enum {
    BLOCK_AIR = 0,
    BLOCK_STONE = 1,
    BLOCK_DIRT = 2,
    BLOCK_GRASS = 3,
    BLOCK_SAND = 4,
    BLOCK_WOOD = 5,
    BLOCK_BEDROCK = 6,
    BLOCK_GLOWSTONE = 7
} BlockType;

// Block properties (emission and opacity)
typedef struct {
    uint8_t emission; // Light emitted by this block (0-15)
    uint8_t opacity;  // How much light is absorbed (1-15, where 15 = opaque, 0 = transparent)
    uint8_t hardness;
    UNIMPLEMENTED // How long it takes to break this block (0=insta, higher is harder) (unused)
        uint8_t blast_resistance;
    UNIMPLEMENTED // Resistance to explosions (0=none, higher is stronger) (unused)
} BlockProperties;

// Get block properties (emission and opacity)
static inline BlockProperties get_block_properties(BlockType type) {
    switch (type) {
    case BLOCK_GLOWSTONE:
        return (BlockProperties){15, 0}; // Emits 15 light, transparent
    case BLOCK_STONE:
        return (BlockProperties){0, 15}; // No emission, opaque
    case BLOCK_DIRT:
        return (BlockProperties){0, 15}; // No emission, opaque
    case BLOCK_GRASS:
        return (BlockProperties){0, 15}; // No emission, opaque
    case BLOCK_SAND:
        return (BlockProperties){0, 15}; // No emission, opaque
    case BLOCK_WOOD:
        return (BlockProperties){0, 15}; // No emission, opaque
    case BLOCK_BEDROCK:
        return (BlockProperties){0, 15}; // No emission, opaque
    case BLOCK_AIR:
        return (BlockProperties){0, 0}; // No emission, fully transparent
    default:
        return (BlockProperties){0, 0}; // No emission, translucent; nonexistant(?)
    }
}

// Cached visible block entry for mesh caching
typedef struct {
    int x, y, z;           // Local chunk coordinates
    uint8_t exposed_faces; // Bitmask of which faces are exposed (bits 0-5 = faces 0-5)
    uint8_t light_level;   // Sky light level (0-15, like Minecraft)
    uint8_t face_light[6]; // Per-face light levels (0-15) baked at meshing time
} CachedVisibleBlock;

// Merged quad from greedy meshing, represents a rectangular face region
typedef struct {
    int x, y, z;    // World position of one corner of the quad
    int w, h;       // Width and height in blocks
    int face;       // Face direction (0-5)
    uint8_t light;  // Light level for this quad
    BlockType type; // Block type (for color/texture)
} MergedQuad;

// Chunk mesh data, holds merged quads per face direction
typedef struct {
    MergedQuad *quads[6]; // Merged quads for each face direction
    int quad_count[6];    // Number of quads per face
    int quad_capacity[6]; // Allocated capacity per face
} MergedMesh;

// Chunk system for infinite worlds
#define CHUNK_WIDTH 32
#define CHUNK_HEIGHT 64
#define CHUNK_DEPTH 32

// World height limits - prevents unloaded chunk light leaks
#define WORLD_Y_MIN 0
#define WORLD_Y_MAX 500

// Block structure
typedef struct {
    BlockType type;
} Block;

// Texture cache for block types
typedef struct {
    Texture2D grass_texture;
    Texture2D dirt_texture;
    Texture2D stone_texture;
    Texture2D sand_texture;
    Texture2D wood_texture;
    Texture2D bedrock_texture;
    bool textures_loaded;
} TextureCache;

// Chunk structure - a 32x64x32 section of the world
typedef struct {
    Block blocks[CHUNK_HEIGHT][CHUNK_DEPTH][CHUNK_WIDTH];
    // Double-buffered lighting to avoid tearing when worker updates lighting data
    uint8_t skylight[2][CHUNK_HEIGHT][CHUNK_DEPTH][CHUNK_WIDTH];   // Skylight levels (0-15) for each block
    uint8_t blocklight[2][CHUNK_HEIGHT][CHUNK_DEPTH][CHUNK_WIDTH]; // Blocklight levels (0-15) for each block
    volatile int active_light_buffer;                              // Index of the active lighting buffer (0 or 1)
    pthread_mutex_t light_swap_mutex;                              // Protects lighting buffer swaps

    int32_t chunk_x; // Chunk coordinates
    int32_t chunk_y;
    int32_t chunk_z;
    bool loaded;           // Whether this chunk is currently in memory
    bool generated;        // Whether terrain has been generated
    bool modified;         // Whether this chunk has unsaved changes
    bool needs_relighting; // Whether lighting needs recalculation (on block change or load)
    bool lighting_dirty;   // Whether only a sub-region of this chunk needs relighting
    int dirty_min_x;       // Inclusive lighting dirty region bounds
    int dirty_max_x;
    int dirty_min_y;
    int dirty_max_y;
    int dirty_min_z;
    int dirty_max_z;
    bool meshed;               // Whether visible blocks have been cached
    bool pending_save;         // Whether this chunk is queued to be saved asynchronously
    bool pending_unload;       // Whether this chunk is scheduled for unload after save completes
    volatile int in_use_count; // Worker jobs currently processing this chunk
    // Double-buffered mesh: two buffers so render thread always has valid data
    CachedVisibleBlock *visible_blocks[2]; // Pre-computed list of blocks with exposed faces (ping-pong buffers)
    int visible_count[2];                  // Number of blocks in each buffer
    int visible_capacity[2];               // Allocated capacity for each buffer
    volatile int active_mesh;              // Index of which buffer is currently being rendered (0 or 1), volatile for inter-thread visibility
    // Greedy meshed geometry - merged quads instead of individual blocks
    MergedMesh *merged_mesh[2];      // Double-buffered merged quads (0 or 1)
    volatile int active_merged_mesh; // Which merged mesh buffer is active
    pthread_mutex_t mesh_swap_mutex; // Protects mesh swap to ensure atomicity
    pthread_mutex_t mutex;           // Protects this chunk during worker processing
} Chunk;

// Hash table entry for chunk lookup (Issue #1: spatial hash for chunk lookup)
typedef struct {
    int32_t chunk_x;
    int32_t chunk_y;
    int32_t chunk_z;
    Chunk *chunk;
} ChunkHashEntry;

// Chunk cache - stores loaded chunks with spatial hash for O(1) lookup
typedef struct {
    Chunk *chunks;
    int chunk_count;
    int chunk_capacity;
    // Hash table for O(1) chunk lookup by coordinates (Issue #1)
    ChunkHashEntry *hash_table;
    int hash_capacity;
} ChunkCache;

// Worker job types: _LIGHTING_AND_MESH or _SAVE_CHUNK
typedef enum {
    WORKER_JOB_LIGHTING_AND_MESH, // Recalculate lighting and/or mesh for a chunk
    WORKER_JOB_SAVE_CHUNK         // Save chunk to disk (async)
} WorkerJobType;

// Worker job - stores chunk coordinates and job type to avoid pointer invalidation
typedef struct {
    int32_t chunk_x;
    int32_t chunk_y;
    int32_t chunk_z;
    WorkerJobType type;
} WorkerJob;

// Worker thread job queue
typedef struct {
    WorkerJob *queue; // Array of chunk coordinates, not pointers
    int count;
    int capacity;
    int jobs_in_progress; // Number of jobs currently being processed by worker
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool shutdown;
} WorkerQueue;

// World structure - infinite world with chunk-based loading
typedef struct {
    ChunkCache chunk_cache;
    TextureCache textures;
    int32_t last_loaded_chunk_x; // For tracking what needs to be loaded
    int32_t last_loaded_chunk_y;
    int32_t last_loaded_chunk_z;
    char world_name[256];               // Current world name for proper chunk loading
    Vector3 last_player_position;       // Last known player position for saving/loading
    Vector3 last_chunk_update_position; // Last position used for chunk load/unload updates
    Vector3 last_chunk_update_forward;  // Last camera forward used for chunk load/unload updates
    uint64_t seed;                      // World seed for reproducible terrain generation
    bool compress_chunk_files;          // Whether this world's chunk files should be compressed
    WorkerQueue worker_queue;           // Queue of chunks to process
    pthread_t worker_thread;            // Worker thread handle
    bool worker_running;                // Whether worker thread is active
    pthread_mutex_t cache_mutex;        // Protects chunk_cache array from realloc while worker accesses it
    // Pointer to the active player when in-game (used for saving player data)
    void *current_player;
    // Cached player nickname (from players.toml); used for chat display
    char player_nickname[64];
} World;

// Function declarations

// Create a new world
World *world_create(void);

// Free world
void world_free(World *world);

// Get color for a block type (obsolete/fallback)
Color world_get_block_color(BlockType type);

// Get texture for a block type (use this one instead of world_get_block_color)
Texture2D world_get_block_texture(World *world, BlockType type);

void world_load_textures(World *world);
void world_unload_textures(World *world);
void world_generate_prism(World *world);
void world_system_init(void);
bool world_save(World *world, const char *world_name);
bool world_load(World *world, const char *world_name);
bool world_save_chunk(Chunk *chunk, const char *world_name, bool allow_compression); // Save a single chunk to disk
void world_update_chunks(World *world, Vector3 player_pos, Vector3 camera_forward, float render_distance_blocks);
Chunk *world_get_chunk(World *world, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);
void world_set_block(World *world, int x, int y, int z, BlockType type);
BlockType world_get_block(World *world, int x, int y, int z);
void world_chunk_set_block(Chunk *chunk, int x, int y, int z, BlockType type);
BlockType world_chunk_get_block(Chunk *chunk, int x, int y, int z);
void world_generate_chunk(Chunk *chunk, uint64_t seed);
Chunk *world_load_or_create_chunk(World *world, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);
void chunk_cache_visible_blocks(Chunk *chunk, World *world);                                                                                 // Pre-compute list of visible blocks
void chunk_update_visible_blocks_region(Chunk *chunk, World *world, int local_x, int local_y, int local_z, int radius);                      // (Issue #2) Update only affected region
void chunk_free_visible_blocks(Chunk *chunk);                                                                                                // Clean up visible blocks cache
void calculate_chunk_skylight(Chunk *chunk, World *world, int target_buffer, bool dirty_region, int min_x, int max_x, int min_z, int max_z); // Calculate proper skylight levels for chunk
uint8_t world_get_skylight(World *world, int x, int y, int z);                                                                               // Get skylight level at block position
void calculate_chunk_blocklight(Chunk *chunk, World *world, int target_buffer);                                                              // Calculate blocklight from emitting blocks
uint8_t world_get_blocklight(World *world, int x, int y, int z);                                                                             // Get blocklight level at block position
void worker_queue_chunk(World *world, Chunk *chunk);                                                                                         // Add chunk to worker queue for lighting/meshing
void worker_queue_chunk_save(World *world, Chunk *chunk);                                                                                    // Add chunk to worker queue for saving
void worker_flush_queue(World *world);                                                                                                       // Wait for all worker queue jobs to complete
void worker_shutdown(World *world);                                                                                                          // Cleanly shut down worker thread
void worker_init(World *world);                                                                                                              // Initialize worker thread system
// Apply saved player data from world players file into a runtime Player instance
bool world_apply_players_to(World *world, void *player);

#endif
