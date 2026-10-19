struct BaryDeriv
{
    float3 lambda;
    float3 ddx;
    float3 ddy;
};

// https://x.com/Stubbesaurus/status/937994790553227264
float3 decode_oct(float2 e)
{
    float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    float t = max(-v.z, 0);
    v.xy += float2(v.x >= 0 ? -t : t, v.y >= 0 ? -t : t);
    return normalize(v);
}

float2 unpack_r8g8(uint packed)
{
    float2 result;
    result.x = (float((packed >> 0) & 255)) / 127.0F - 1.0F;
    result.y = (float((packed >> 8) & 255)) / 127.0F - 1.0F;

    return result;
}

float4 unpack_r10g10b10a2(uint packed)
{
    float4 result = 0.0F.xxxx;
    result.x = float((packed >> 00) & 1023) / 511.0F - 1.0F;
    result.y = float((packed >> 10) & 1023) / 511.0F - 1.0F;
    result.z = float((packed >> 20) & 1023) / 511.0F - 1.0F;
    result.w = ((packed >> 30) & 1) == 1 ? -1.0F : 1.0F;

    return result;
}

uint load_u8(ByteAddressBuffer data, uint byte_offset)
{
    return (data.Load(byte_offset & ~3u) >> ((byte_offset & 3u) * 8u)) & 0xFFu;
}

float3 quat_rotate_vec3(float3 pt, float4 quat)
{
    return pt + 2.0 * cross(quat.xyz, cross(quat.xyz, pt) + quat.w * pt);
}

float3 transform_vec3(float3 pt, float4 pos_and_scale, float4 quat)
{
    // rotate -> scale -> translate
    // rotation code taken from https://www.geeks3d.com/20141201/how-to-rotate-a-vertex-by-a-quaternion-in-glsl/
    return (pt + 2.0 * cross(quat.xyz, cross(quat.xyz, pt) + quat.w * pt)) * pos_and_scale.w + pos_and_scale.xyz;
}

// https://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/
BaryDeriv calc_full_bary(float4 pt0, float4 pt1, float4 pt2, float2 pixel_ndc, float2 win_size)
{
    BaryDeriv ret;
    float3 inv_w = 1.0 / float3(pt0.w, pt1.w, pt2.w);

    float2 ndc0 = pt0.xy * inv_w.x;
    float2 ndc1 = pt1.xy * inv_w.y;
    float2 ndc2 = pt2.xy * inv_w.z;

    float inv_det = 1.0 / determinant(float2x2(ndc2 - ndc1, ndc0 - ndc1));
    ret.ddx = float3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * inv_det * inv_w;
    ret.ddy = float3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * inv_det * inv_w;

    float ddx_sum = dot(ret.ddx, 1.0.xxx);
    float ddy_sum = dot(ret.ddy, 1.0.xxx);

    float2 delta = pixel_ndc - ndc0;
    float interp_inv_w = inv_w.x + delta.x * ddx_sum + delta.y * ddy_sum;
    float interp_w = 1.0 / interp_inv_w;

    ret.lambda = interp_w * (float3(inv_w.x, 0.0, 0.0) + delta.x * ret.ddx + delta.y * ret.ddy);

    ret.ddx *= 2.0 / win_size.x;
    ret.ddy *= 2.0 / win_size.y;

    ddx_sum *= 2.0 / win_size.x;
    ddy_sum *= 2.0 / win_size.y;

    float w_ddx = 1.0 / (interp_inv_w + ddx_sum);
    float w_ddy = 1.0 / (interp_inv_w + ddy_sum);

    float3 num = ret.lambda * interp_inv_w;
    ret.ddx = w_ddx * (num + ret.ddx) - ret.lambda;
    ret.ddy = w_ddy * (num + ret.ddy) - ret.lambda;
    return ret;
}

float wsum(float v1, float v2, float v3, float3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
}

float2 wsum(float2 v1, float2 v2, float2 v3, float3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
}

float3 wsum(float3 v1, float3 v2, float3 v3, float3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
}

float4 wsum(float4 v1, float4 v2, float4 v3, float3 factors)
{
    return v1 * factors.x + v2 * factors.y + v3 * factors.z;
}

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
// Adapted version from https://github.com/zeux/niagara
bool project_sphere(float3 c, float r, float znear, float P00, float P11, out float4 aabb)
{
    if (-znear - r < c.z)
    {
        return false;
    }

    float3 cr = c * r;
    float czr2 = c.z * c.z - r * r;

    float vx = sqrt(c.x * c.x + czr2);
    float minx = (vx * c.x + cr.z) / (cr.x - vx * c.z);
    float maxx = (cr.z - vx * c.x) / (vx * c.z + cr.x);

    float vy = sqrt(c.y * c.y + czr2);
    float miny = (vy * c.y + cr.z) / (cr.y - vy * c.z);
    float maxy = (cr.z - vy * c.y) / (vy * c.z + cr.y);

    aabb = float4(minx * P00, miny * P11, maxx * P00, maxy * P11);
    aabb = aabb.xwzy * 0.5f + 0.5f;

    return true;
}
