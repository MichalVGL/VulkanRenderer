#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragTangent;
layout(location = 2) in vec2 fragUV;

layout(push_constant) uniform PushConstants {
    mat4 transform;
    uint64_t vertexBufferAddress;
    uint32_t albedoTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t materialTextureIndex;
} push;

// Unbounded runtime array of textures
layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

//GBuffer
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
    float denom = abs(n.x) + abs(n.y) + abs(n.z);
    n /= max(denom, 1e-6);
    if(n.z < 0.0)
        n.xy = OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

layout(early_fragment_tests) in;

void main()
{
    vec4 albedo = texture(globalTextures[nonuniformEXT(push.albedoTextureIndex)], fragUV);

    const float alphaThreshold = 0.95f;
    if (albedo.a < alphaThreshold)
        discard;

    outAlbedo = albedo;
    
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = normalize(cross(N, T) * sign(fragTangent.w)); // bitangent sign stored in tangent.w
    
    mat3 TBN = mat3(T, B, N);

    vec3 normalSample = texture(globalTextures[nonuniformEXT(push.normalTextureIndex)], fragUV).xyz;
    normalSample = normalize(normalSample * 2.0 - 1.0); // remap from [0,1] to [-1,1]
    outNormal = EncodeNormal(TBN * normalSample); // octahedral encode

    //outNormal = EncodeNormal(normalize(fragNormal.xyz));

    outMaterial = texture(globalTextures[nonuniformEXT(push.materialTextureIndex)], fragUV);
}
