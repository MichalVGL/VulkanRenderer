#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 albedo = texture(gAlbedo, fragUV);
    outColor = albedo; // just output albedo for now, add lighting later
}