#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(push_constant) uniform PushConstants
{
    mat4 transform;
} push;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;

void main()
{
    gl_Position = push.transform * vec4(position, 1.0);
    fragNormal = normal;
    fragUV = uv;
}