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
