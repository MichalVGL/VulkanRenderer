#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in flat uint fragTextureIndex;

/*
layout(push_constant) uniform PushConstants {
    layout(offset = 72) uint textureIndex; // Assuming mat4 (64) + uint64_t (8) are before this
} pushConstants;
*/

// Unbounded runtime array of textures
layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec2 outNormal;    // encoded normal
layout(location = 2) out vec4 outMaterial;  // unused for now

void main()
{
    // albedo from texture
    outAlbedo = texture(globalTextures[nonuniformEXT(fragTextureIndex)], fragUV);

    // octahedral encode normal
    vec3 n = normalize(fragNormal);
    vec2 encoded = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
        encoded = (1.0 - abs(encoded.yx)) * sign(encoded);
    outNormal = encoded * 0.5 + 0.5; // remap to [0,1]

    outMaterial = vec4(0.0);
}