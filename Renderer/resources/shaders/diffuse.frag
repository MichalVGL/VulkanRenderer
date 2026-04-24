#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(push_constant) uniform PushConstants {
    layout(offset = 72) uint textureIndex; // Assuming mat4 (64) + uint64_t (8) are before this
} pushConstants;

// Unbounded runtime array of textures
layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

layout(location = 0) out vec4 outColor;

void main()
{
    // nonuniformEXT is required when indexing an array with a value that can change per-triangle/pixel
    vec4 texColor = texture(globalTextures[nonuniformEXT(pushConstants.textureIndex)], fragUV);
    
    outColor = vec4(texColor.rgb, 1.0);
}