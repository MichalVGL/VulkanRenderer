#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require

layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstants {
    mat4 transform;
    uint64_t vertexBufferAddress;
    uint32_t albedoTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t materialTextureIndex;
} push;

// Unbounded runtime array of textures
layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

void main()
{
    vec4 albedo = texture(globalTextures[nonuniformEXT(push.albedoTextureIndex)], fragUV);

    const float alphaThreshold = 0.95f;
    if (albedo.a < alphaThreshold)
        discard;
}