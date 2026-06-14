#version 450

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
    vec2 uv = SampleSphericalMap(dir);
    outColor = vec4(texture(sphericalMap, uv).rgb, 1.0);
}