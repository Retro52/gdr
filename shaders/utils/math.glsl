struct BaryDeriv
{
    vec3 lambda;
    vec3 ddx;
    vec3 ddy;
};

// https://x.com/Stubbesaurus/status/937994790553227264
vec3 decode_oct(vec2 e)
{
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    float t = max(-v.z, 0);
    v.xy += vec2(v.x >= 0 ? -t : t, v.y >= 0 ? -t : t);
    return normalize(v);
}

vec2 unpack_r8g8(uint packed)
{
    vec2 result;
    result.x = (float((packed >> 0) & 255)) / 127.0F - 1.0F;
    result.y = (float((packed >> 8) & 255)) / 127.0F - 1.0F;

    return result;
}

vec4 unpack_r10g10b10a2(uint packed)
{
    vec4 result = vec4(0.0F);
    result.x = float((packed >> 00) & 1023) / 511.0F - 1.0F;
    result.y = float((packed >> 10) & 1023) / 511.0F - 1.0F;
    result.z = float((packed >> 20) & 1023) / 511.0F - 1.0F;
    result.w = ((packed >> 30) & 1) == 1 ? -1.0F : 1.0F;

    return result;
}

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

// https://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/
BaryDeriv calc_full_bary(vec4 pt0, vec4 pt1, vec4 pt2, vec2 pixel_ndc, vec2 win_size)
{
    BaryDeriv ret;
    vec3 inv_w = 1.0 / vec3(pt0.w, pt1.w, pt2.w);

    vec2 ndc0 = pt0.xy * inv_w.x;
    vec2 ndc1 = pt1.xy * inv_w.y;
    vec2 ndc2 = pt2.xy * inv_w.z;

    float inv_det = 1.0 / determinant(mat2(ndc2 - ndc1, ndc0 - ndc1));
    ret.ddx = vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * inv_det * inv_w;
    ret.ddy = vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * inv_det * inv_w;

    float ddx_sum = dot(ret.ddx, vec3(1.0));
    float ddy_sum = dot(ret.ddy, vec3(1.0));

    vec2 delta = pixel_ndc - ndc0;
    float interp_inv_w = inv_w.x + delta.x * ddx_sum + delta.y * ddy_sum;
    float interp_w = 1.0 / interp_inv_w;

    ret.lambda = interp_w * (vec3(inv_w.x, 0.0, 0.0) + delta.x * ret.ddx + delta.y * ret.ddy);

    ret.ddx *= 2.0 / win_size.x;
    ret.ddy *= 2.0 / win_size.y;

    ddx_sum *= 2.0 / win_size.x;
    ddy_sum *= 2.0 / win_size.y;

    float w_ddx = 1.0 / (interp_inv_w + ddx_sum);
    float w_ddy = 1.0 / (interp_inv_w + ddy_sum);

    vec3 num = ret.lambda * interp_inv_w;
    ret.ddx = w_ddx * (num + ret.ddx) - ret.lambda;
    ret.ddy = w_ddy * (num + ret.ddy) - ret.lambda;
    return ret;
}

float wsum(float v1, float v2, float v3, vec3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
}

vec2 wsum(vec2 v1, vec2 v2, vec2 v3, vec3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
}

vec3 wsum(vec3 v1, vec3 v2, vec3 v3, vec3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
}

vec4 wsum(vec4 v1, vec4 v2, vec4 v3, vec3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
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
