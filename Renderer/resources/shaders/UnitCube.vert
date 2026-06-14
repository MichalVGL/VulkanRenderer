#version 450

const vec3 g_vertexPositions[36] = vec3[]
(
    // +X
    vec3(1, 1, -1), vec3(1, -1, -1), vec3(1, -1, 1),
    vec3(1, 1, -1), vec3(1, -1, 1), vec3(1, 1, 1),

    // -X
    vec3(-1, -1, -1), vec3(-1, 1, -1), vec3(-1, 1, 1),
    vec3(-1, -1, -1), vec3(-1, 1, 1), vec3(-1, -1, 1),

    // +Y
    vec3(-1, 1, -1), vec3(1, 1, -1), vec3(1, 1, 1),
    vec3(-1, 1, -1), vec3(1, 1, 1), vec3(-1, 1, 1),

    // -Y
    vec3(-1, -1, 1), vec3(1, -1, 1), vec3(1, -1, -1), 
    vec3(-1, -1, 1), vec3(1, -1, -1), vec3(-1, -1, -1), 
    
    // +Z
    vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
    vec3(1, -1, 1), vec3(-1, 1, 1), vec3(1, 1, 1),  

    // -Z
    vec3(-1, -1, -1), vec3(1, -1, -1), vec3(1, 1, -1),
    vec3(-1, -1, -1), vec3(1, 1, -1), vec3(-1, 1, -1)
);

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
} push;

layout(location = 0) out vec3 fragLocalPos;

void main()
{
    vec3 position = g_vertexPositions[gl_VertexIndex];
    fragLocalPos = position;
    gl_Position = push.projection * push.view * vec4(position, 1.0);
}