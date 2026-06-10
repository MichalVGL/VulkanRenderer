#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require

struct Vertex {
    vec3 position; // offset 0
    vec3 normal;   // offset 12
    vec2 uv;       // offset 24
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
    mat4 transform; //todo, chang ename to mvp and push model seperatly
    uint64_t vertexBufferAddress;
    uint32_t albedoTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t materialTextureIndex;
} push;

layout(set = 1, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
} sceneData;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;

void main()
{
    // Retrieve the buffer using the address
    VertexBuffer vb = VertexBuffer(push.vertexBufferAddress);
    
    // Pull the exact vertex by index
    Vertex v = vb.vertices[gl_VertexIndex];
    gl_Position = sceneData.viewProj * push.transform * vec4(v.position, 1.0);
    
    // Pass properties to fragment shader
    fragNormal = mat3(sceneData.view) * v.normal; //todo, push model seperatly
    fragUV = v.uv;
}
