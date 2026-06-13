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
    /*
    //harcoded fullscreen triangle
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragUV = positions[gl_VertexIndex] * 0.5 + 0.5;
    */

    vec3 position = g_vertexPositions[gl_VertexIndex];
    fragLocalPos = position;
    gl_Position = push.projection * push.view * vec4(position, 1.0);
}