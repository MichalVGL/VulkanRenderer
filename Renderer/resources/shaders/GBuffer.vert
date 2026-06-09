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
    mat4 transform;
    uint64_t vertexBufferAddress;
    uint32_t albedoTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t materialTextureIndex;
} push;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out flat uint fragAlbedoIndex;
layout(location = 3) out flat uint fragNormalIndex;
layout(location = 4) out flat uint fragMaterialIndex;

//layout(early_fragment_tests) in;

void main()
{
    //const float alphaThreshold = 0.95f;
    //if(texture(globalTextures[nonuniformEXT(push.albedoTextureIndex)], fragUV).a < alphaThreshold)
        //discard; // alpha test for masking, can be used for foliage, fences, etc.

    // Retrieve the buffer using the address
    VertexBuffer vb = VertexBuffer(push.vertexBufferAddress);
    
    // Pull the exact vertex by index
    Vertex v = vb.vertices[gl_VertexIndex];
    gl_Position = push.transform * vec4(v.position, 1.0);
    
    // Pass properties to fragment shader
    fragNormal = mat3(push.transform) * v.normal; 
    fragUV = v.uv;
    fragAlbedoIndex = push.albedoTextureIndex;
    fragNormalIndex = push.normalTextureIndex;
    fragMaterialIndex = push.materialTextureIndex;  
}