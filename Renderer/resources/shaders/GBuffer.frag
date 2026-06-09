#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;
layout(location = 2) flat in uint fragAlbedoIndex;
layout(location = 3) flat in uint fragNormalIndex;
layout(location = 4) flat in uint fragMaterialIndex;

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

vec2 OctWrap(vec2 v)
{
    return (1.0 - abs(v.yx)) * sign(v);
}

// Octahedral normal encoding
// based on: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
vec2 EncodeNormal(vec3 n) // octahedral encode
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if(n.z < 0.0)
        n.xy = OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

layout(early_fragment_tests) in;

void main()
{

    vec4 albedo = texture(globalTextures[nonuniformEXT(fragAlbedoIndex)], fragUV);

    if (albedo.a < 0.5)
        discard;

    // albedo from texture
    outAlbedo = texture(globalTextures[nonuniformEXT(fragAlbedoIndex)], fragUV);

    vec3 n = normalize(texture(globalTextures[nonuniformEXT(fragNormalIndex)], fragUV).xyz);
    outNormal = EncodeNormal(n); // octahedral encode

    outMaterial = texture(globalTextures[nonuniformEXT(fragMaterialIndex)], fragUV);
}