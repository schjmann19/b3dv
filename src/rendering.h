#ifndef RENDERING_H
#define RENDERING_H

#include "raylib.h"
#include "world.h"

bool is_block_occluded(World* world, int x, int y, int z);
bool has_visible_face(World* world, int x, int y, int z, Vector3 block_pos, Vector3 cam_pos);
void draw_cube_faces(Vector3 pos, float size, Color color, Vector3 cam_pos, Color wire_color, World* world, int block_x, int block_y, int block_z);
bool is_block_visible(Vector3 block_pos, Vector3 cam_pos, Vector3 cam_forward,
                      Vector3 cam_right, Vector3 cam_up, float render_distance,
                      float fovy, float aspect);

#endif
