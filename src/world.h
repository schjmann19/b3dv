#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"

// Block types
typedef enum {
    BLOCK_AIR = 0,
    BLOCK_STONE = 1,
    BLOCK_DIRT = 2,
    BLOCK_GRASS = 3,
    BLOCK_SAND = 4,
    BLOCK_WOOD = 5
} BlockType;

// Chunk system for infinite worlds
#define CHUNK_WIDTH 32
#define CHUNK_HEIGHT 64
#define CHUNK_DEPTH 32
#define CHUNK_LOAD_DISTANCE 1  // Load chunks this many chunks away from player

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
    bool textures_loaded;
} TextureCache;

// Chunk structure - a 32x64x32 section of the world
typedef struct {
    Block blocks[CHUNK_HEIGHT][CHUNK_DEPTH][CHUNK_WIDTH];
    int32_t chunk_x;  // Chunk coordinates
    int32_t chunk_y;
    int32_t chunk_z;
    bool loaded;      // Whether this chunk is currently in memory
    bool generated;   // Whether terrain has been generated
} Chunk;

// Chunk cache - stores loaded chunks
typedef struct {
    Chunk* chunks;
    int chunk_count;
    int chunk_capacity;
} ChunkCache;

// World structure - infinite world with chunk-based loading
typedef struct {
    ChunkCache chunk_cache;
    TextureCache textures;
    int32_t last_loaded_chunk_x;  // For tracking what needs to be loaded
    int32_t last_loaded_chunk_y;
    int32_t last_loaded_chunk_z;
    char world_name[256];  // Current world name for proper chunk loading
} World;

// Function declarations
World* world_create(void);
void world_free(World* world);
Color world_get_block_color(BlockType type);
Texture2D world_get_block_texture(World* world, BlockType type);
void world_load_textures(World* world);
void world_unload_textures(World* world);
void world_generate_prism(World* world);
void world_system_init(void);
bool world_save(World* world, const char* world_name);
bool world_load(World* world, const char* world_name);
void world_update_chunks(World* world, Vector3 player_pos, Vector3 camera_forward);
Chunk* world_get_chunk(World* world, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);
void world_set_block(World* world, int x, int y, int z, BlockType type);
BlockType world_get_block(World* world, int x, int y, int z);
void world_chunk_set_block(Chunk* chunk, int x, int y, int z, BlockType type);
BlockType world_chunk_get_block(Chunk* chunk, int x, int y, int z);
void world_generate_chunk(Chunk* chunk);
Chunk* world_load_or_create_chunk(World* world, int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);

#endif
