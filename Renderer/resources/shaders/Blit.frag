#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require

layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstants {
    float exposure;
} push;

layout(set = 0, binding = 0) uniform sampler2D gHDR;

layout(location = 0) out vec4 outColor;

vec3 ACESFilmToneMapping(vec3 color);
vec3 ReinhardToneMapping(vec3 color);
vec3 Uncharted2ToneMapping(vec3 color);

void main()
{
    vec3 color = texture(gHDR, fragUV).rgb;

    // Choose one:
    //color = ReinhardToneMapping(color);
    //color = ACESFilmToneMapping(color * push.exposure);
    color = Uncharted2ToneMapping(color);

    outColor = vec4(color, 1.0);
}

vec3 ACESFilmToneMapping(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp(
        (color * (a * color + b)) /
        (color * (c * color + d) + e),
        0.0,
        1.0
    );
}

vec3 ReinhardToneMapping(vec3 color)
{
    return color / (color + vec3(1.0));
}

vec3 Uncharted2ToneMappingCurve(vec3 color)
{
    const float a = 0.15;
    const float b = 0.50;
    const float c = 0.10;
    const float d = 0.20;
    const float e = 0.02;
    const float f = 0.30;

    return ((color * (a * color + c * b) + d * e) /
            (color * (a * color + b) + d * f))
           - e / f;
}

vec3 Uncharted2ToneMapping(vec3 color)
{
    const float W = 11.2; // white point

    vec3 curvedColor = Uncharted2ToneMappingCurve(color);

    float whiteScale =
        1.0 / Uncharted2ToneMappingCurve(vec3(W)).r;

    return clamp(curvedColor * whiteScale, 0.0, 1.0);
}