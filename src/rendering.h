#ifndef RENDERING_H
#define RENDERING_H

#include "raylib.h"
#include "world.h"

bool is_block_occluded(World* world, int x, int y, int z);
bool has_visible_face(World* world, int x, int y, int z, Vector3 block_pos, Vector3 cam_pos);
void draw_cube_faces(Vector3 pos, float size, Color color, Vector3 cam_pos, Color wire_color, World* world, int block_x, int block_y, int block_z, BlockType block_type);
bool is_block_visible(Vector3 block_pos, Vector3 cam_pos, Vector3 cam_forward,
                      Vector3 cam_right, Vector3 cam_up, float render_distance,
                      float fovy, float aspect);

// Lighting functions
float get_block_light_level(World* world, int x, int y, int z);
Color apply_face_lighting(Color base_color, int face_index, World* world, int neighbor_x, int neighbor_y, int neighbor_z);

// Raycasting
bool raycast_block(World* world, Camera3D camera, float max_distance,
                   int* out_block_x, int* out_block_y, int* out_block_z,
                   int* out_adjacent_x, int* out_adjacent_y, int* out_adjacent_z);

#endif
