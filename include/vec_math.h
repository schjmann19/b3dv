#ifndef VEC_MATH_H
#define VEC_MATH_H

#include "raylib.h"

Vector3 vec3_normalize(Vector3 v);
Vector3 vec3_cross(Vector3 a, Vector3 b);
Vector3 vec3_add(Vector3 a, Vector3 b);
Vector3 vec3_sub(Vector3 a, Vector3 b);
Vector3 vec3_scale(Vector3 v, float scale);
Vector3 vec3_rotate_y(Vector3 v, float angle);

#endif
