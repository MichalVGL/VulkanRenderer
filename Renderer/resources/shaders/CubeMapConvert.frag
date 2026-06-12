#version 450

/*
layout(location = 0) in vec3 fragLocalPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D equirectangularMap;

layout(push_constant) uniform PushConstants
{
    uint faceIndex;
} push;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

vec3 GetDirectionForFace(uint face, vec2 uv)
{
    vec2 st = uv * 2.0 - 1.0;
    switch (face)
    {
        case 0: return normalize(vec3( 1.0,  st.y, -st.x)); // +X
        case 1: return normalize(vec3(-1.0,  st.y,  st.x)); // -X
        case 2: return normalize(vec3( st.x, -1.0,  st.y)); // +Y
        case 3: return normalize(vec3( st.x,  1.0, -st.y)); // -Y
        case 4: return normalize(vec3( st.x,  st.y,  1.0)); // +Z
        case 5: return normalize(vec3(-st.x,  st.y, -1.0)); // -Z
    }
    return vec3(0.0);
}

vec2 DirToEquirectangular(vec3 dir)
{
    float u = atan(dir.z, dir.x) / TWO_PI + 0.5;
    float v = asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5;
    return vec2(u, v);
}

void main()
{
    //vec3 dir = GetDirectionForFace(push.faceIndex, fragUV);
    //vec2 equiUV = DirToEquirectangular(dir);
    //outColor = texture(equirectangularMap, equiUV);
}

*/

layout(location = 0) in vec3 fragLocalPosition;

layout(set = 0, binding = 0) uniform sampler2D sphericalMap;

layout(location = 0) out vec4 outColor;

const vec2 gInvAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(in vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= gInvAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec3 dir = normalize(fragLocalPosition);
    //dir = vec3(dir.z, dir.y, -dir.x); //convert from OpenGL to Vulkan coordinate system
    vec2 uv = SampleSphericalMap(dir);
    outColor = vec4(texture(sphericalMap, uv).rgb, 1.0);
}