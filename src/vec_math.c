#include "vec_math.h"
#include <math.h>

Vector3 vec3_normalize(Vector3 v)
{
    float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len == 0) return (Vector3){0, 0, 0};
    return (Vector3){v.x/len, v.y/len, v.z/len};
}

Vector3 vec3_cross(Vector3 a, Vector3 b)
{
    return (Vector3){
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

Vector3 vec3_add(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 vec3_sub(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 vec3_scale(Vector3 v, float scale)
{
    return (Vector3){v.x * scale, v.y * scale, v.z * scale};
}

Vector3 vec3_rotate_y(Vector3 v, float angle)
{
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    return (Vector3){
        v.x * cos_a - v.z * sin_a,
        v.y,
        v.x * sin_a + v.z * cos_a
    };
}
