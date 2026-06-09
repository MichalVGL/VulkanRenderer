#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;

layout(location = 0) out vec4 outColor;

//==================
// Normal Decoding
//==================

vec2 OctWrap(vec2 v)
{
    return (1.0 - abs(v.yx)) * sign(v);
}

vec3 DecodeNormal(vec2 e)
{
    e = e * 2.0 - 1.0;

    vec3 n;
    n.z = 1.0 - abs(e.x) - abs(e.y);
    n.xy = n.z >= 0.0 ? e.xy : OctWrap(e.xy);

    return normalize(n);
}

//==================
// Main
//==================

void main()
{
    //albedo
    vec4 albedo = texture(gAlbedo, fragUV);
    outColor = vec4(albedo.rgb, 1.0);

    //normal
    //vec4 normal = texture(gNormal, fragUV);
    //outColor = vec4(normal.rg, 0.0, 1.0);

    //metalness
    //vec4 albedo = texture(gMaterial, fragUV);
    //outColor = vec4(albedo.b, albedo.b, albedo.b, 1.0);

    //roughness
    //vec4 albedo = texture(gMaterial, fragUV);
    //outColor = vec4(albedo.g, albedo.g, albedo.g, 1.0);
}