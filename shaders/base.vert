#version 450

vec2 positions[3] = vec2[](
vec2(0.0, -0.5),
vec2(0.5, 0.5),
vec2(-0.5, 0.5)
);

vec3 colors_1[3] = vec3[](
vec3(1.0, 0.0, 0.0),
vec3(1.0, 0.0, 0.0),
vec3(1.0, 0.0, 0.0)
);

vec3 colors_2[3] = vec3[](
vec3(0.0, 1.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 1.0, 0.0)
);

layout (location = 0) out vec3 oVertColor;

layout (push_constant) uniform constants
{
    float offset;
} pc;

void main()
{
    oVertColor = pc.offset > 0.0F ? colors_1[gl_VertexIndex] : colors_2[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex] + vec2(pc.offset, 0.0F), pc.offset > 0.0F ? 0.0F : 1.0F, 1.0);
}
