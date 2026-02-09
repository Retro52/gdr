vec3 quat_rotate_vec3(vec3 point, vec4 quat)
{
    return point + 2.0 * cross(quat.xyz, cross(quat.xyz, point) + quat.w * point);
}

vec3 transform_vec3(vec3 point, vec4 pos_and_scale, vec4 quat)
{
    // rotate -> scale -> translate
    // rotation code taken from https://www.geeks3d.com/20141201/how-to-rotate-a-vertex-by-a-quaternion-in-glsl/
    return (point + 2.0 * cross(quat.xyz, cross(quat.xyz, point) + quat.w * point)) * pos_and_scale.w + pos_and_scale.xyz;
}

uint get_bit(uint flags, uint bit)
{
    return (flags >> bit) & 1;
}

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
// Adapted version from https://github.com/zeux/niagara
bool project_sphere(vec3 c, float r, float znear, float P00, float P11, out vec4 aabb)
{
    if (-znear - r < c.z)
    {
        return false;
    }

    vec3 cr = c * r;
    float czr2 = c.z * c.z - r * r;

    float vx = sqrt(c.x * c.x + czr2);
    float minx = (vx * c.x + cr.z) / (cr.x - vx * c.z);
    float maxx = (cr.z - vx * c.x) / (vx * c.z + cr.x);

    float vy = sqrt(c.y * c.y + czr2);
    float miny = (vy * c.y + cr.z) / (cr.y - vy * c.z);
    float maxy = (cr.z - vy * c.y) / (vy * c.z + cr.y);

    aabb = vec4(minx * P00, miny * P11, maxx * P00, maxy * P11);
    aabb = vec4(aabb.x, aabb.w, aabb.z, aabb.y) * vec4(0.5f) + vec4(0.5f);

    return true;
}
