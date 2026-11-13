#pragma once

#include "utils/math.hlsl"
#include "include/shaders/types.hlsl.h"

void unpack_instance_data(MeshData instance, float4x4 view, float4 translation_scale, float4 rotation, out float4 sphere)
{
    float3 local_center = float3(
        instance.center[0],
        instance.center[1],
        instance.center[2]
    );

    float4 world_center = float4(transform_vec3(local_center, translation_scale, rotation), 1.0F);

    sphere.xyz = mul(view, world_center).xyz;
    sphere.w = abs(float(instance.radius) * translation_scale.w);
}

void unpack_meshlet_data(Meshlet meshlet, float4x4 view, float4 translation_scale, float4 rotation, out float4 sphere, out float4 cone)
{
    float3 local_center = float3(
        meshlet.sphere_center[0],
        meshlet.sphere_center[1],
        meshlet.sphere_center[2]
    );

    float4 world_center = float4(transform_vec3(local_center, translation_scale, rotation), 1.0F);

    sphere.xyz = mul(view, world_center).xyz;
    sphere.w = abs(float(meshlet.sphere_radius) * translation_scale.w);

#ifdef ENABLE_CONE_CULL
    float3 local_cone = float3(
        meshlet.cone_axis[0],
        meshlet.cone_axis[1],
        meshlet.cone_axis[2]
    );

    float4 world_cone = float4(quat_rotate_vec3(local_cone, rotation), 0.0F);

    cone.w = meshlet.cone_cutoff;
    cone.xyz = mul(view, world_cone).xyz;
#endif
}
